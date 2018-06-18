/*
 * Spin to C/C++ translator
 * Copyright 2016-2018 Total Spectrum Software Inc.
 * 
 * +--------------------------------------------------------------------
 * ¦  TERMS OF USE: MIT License
 * +--------------------------------------------------------------------
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * +--------------------------------------------------------------------
 */
#include "spinc.h"
#include "outasm.h"

// used for converting Spin relative addresses to absolute addresses
// (only needed for OUTPUT_COGSPIN)
static int fixup_number = 0;
static int pending_fixup = 0;

// flags for what has been output so far
static int inDat;
static int inCon;
static int didOrg;
static int lmmMode;

static void
doPrintOperand(struct flexbuf *fb, Operand *reg, int useimm, enum OperandEffect effect)
{
    char temp[128];
    
    if (!reg) {
        ERROR(NULL, "internal error bad operand");
        flexbuf_addstr(fb, "???");
        return;
    }
    if (effect != OPEFFECT_NONE) {
        if (gl_p2) {
            if (reg->kind != REG_HW) {
                ERROR(NULL, "operand effect on wrong register");
            }
        } else {
            ERROR(NULL, "illegal operand effect");
        }
    }
    switch (reg->kind) {
    case IMM_INT:
        if (reg->val >= 0 && reg->val < 512) {
            flexbuf_addstr(fb, "#");
            if (reg->name && reg->name[0]) {
                flexbuf_addstr(fb, reg->name);
            } else {
                sprintf(temp, "%d", (int)(int32_t)reg->val);
                flexbuf_addstr(fb, temp);
            }
        } else if (gl_p2) {
            flexbuf_addstr(fb, "##");
            if (reg->name && reg->name[0]) {
                flexbuf_addstr(fb, reg->name);
            } else {
                sprintf(temp, "%d", (int)(int32_t)reg->val);
                flexbuf_addstr(fb, temp);
            }            
        } else {
            // the immediate actually got processed as a register
            flexbuf_addstr(fb, reg->name);
        }
        break;
    case BYTE_REF:
    case WORD_REF:
    case LONG_REF:
        ERROR(NULL, "Internal error: tried to use memory directly");
        break;
    case IMM_HUB_LABEL:
        if (gl_p2 && useimm) {
            flexbuf_addstr(fb, "#@");
        }
        flexbuf_addstr(fb, reg->name);
        break;
    case IMM_COG_LABEL:
        if (useimm) {
            flexbuf_addstr(fb, "#");
        }
        /* fall through */
    default:
        if (effect == OPEFFECT_PREINC) {
            flexbuf_printf(fb, "++");
        } else if (effect == OPEFFECT_PREDEC) {
            flexbuf_printf(fb, "--");
        }
        flexbuf_addstr(fb, reg->name);
        if (effect == OPEFFECT_POSTINC) {
            flexbuf_printf(fb, "++");
        } else if (effect == OPEFFECT_POSTDEC) {
            flexbuf_printf(fb, "--");
        }
        break;
    }
}

static void
PrintOperandSrc(struct flexbuf *fb, Operand *reg, enum OperandEffect effect)
{
    doPrintOperand(fb, reg, 1, effect);
}

static void
PrintOperand(struct flexbuf *fb, Operand *reg)
{
    doPrintOperand(fb, reg, 0, OPEFFECT_NONE);
}

void
PrintOperandAsValue(struct flexbuf *fb, Operand *reg)
{
    Operand *indirect;
    
    switch (reg->kind) {
    case IMM_INT:
        flexbuf_printf(fb, "%d", (int)(int32_t)reg->val);
        break;
    case IMM_HUB_LABEL:
    case STRING_DEF:
        if (gl_p2) {
            flexbuf_addstr(fb, "@");
        } else if (gl_output == OUTPUT_COGSPIN) {
            // record fixup info
            if (fixup_number > 0) {
                flexbuf_printf(fb, "( (@__fixup_%d - 4) << 16) + @", fixup_number);
            } else {
                flexbuf_addstr(fb, "@");
            }
            fixup_number++;
            pending_fixup = fixup_number;
        } else {
            flexbuf_addstr(fb, "@@@");
        }
        // fall through
    case IMM_COG_LABEL:
        flexbuf_addstr(fb, reg->name);
        break;
    case IMM_STRING:
        flexbuf_addchar(fb, '"');
        flexbuf_addstr(fb, reg->name);
        flexbuf_addchar(fb, '"');
        break;
    case REG_HUBPTR:
    case REG_COGPTR:
        indirect = (Operand *)reg->val;
        flexbuf_addstr(fb, indirect->name);
        break;
    default:
        PrintOperand(fb, reg);
        break;
    }
}

static void
PrintCond(struct flexbuf *fb, IRCond cond)
{
    switch (cond) {
    case COND_TRUE:
      break;
    case COND_EQ:
      flexbuf_addstr(fb, " if_e");
      break;
    case COND_NE:
      flexbuf_addstr(fb, " if_ne");
      break;
    case COND_LT:
      flexbuf_addstr(fb, " if_b");
      break;
    case COND_GE:
      flexbuf_addstr(fb, " if_ae");
      break;
    case COND_GT:
      flexbuf_addstr(fb, " if_a");
      break;
    case COND_LE:
      flexbuf_addstr(fb, " if_be");
      break;
    case COND_C:
      flexbuf_addstr(fb, " if_c");
      break;
    case COND_NC:
      flexbuf_addstr(fb, " if_nc");
      break;
    default:
      flexbuf_addstr(fb, " if_??");
      break;
    }
    flexbuf_addchar(fb, '\t');
}

#define MAX_BYTES_ON_LINE 16

static void
OutputBlob(Flexbuf *fb, Operand *label, Operand *op)
{
    Flexbuf *databuf;
    Flexbuf *relocbuf;
    uint8_t *data;
    int len;
    int addr;
    Reloc *nextreloc;
    int relocs;
    uint32_t runlen;
    int lastdata;
    
    if (op->kind != IMM_BINARY) {
        ERROR(NULL, "Internal: bad binary blob");
        return;
    }
    if (gl_p2) {
        flexbuf_printf(fb, "\talignl\n"); // ensure long alignment
    } else {
        flexbuf_printf(fb, "\tlong\n"); // ensure long alignment
    }
    flexbuf_printf(fb, label->name);
    flexbuf_printf(fb, "\n");
    databuf = (Flexbuf *)op->name;
    relocbuf = (Flexbuf *)op->val;
    if (relocbuf) {
        relocs = flexbuf_curlen(relocbuf) / sizeof(Reloc);
        nextreloc = (Reloc *)flexbuf_peek(relocbuf);
    } else {
        relocs = 0;
        nextreloc = NULL;
    }
    len = flexbuf_curlen(databuf);
    // make sure it is a multiple of 4
    while ( 0 != (len & 3) ) {
        flexbuf_addchar(databuf, 0);
        len = flexbuf_curlen(databuf);
    }
    data = (uint8_t *)flexbuf_peek(databuf);
    addr = 0;
    while (addr < len) {
        // figure out how many bytes we can output
        int bytesPending = len - addr;
        int bytesToReloc;

    again:
        if (relocs > 0) {
            bytesToReloc = nextreloc->off - addr;
            if (bytesToReloc == 0) {
                intptr_t offset;
                // we have to output a relocation or debug entry now
                if (nextreloc->kind == RELOC_KIND_LONG) {
                    if (bytesPending < 4) {
                        ERROR(NULL, "internal error: not enough space for reloc");
                        return;
                    }
                    
                    flexbuf_printf(fb, "\tlong\t");
                    offset = nextreloc->val;
                    if (offset == 0) {
                        flexbuf_printf(fb, "@@@%s\n", label->name);
                    } else if (offset > 0) {
                        flexbuf_printf(fb, "@@@%s + %d\n", label->name, offset);
                    } else {
                        flexbuf_printf(fb, "@@@%s - %d\n", label->name, -offset);
                    }
                    data += 4;
                    addr += 4;
                    nextreloc++;
                    --relocs;
                    continue;
                }
                if (nextreloc->kind == RELOC_KIND_DEBUG) {
                    LineInfo *info = (LineInfo *)nextreloc->val;
                    if (info && info->linedata) {
                        flexbuf_printf(fb, "'-' %s", info->linedata);
                    }
                    nextreloc++;
                    --relocs;
                    goto again;
                }
                ERROR(NULL, "internal error: bad reloc kind %d", nextreloc->kind);
                return;
            }
            if (bytesPending > bytesToReloc) {
                bytesPending = bytesToReloc;
            }
        }

        /* if we have more than 7 bytes pending, look for runs of data */
        if (bytesPending > MAX_BYTES_ON_LINE) {
            /* check for a run of data */
            runlen = 0;
            lastdata = data[0];
            while (data[runlen] == lastdata && addr < len && runlen < bytesPending) {
                runlen++;
            }
            if (runlen > 4) {
                /* output as long as we can */
                if (0 == (runlen & 3)) {
                    flexbuf_printf(fb, "\tlong\t$%08x[%d]\n", lastdata, runlen/4);
                } else {
                    flexbuf_printf(fb, "\tbyte\t$%02x[%d]\n", lastdata, runlen);
                }
                addr += runlen;
                data += runlen;
                continue;
            }
        }
        /* try to chunk into longs if we can */
        if (bytesPending > MAX_BYTES_ON_LINE) {
            bytesPending = MAX_BYTES_ON_LINE;
        }
        flexbuf_printf(fb, "\tbyte\t$%02x", data[0]);
        data++;
        addr++;
        bytesPending--;
        while (bytesPending > 0) {
            flexbuf_printf(fb, ", $%02x", data[0]);
            data++; addr++; --bytesPending;
        }
        flexbuf_printf(fb,"\n");
    }
}

/* find string for opcode */
static const char *
StringFor(IROpcode opc)
{
    switch(opc) {
    case OPC_STRING:
    case OPC_BYTE:
        return "byte";
    case OPC_LONG:
        return "long";
    case OPC_WORD:
        return "word";
    case OPC_WORD1:
        return "word 1 |";
    default:
        ERROR(NULL, "internal error, bad StringFor call");
        return "???";
    }
}

//
// emit public methods for Spin
// this is needed if we want to assemble the .pasm output with
// bstc or some similar compiler
//
void
EmitSpinMethods(struct flexbuf *fb, Module *P)
{
    if (!P) return;
    if (gl_output == OUTPUT_COGSPIN) {
        int varlen = P->varsize;
        varlen = (varlen + 3) & ~3; // round up to long boundary
        if (varlen < 1) varlen = 1;
        Function *f;
        
        flexbuf_addstr(fb, "VAR\n");
        flexbuf_addstr(fb, "  long __mbox[__MBOX_SIZE]   ' mailbox for communicating with remote COG\n");
        flexbuf_printf(fb, "  long __objmem[%d]          ' space for hub data in COG code\n", varlen / 4);
        flexbuf_addstr(fb, "  long __stack[__STACK_SIZE] ' stack for new COG\n");
        flexbuf_addstr(fb, "  byte __cognum              ' 1 + the ID of the running COG (0 if nothing running)\n\n");

        flexbuf_addstr(fb, "'' Code to start the object running in its own COG\n");
        flexbuf_addstr(fb, "'' This must always be called before any other methods\n");
        flexbuf_addstr(fb, "PUB __coginit(id)\n");
        flexbuf_addstr(fb, "  if (__cognum == 0) ' if the cog isn't running yet\n");
        flexbuf_addstr(fb, "    __fixup_addresses\n");
        flexbuf_addstr(fb, "    longfill(@__mbox, 0, __MBOX_SIZE)\n");
        if (gl_p2) {
            flexbuf_addstr(fb, "    __mbox[1] := @entry\n");
        } else {
            flexbuf_addstr(fb, "    __mbox[1] := @pasm__init - @entry\n");
        }
        flexbuf_addstr(fb, "    __mbox[2] := @__objmem\n");
        flexbuf_addstr(fb, "    __mbox[3] := @__stack\n");
        flexbuf_addstr(fb, "    if (id < 0)\n");
        flexbuf_addstr(fb, "      id := cognew(@entry, @__mbox)\n");
        flexbuf_addstr(fb, "    else\n");
        flexbuf_addstr(fb, "      coginit(id, @entry, @__mbox) ' actually start the cog\n");
        flexbuf_addstr(fb, "    __cognum := id + 1\n");
        flexbuf_addstr(fb, "  return id\n\n");

        flexbuf_addstr(fb, "PUB __cognew\n");
        flexbuf_addstr(fb, "  return __coginit(-1)\n\n");
                       
        flexbuf_addstr(fb, "'' Code to stop the remote COG\n");
        flexbuf_addstr(fb, "PUB __cogstop\n");
        flexbuf_addstr(fb, "  if __cognum\n");
        flexbuf_addstr(fb, "    __lock  ' wait until everyone else is finished\n");
        flexbuf_addstr(fb, "    cogstop(__cognum~ - 1)\n");
	flexbuf_addstr(fb, "    __mbox[0] := 0\n");
	flexbuf_addstr(fb, "    __cognum := 0\n\n");
        
        flexbuf_addstr(fb, "'' Code to lock access to the PASM COG\n");
        flexbuf_addstr(fb, "'' The idea here is that (in theory) multiple Spin bytecode threads might\n");
        flexbuf_addstr(fb, "'' want access to the PASM COG, so this lock mackes sure they don't step on each other.\n");
        flexbuf_addstr(fb, "'' This method also makes sure the remote COG is idle and ready to receive commands.\n");
        flexbuf_addstr(fb, "PRI __lock\n");
        flexbuf_addstr(fb, "  repeat\n");
        flexbuf_addstr(fb, "    repeat until __mbox[0] == 0   ' wait until no other Spin code is using remote\n");
        flexbuf_addstr(fb, "    __mbox[0] := __cognum         ' try to claim it\n");
        flexbuf_addstr(fb, "  until __mbox[0] == __cognum     ' make sure we really did get it\n\n");
        flexbuf_addstr(fb, "  repeat until __mbox[1] == 0     ' now wait for the COG itself to be idle\n\n");

        flexbuf_addstr(fb, "'' Code to release access to the PASM COG\n");
        flexbuf_addstr(fb, "PRI __unlock\n");
        flexbuf_addstr(fb, "  __mbox[0] := 0\n\n");

        flexbuf_addstr(fb, "'' Check to see if the PASM COG is busy (still working on something)\n");
        flexbuf_addstr(fb, "PUB __busy\n");
        flexbuf_addstr(fb, "  return __mbox[1] <> 0\n\n");

        flexbuf_addstr(fb, "'' Code to send a message to the remote COG asking it to perform a method\n");
        flexbuf_addstr(fb, "'' func is the PASM entrypoint of the method to perform\n");
        flexbuf_addstr(fb, "'' if getresult is nonzero then we wait for the remote COG to answer us with a result\n");
        flexbuf_addstr(fb, "'' if getresult is 0 then we continue without waiting (the remote COG runs in parallel\n");
        flexbuf_addstr(fb, "'' We must always call __lock before this, and set up the parameters starting in __mbox[2]\n");
        flexbuf_addstr(fb, "PRI __invoke(func, getresult) : r\n");
        flexbuf_addstr(fb, "  __mbox[1] := func - @entry     ' set the function to perform (NB: this is a HUB address)\n");
        flexbuf_addstr(fb, "  if getresult                   ' if we should wait for an answer\n");
        flexbuf_addstr(fb, "    repeat until __mbox[1] == 0  ' wait for remote COG to be idle\n");
        flexbuf_addstr(fb, "    r := __mbox[2]               ' pick up remote COG result\n");
        flexbuf_addstr(fb, "  __unlock                       ' release to other COGs\n");
        flexbuf_addstr(fb, "  return r\n\n");

        flexbuf_addstr(fb, "'' Code to convert Spin relative addresses to absolute addresses\n");
        flexbuf_addstr(fb, "'' The PASM code contains some absolute pointers internally; but the\n");
        flexbuf_addstr(fb, "'' regular Spin compiler cannot emit these (bstc and fastspin can, with the\n");
        flexbuf_addstr(fb, "'' @@@ operator, but we don't want to rely on having those compilers).\n");
        flexbuf_addstr(fb, "'' So the compiler inserts a chain of fixups, with each entry having the Spin\n");
        flexbuf_addstr(fb, "'' relative address in the low word, and a pointer to the next fixup in the high word.\n");
        flexbuf_addstr(fb, "'' This code follows that chain and adjusts the relative addresses to absolute ones.\n");
        
        flexbuf_addstr(fb, "PRI __fixup_addresses | ptr, nextptr, temp\n");
        flexbuf_addstr(fb, "  ptr := __fixup_ptr[0]\n");
        flexbuf_addstr(fb, "  repeat while (ptr)      ' the fixup chain is terminated with a 0 pointer\n");
        flexbuf_addstr(fb, "    ptr := @@ptr          ' point to next fixup\n");
        flexbuf_addstr(fb, "    temp := long[ptr]     ' get the data\n");
        flexbuf_addstr(fb, "    nextptr := temp >> 16 ' high 16 bits contains link to next fixup\n");
        flexbuf_addstr(fb, "    temp := temp & $ffff  ' low 16 bits contains real pointer\n");
        flexbuf_addstr(fb, "    long[ptr] := @@temp   ' replace fixup data with real pointer\n");
        flexbuf_addstr(fb, "    ptr := nextptr\n");
        flexbuf_addstr(fb, "  __fixup_ptr[0] := 0 ' mark fixups as done\n\n");

        flexbuf_addstr(fb, "'--------------------------------------------------\n");
        flexbuf_addstr(fb, "' Stub functions to perform remote calls to the COG\n");
        flexbuf_addstr(fb, "'--------------------------------------------------\n\n");
        
        // now we have to create the stub functions
        for (f = P->functions; f; f = f->next) {
            if (f->is_public) {
                AST *list = f->params;
                AST *ast;
                int paramnum = 2;
                int needcomma = 0;
                int synchronous;
                int i;
                flexbuf_printf(fb, "PUB %s", f->name);
                if (list) {
                    flexbuf_addstr(fb, "(");
                    while (list) {
                        ast = list->left;
                        if (needcomma) {
                            flexbuf_addstr(fb, ", ");
                        }
                        flexbuf_addstr(fb, ast->d.string);
                        needcomma = 1;
                        list = list->right;
                    }
                    flexbuf_addstr(fb, ")");
                }
                if (f->numresults > 1) {
                    flexbuf_addstr(fb, " : r0");
                    for (i = 1; i < f->numresults; i++) {
                        flexbuf_printf(fb, ", r%d", i);
                    }
                }
                flexbuf_addstr(fb, "\n");
                flexbuf_addstr(fb, "  __lock\n");
                list = f->params;
                while (list) {
                    flexbuf_printf(fb, "  __mbox[%d] := %s\n", paramnum, list->left->d.string);
                    list = list->right;
                    paramnum++;
                }
                // if there's a result from the function, make this call
                // synchronous
                if (f->rettype && f->rettype->kind == AST_VOIDTYPE)
                    synchronous = 0;
                else
                    synchronous = 1;
                if (f->numresults < 2) {
                    flexbuf_printf(fb, "  return __invoke(@pasm_%s, %d)\n\n", f->name, synchronous);
                } else {
                    // synchronous call, fetch all results
                    flexbuf_printf(fb, "  __mbox[1] := @pasm_%s - @entry\n", f->name);
                    flexbuf_printf(fb, "  repeat until __mbox[1] == 0\n");
                    for (i = 0; i < f->numresults; i++) {
                        flexbuf_printf(fb, "  r%d := __mbox[%d]\n", i, 2+i);
                    }
                    flexbuf_printf(fb, "  __unlock\n\n");
                }
            }
        }
        flexbuf_addstr(fb, "'--------------------------------------------------\n");
        flexbuf_addstr(fb, "' The converted object (Spin translated to PASM)\n");
        flexbuf_addstr(fb, "' This is the code that will run in the remote COG\n");
        flexbuf_addstr(fb, "'--------------------------------------------------\n\n");

    } else {
        flexbuf_addstr(fb, "PUB main\n");
        flexbuf_addstr(fb, "  coginit(0, @entry, 0)\n");
    }        
}

// LMM jumps +- this amount are turned into add/sub of the pc
// pick a conservative value
// 127 would be the absolute maximum here
#define MAX_REL_JUMP_OFFSET 100

/* convert IR list into assembly language */
static int didPub = 0;

void
DoAssembleIR(struct flexbuf *fb, IR *ir, Module *P)
{
    const char *str;
    if (ir->opc == OPC_COMMENT) {
        if (ir->dst->kind != IMM_STRING) {
            ERROR(NULL, "COMMENT is not a string");
            return;
        }
        flexbuf_addstr(fb, "' ");
        str = ir->dst->name;
        while (*str && *str != '\n') {
            flexbuf_addchar(fb, *str);
            str++;
        }
        flexbuf_addchar(fb, '\n');
        return;
    }
    if (ir->opc == OPC_DUMMY) {
        return;
    }
    if (ir->opc == OPC_REPEAT_END) {
        // not an actual instruction, just a marker for
        // avoiding moving instructions
        return;
    }
    if (ir->opc == OPC_CONST) {
        // handle const declaration
        if (!inCon) {
            flexbuf_addstr(fb, "CON\n");
            inCon = 1;
            inDat = 0;
        }
        flexbuf_addstr(fb, "\t");
        PrintOperand(fb, ir->dst);
        flexbuf_addstr(fb, " = ");
        PrintOperandAsValue(fb, ir->src);
        flexbuf_addstr(fb, "\n");
        return;
    }
    if (!inDat) {
        if (!didPub && P) {
            EmitSpinMethods(fb, P);
            didPub = 1;
        }
        flexbuf_addstr(fb, "DAT\n");
        inCon = 0;
        inDat = 1;
        if (!didOrg) {
            flexbuf_addstr(fb, "\torg\t0\n");
            didOrg = 1;
        }
    }
    if (gl_compressed) {
        switch(ir->opc) {
        case OPC_DJNZ:
        case OPC_CALL:
        case OPC_JUMP:
        case OPC_RET:
            flexbuf_addstr(fb, "\tlong\t$FFFF\n");
            return;
        default:
            break;
        }
    } else if (!gl_p2) {
        // handle jumps in LMM mode
        switch (ir->opc) {
        case OPC_CALL:
            if (IsHubDest(ir->dst)) {
                if (!lmmMode) {
                    // call of hub function from COG
                    PrintCond(fb, ir->cond);
                    flexbuf_addstr(fb, "mov\tpc, $+2\n");
                    PrintCond(fb, ir->cond);
                    flexbuf_addstr(fb, "call\t#LMM_CALL_FROM_COG\n");
                } else {
                    PrintCond(fb, ir->cond);
                    flexbuf_addstr(fb, "jmp\t#LMM_CALL\n");
                }
                flexbuf_addstr(fb, "\tlong\t");
                if (ir->dst->kind != IMM_HUB_LABEL) {
                    ERROR(NULL, "internal error: non-hub label in LMM jump");
                }
                PrintOperandAsValue(fb, ir->dst);
                flexbuf_addstr(fb, "\n");
                return;
            }
            break;
        case OPC_DJNZ:
            if (ir->fcache) {
                PrintCond(fb, ir->cond);
                flexbuf_addstr(fb, "djnz\t");
                PrintOperand(fb, ir->dst);
                flexbuf_addstr(fb, ", #LMM_FCACHE_START + (");
                PrintOperand(fb, ir->src);
                flexbuf_addstr(fb, " - ");
                PrintOperand(fb, ir->fcache);
                flexbuf_addstr(fb, ")\n");
                return;
            } else if (IsHubDest(ir->src)) {
                PrintCond(fb, ir->cond);
                flexbuf_addstr(fb, "djnz\t");
                PrintOperand(fb, ir->dst);
                flexbuf_addstr(fb, ", #LMM_JUMP\n");
                flexbuf_addstr(fb, "\tlong\t");
                if (ir->src->kind != IMM_HUB_LABEL) {
                    ERROR(NULL, "internal error: non-hub label in LMM jump");
                }
                PrintOperandAsValue(fb, ir->src);
                flexbuf_addstr(fb, "\n");
                return;
            }
            break;
        case OPC_JUMP:
            if (ir->fcache) {
                PrintCond(fb, ir->cond);
                flexbuf_addstr(fb, "jmp\t#LMM_FCACHE_START + (");
                PrintOperand(fb, ir->dst);
                flexbuf_addstr(fb, " - ");
                PrintOperand(fb, ir->fcache);
                flexbuf_addstr(fb, ")\n");
                return;
            } else if (IsHubDest(ir->dst)) {
                IR *dest;
                if (!lmmMode) {
                    ERROR(NULL, "jump from COG to LMM not supported yet");
                }
                if (ir->dst->kind != IMM_HUB_LABEL) {
                    ERROR(NULL, "internal error: non-hub label in LMM jump");
                }
                PrintCond(fb, ir->cond);
                // if we know the destination we may be able to optimize
                // the branch
                if (ir->aux) {
                    int offset;
                    dest = (IR *)ir->aux;
                    offset = dest->addr - ir->addr;
                    if ( offset > 0 && offset < MAX_REL_JUMP_OFFSET) {
                        flexbuf_printf(fb, "add\tpc, #4*(");
                        PrintOperand(fb, ir->dst);
                        flexbuf_printf(fb, " - ($+1))\n");
                        return;
                    }
                    if ( offset < 0 && offset > -MAX_REL_JUMP_OFFSET) {
                        flexbuf_printf(fb, "sub\tpc, #4*(($+1) - ");
                        PrintOperand(fb, ir->dst);
                        flexbuf_printf(fb, ")\n");
                        return;
                    }
                }
                flexbuf_addstr(fb, "rdlong\tpc,pc\n");
                flexbuf_addstr(fb, "\tlong\t");
                PrintOperandAsValue(fb, ir->dst);
                flexbuf_addstr(fb, "\n");
                return;
            }
            break;
        case OPC_RET:
            if (ir->fcache) {
                ERROR(NULL, "return from fcached code not supported");
                return;
            } else if (lmmMode) {
                PrintCond(fb, ir->cond);
                flexbuf_addstr(fb, "jmp\t#LMM_RET\n");
                return;
            }
        default:
            break;
        }
    }
    
    if (ir->instr) {
        int ccset;
        
        PrintCond(fb, ir->cond);
        flexbuf_addstr(fb, ir->instr->name);
        switch (ir->instr->ops) {
        case NO_OPERANDS:
            break;
        case SRC_OPERAND_ONLY:
        case DST_OPERAND_ONLY:
        case CALL_OPERAND:
        case P2_JUMP:
        case P2_DST_CONST_OK:
            flexbuf_addstr(fb, "\t");
            PrintOperandSrc(fb, ir->dst, OPEFFECT_NONE);
            break;
        default:
            flexbuf_addstr(fb, "\t");
            if (ir->opc == OPC_REPEAT) {
                flexbuf_addstr(fb, "@");
            }
            PrintOperand(fb, ir->dst);
            flexbuf_addstr(fb, ", ");
            PrintOperandSrc(fb, ir->src, ir->srceffect);
            break;
        }
        ccset = ir->flags & (FLAG_WC|FLAG_WZ|FLAG_NR|FLAG_WR);
        if (ccset) {
            const char *sepstring = " ";
            if (gl_p2 && ((FLAG_WC|FLAG_WZ) == (ccset & (FLAG_WC|FLAG_WZ)))) {
                flexbuf_printf(fb, "%swcz", sepstring);
                sepstring = ",";
            } else { 
                if (ccset & FLAG_WC) {
                    flexbuf_printf(fb, "%swc", sepstring);
                    sepstring = ",";
                }
                if (ccset & FLAG_WZ) {
                    flexbuf_printf(fb, "%swz", sepstring);
                    sepstring = ",";
                }
            }
            if (ccset & FLAG_NR) {
                flexbuf_printf(fb, "%snr", sepstring);
            } else if (ccset & FLAG_WR) {
                flexbuf_printf(fb, "%swr", sepstring);
            }
        }
#if 0        
        if (ir->flags & FLAG_KEEP_INSTR) {
            flexbuf_printf(fb, " '' (volatile)");
        }
#endif
        flexbuf_addstr(fb, "\n");
        return;
    }
    
    switch(ir->opc) {
    case OPC_DEAD:
        /* no code necessary, internal opcode */
        flexbuf_addstr(fb, "\t.dead\t");
        flexbuf_addstr(fb, ir->dst->name);
        flexbuf_addstr(fb, "\n");
        break;
    case OPC_LITERAL:
        PrintOperand(fb, ir->dst);
	break;
    case OPC_LABEL:
        flexbuf_addstr(fb, ir->dst->name);
        flexbuf_addstr(fb, "\n");
        break;
    case OPC_RET:
        flexbuf_addchar(fb, '\t');
        flexbuf_addstr(fb, "ret\n");
        break;
    case OPC_BYTE:
    case OPC_WORD:
    case OPC_WORD1:
    case OPC_LONG:
    case OPC_STRING:
        flexbuf_addchar(fb, '\t');
	flexbuf_addstr(fb, StringFor(ir->opc));
	flexbuf_addstr(fb, "\t");
	PrintOperandAsValue(fb, ir->dst);
        if (ir->src) {
            // repeat count
            flexbuf_addstr(fb, "[");
            PrintOperandAsValue(fb, ir->src);
            flexbuf_addstr(fb, "]");
        }
        flexbuf_addstr(fb, "\n");
	break;
    case OPC_RESERVE:
        flexbuf_printf(fb, "\tres\t");
        PrintOperandAsValue(fb, ir->dst);
        flexbuf_addstr(fb, "\n");
        break;
    case OPC_RESERVEH:
        flexbuf_printf(fb, "\tlong\t0[");
        PrintOperandAsValue(fb, ir->dst);
        flexbuf_addstr(fb, "]\n");
        break;
    case OPC_FCACHE:
        flexbuf_printf(fb, "\tcall\t#LMM_FCACHE_LOAD\n");
        flexbuf_printf(fb, "\tlong\t(");
        PrintOperandAsValue(fb, ir->dst);
        flexbuf_printf(fb, "-");
        PrintOperandAsValue(fb, ir->src);
        flexbuf_printf(fb, ")\n");
        break;
    case OPC_LABELED_BLOB:
        // output a binary blob
        // dst has a label
        // data is in a string in src
        OutputBlob(fb, ir->dst, ir->src);
        break;
    case OPC_FIT:
        flexbuf_addstr(fb, "\tfit\t496\n");
        break;
    case OPC_ORG:
        flexbuf_printf(fb, "\torg\t");
        PrintOperandAsValue(fb, ir->dst);
        flexbuf_printf(fb, "\n");
        break;
    case OPC_HUBMODE:
        if (gl_p2) {
            flexbuf_printf(fb, "\torgh\t$%x\n", P2_HUB_BASE);
        } else if (gl_compressed) {
            flexbuf_printf(fb, "\torgh\n");
        }
        lmmMode = 1;
        break;
    default:
        ERROR(NULL, "Internal error: unable to process IR\n");
        break;
    }
}

/* assemble an IR list */
char *
IRAssemble(IRList *list, Module *P)
{
    IR *ir;
    struct flexbuf fb;
    char *ret;
    
    inDat = 0;
    inCon = 0;
    didOrg = 0;
    didPub = 0;
    lmmMode = 0;
    
    if (gl_p2 && gl_output != OUTPUT_COGSPIN) {
        didPub = 1; // we do not want PUB declaration in P2 code
    }
    flexbuf_init(&fb, 512);
    for (ir = list->head; ir; ir = ir->next) {
        DoAssembleIR(&fb, ir, P);
        if (gl_output == OUTPUT_COGSPIN) {
            if (pending_fixup) {
                flexbuf_printf(&fb, "__fixup_%d\n", pending_fixup);
                pending_fixup = 0;
            }
        }
    }
    if (gl_output == OUTPUT_COGSPIN) {
        flexbuf_printf(&fb, "__fixup_ptr\n\tlong\t");
        if (fixup_number > 0) {
            flexbuf_printf(&fb, "@__fixup_%d - 4\n", fixup_number);
        } else {
            flexbuf_printf(&fb, "0\n");
        }
    }
    flexbuf_addchar(&fb, 0);
    ret = flexbuf_get(&fb);
    flexbuf_delete(&fb);
    return ret;
}

void
DumpIRL(IRList *irl)
{
    puts(IRAssemble(irl, NULL));
}
