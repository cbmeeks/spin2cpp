/*
 * Spin to C/C++ translator
 * Copyright 2011-2018 Total Spectrum Software Inc.
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
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include "spinc.h"
#include "preprocess.h"
#include "version.h"
#include "spin.tab.h"

//#define DEBUG_YACC

extern int spinyyparse(void);
extern int basicyyparse(void);

extern int spinyydebug;
extern int basicyydebug;

// process a module after parsing it
static void ProcessModule(Module *P);

static int
FindSymbolExact(SymbolTable *S, const char *name)
{
    Symbol *sym = FindSymbol(S, name);
    if (!sym)
        return 0;
    if (!strcmp(sym->name, name))
        return 1;
    return 0;
}

/*
 * make sure that a class name is safe, i.e. will
 * not conflict with any identifier or C keyword/function
 */
static void
makeClassNameSafe(Module *P)
{
    // check for conflict with C reserved word */
    if ( Is_C_Reserved(P->classname) 
         || (0 != FindSymbolExact(&P->objsyms, P->classname))
        )
    {
        NormalizeIdentifier(P->classname);
    }

    // now check for conflicts within the class
    while (0 != FindSymbolExact(&P->objsyms, P->classname)) {
        char *newname = (char *)calloc(1, strlen(P->classname)+8);
        strcpy(newname, P->classname);
        free(P->classname);
        P->classname = newname;
        strcat(P->classname, "Class");
    }
}

/*
 * add global variable functions
 */

/* code for P1 */
const char p1_system_spincode[] =
    "pri waitcnt(x)\n"
    "  asm\n"
    "    waitcnt x,#0\n"
    "  endasm\n"
    "pri getcnt\n"
    "  return cnt\n"
    "pri waitpeq(pin, mask, c)\n"
    "  asm\n"
    "    waitpeq pin,mask\n"
    "  endasm\n"
    "pri waitpne(pin, mask, c)\n"
    "  asm\n"
    "    waitpne pin,mask\n"
    "  endasm\n"
    "pri cogid | rval\n"
    "  asm\n"
    "    cogid rval\n"
    "  endasm\n"
    "  return rval\n"
    "pri cogstop(id)\n"
    "  asm\n"
    "    cogstop id\n"
    "  endasm\n"
    "  return 0\n"
    "pri clkset(mode, freq)\n"
    "  CLKFREQ := freq\n"
    "  CLKMODE := mode\n"
    "  asm\n"
    "    clkset mode\n"
    "  endasm\n"
    "pri reboot\n"
    "  clkset($80, 0)\n"
    "pri lockclr(id) | mask, rval\n"
    "  mask := -1\n"
    "  asm\n"
    "    lockclr id wc\n"
    "    muxc   rval,mask\n"
    "  endasm\n"
    "  return rval\n"
    "pri lockset(id) | mask, rval\n"
    "  mask := -1\n"
    "  asm\n"
    "    lockset id wc\n"
    "    muxc   rval,mask\n"
    "  endasm\n"
    "  return rval\n"
    "pri locknew | rval\n"
    "  asm\n"
    "    locknew rval\n"
    "  endasm\n"
    "  return rval\n"
    "pri lockret(id)\n"
    "  asm\n"
    "    lockret id\n"
    "  endasm\n"
    "  return 0\n"
    "pri _coginit(id, code, param) | parm\n"
    "  parm := (param & $fffc) << 16\n"
    "  parm |= (code & $fffc) << 2\n"
    "  parm | = id & $f\n"
    "  asm\n"
    "    coginit parm wr\n"
    "  endasm\n"
    "  return parm\n"
    "pri _sqrt(a) | r, bit, tmp\n"
    "  if (a =< 0)\n"
    "    return 0\n"
    "  r := 0\n"
    "  bit := (1<<30)\n"
    "  repeat while (bit > a)\n"
    "    bit := bit >> 2\n"
    "  repeat while (bit <> 0)\n"
    "    tmp := r+bit\n"
    "    if (a => tmp)\n"
    "      a := a - tmp\n"
    "      r := (r >> 1) + bit\n"
    "    else\n"
    "      r := r >> 1\n"
    "    bit := bit >> 2\n"
    "  return r\n"
    ;

// common code (for both P1 and P2)
const char common_spincode[] =
    "pri longfill(ptr, val, count)\n"
    "  repeat count\n"
    "    long[ptr] := val\n"
    "    ptr += 4\n"
    "pri longmove(dst, src, count)\n"
    "  repeat count\n"
    "    long[dst] := long[src]\n"
    "    dst += 4\n"
    "    src += 4\n"
    "pri wordfill(ptr, val, count)\n"
    "  repeat count\n"
    "    word[ptr] := val\n"
    "    ptr += 2\n"
    "pri wordmove(dst, src, count)\n"
    "  repeat count\n"
    "    word[dst] := word[src]\n"
    "    dst += 2\n"
    "    src += 2\n"
    "pri bytefill(ptr, val, count)\n"
    "  repeat count\n"
    "    byte[ptr] := val\n"
    "    ptr += 1\n"
    "pri bytemove(dst, src, count)\n"
    "  repeat count\n"
    "    byte[dst] := byte[src]\n"
    "    dst += 1\n"
    "    src += 1\n"
    "pri strsize(str) : r\n"
    "  r := 0\n"
    "  repeat while byte[str] <> 0\n"
    "    r++\n"
    "    str++\n"
    "pri strcomp(s1, s2) | c1, c2\n"
    "  repeat\n"
    "    c1 := byte[s1++]\n"
    "    c2 := byte[s2++]\n"
    "    if (c1 <> c2)\n"
    "      return 0\n"
    "  until (c1 == 0)\n"
    "  return -1\n"
    "pri _lookup(x, b, arr, n) | i\n"
    "  i := x - b\n"
    "  if (i => 0 and i < n)\n"
    "    return long[arr][i]\n"
    "  return 0\n"
    "pri _lookdown(x, b, arr, n) | i\n"
    "  repeat i from 0 to n-1\n"
    "    if (long[arr] == x)\n"
    "      return i+b\n"
    "    arr += 4\n"
    "  return 0\n"
    "pri _lfsr_forward(x) | a\n"
    "  if (x == 0)\n"
    "    x := 1\n"
    "  a := $8000000b\n"
    "  repeat 32\n"
    "    asm\n"
    "      test x, a wc\n"
    "      rcl  x, #1\n"
    "    endasm\n"
    "  return x\n"
    "pri _lfsr_backward(x) | a\n"
    "  if (x == 0)\n"
    "    x := 1\n"
    "  a := $17\n"
    "  repeat 32\n"
    "    asm\n"
    "      test x, a wc\n"
    "      rcr  x, #1\n"
    "    endasm\n"
    "  return x\n"
    "con\n"
    " _txpin = 30\n"
    " _bitcycles = 80_000_000 / 115_200\n"
    "pri _tx(c) | val, nextcnt\n"
    "  OUTA[_txpin] := 1\n"
    "  DIRA[_txpin] := 1\n"
    "  val := (c | 256 | 512) << 1\n"
    "  nextcnt := cnt\n"
    "  repeat 11\n"
    "    waitcnt(nextcnt += _bitcycles)\n"
    "    OUTA[_txpin] := val\n"
    "    val >>= 1\n"

    "pri _basic_print_char(c)\n"
    "  return _tx(c)\n"
    
    "pri _basic_print_string(ptr)|c\n"
    "  repeat while ((c := byte[ptr++]) <> 0)\n"
    "    _basic_print_char(c)\n"
    
    "pri _basic_print_uns_raw(x) | d\n"
    "  d := x // 10\n"
    "  x := x / 10\n"
    "  if (x)\n"
    "    _basic_print_uns_raw(x)\n"
    "  _basic_print_char(d + \"0\")\n"
    
    "pri _basic_print_integer(x)\n"
    "  if (x < 0)\n"
    "    _basic_print_char(\"-\")\n"
    "    x := -x\n"
    "  _basic_print_uns_raw(x)\n"
    "pri _basic_print_float(x)\n"
    "  return\n"
    "pri _basic_print_nl\n"
    "  _basic_print_char(13)\n"
    "  _basic_print_char(10)\n"
;

// code for P2
const char p2_system_spincode[] =
    "pri cnt | r\n"
    "  asm\n"
    "    getct r\n"
    "  endasm\n"
    "  return r\n"
    "pri getcnt | r\n"
    "  asm\n"
    "    getct r\n"
    "  endasm\n"
    "  return r\n"
    "pri waitcnt(x)\n"
    "  asm\n"
    "    addct1  x, #0\n"
    "    waitct1\n"
    "  endasm\n"
    "pri cogid | rval\n"
    "  asm\n"
    "    cogid rval\n"
    "  endasm\n"
    "  return rval\n"
    "pri cogstop(id)\n"
    "  asm\n"
    "    cogstop id\n"
    "  endasm\n"
    "  return 0\n"
    "pri lockclr(id) | mask, rval\n"
    "  mask := -1\n"
    "  asm\n"
    "    lockrel id wc\n"
    "    muxc   rval,mask\n"
    "  endasm\n"
    "  return rval\n"
    "pri lockset(id) | mask, rval\n"
    "  mask := -1\n"
    "  asm\n"
    "    locktry id wc\n"
    "    muxnc   rval,mask\n"  /* NOTE: C bit is opposite in P2 */
    "  endasm\n"
    "  return rval\n"
    "pri locknew | rval\n"
    "  asm\n"
    "    locknew rval\n"
    "  endasm\n"
    "  return rval\n"
    "pri lockret(id)\n"
    "  asm\n"
    "    lockret id\n"
    "  endasm\n"
    "  return 0\n"
    "pri clkset(mode, freq)\n"
    "  CLKFREQ := freq\n"
    "  CLKMODE := mode\n"
    "  asm\n"
    "    hubset mode\n"
    "  endasm\n"
    "pri reboot\n"
    "  clkset($80, 0)\n"
    "pri _sqrt(a) | r\n"
    "  if (a =< 0)\n"
    "    return 0\n"
    "  asm\n"
    "    qsqrt a, #0\n"
    "    getqx r\n"
    "  endasm\n"
    "  return r\n"
    "pri _coginit(id, code, param)\n"
    "  asm\n"
    "    setq param\n"
    "    coginit id, code wc\n"
    "  endasm\n"
    "  return id\n"
;

void
InitGlobalModule(void)
{
    SymbolTable *table;
    Symbol *sym;
    int oldtmpnum;
    const char *syscode;
    
    current = globalModule = NewModule("_system_", LANG_SPIN);
    table = &globalModule->objsyms;
    sym = AddSymbol(table, "CLKFREQ", SYM_VARIABLE, ast_type_long);
    sym->flags |= SYMF_GLOBAL;
    sym->offset = gl_p2 ? P2_HUB_BASE : 0;
    sym = AddSymbol(table, "CLKMODE", SYM_VARIABLE, ast_type_byte);
    sym->flags |= SYMF_GLOBAL;
    sym->offset = gl_p2 ? 0x804 : 4;

    /* compile inline assembly */
    if (gl_output == OUTPUT_ASM || gl_output == OUTPUT_COGSPIN) {
        /* set up temporary variable processing */
        oldtmpnum = SetTempVariableBase(90000, 0);

        if (gl_p2) {
            syscode = p2_system_spincode;
        } else {
            syscode = p1_system_spincode;
        }
        strToLex(&globalModule->L, syscode, "_system_");
        spinyyparse();
        strToLex(&globalModule->L, common_spincode, "_common_");
        spinyyparse();
        
        ProcessModule(globalModule);

        curfunc = NULL;
        /* restore temp variable base */
        (void)SetTempVariableBase(oldtmpnum, 89999);
    }

}

static int
DeclareVariablesOfSize(Module *P, int basetypesize, int offset)
{
    AST *upper;
    AST *ast;
    AST *curtype;
    int curtypesize;
    
    for (upper = P->varblock; upper; upper = upper->right) {
        AST *idlist;
        if (upper->kind != AST_LISTHOLDER) {
            ERROR(upper, "Expected list holder\n");
        }
        ast = upper->left;
        if (ast->kind == AST_COMMENTEDNODE)
            ast = ast->left;
        switch (ast->kind) {
        case AST_BYTELIST:
            curtype = ast_type_byte;
            curtypesize = 1;
            idlist = ast->left;
            break;
        case AST_WORDLIST:
            curtype = ast_type_word;
            curtypesize = 2;
            idlist = ast->left;
            break;
        case AST_LONGLIST:
	    curtype = NULL; // was ast_type_generic;
            curtypesize = 4;
            idlist = ast->left;
            break;
        case AST_DECLARE_GLOBAL:
            curtype = ast->left;
            idlist = ast->right;
            curtypesize = TypeSize(curtype);
            break;
        case AST_COMMENT:
            /* skip */
            continue;
        default:
            ERROR(ast, "bad type  %d in variable list\n", ast->kind);
            return offset;
        }
        if (basetypesize == curtypesize || (basetypesize == 4 && curtypesize >= 4)) {
            offset = EnterVars(SYM_VARIABLE, &current->objsyms, curtype, idlist, offset);
        }
    }
    return offset;
    
}

AST *
InferTypeFromName(AST *identifier)
{
    const char *name = identifier->d.string;
    if (!*name) {
        ERROR(identifier, "Internal error, empty identifier");
        return NULL;
    }
    while (name[1] != 0) {
        name++;
    }
    switch(name[0]) {
    case '$':
        return ast_type_string;
    case '%':
        return ast_type_long;
    case '#':
        return ast_type_float;
    default:
        return ast_type_long;
    }
}

void
MaybeDeclareGlobal(Module *P, AST *identifier, AST *typ)
{
    if (typ->kind == AST_OBJECT) {
        AST *newobj;
        if (AstUses(P->objblock, identifier))
            return;

        if (typ->kind == AST_OBJDECL) {
            typ = typ->left;
        }
        newobj = NewAST(AST_OBJECT, identifier, NULL);
        newobj->d.ptr = typ->d.ptr;
        P->objblock = AddToList(P->objblock, newobj);
        DeclareObjects(newobj);
        return;
    }
    
    if (!AstUses(P->varblock, identifier)) {
        AST *iddecl = NewAST(AST_LISTHOLDER, identifier, NULL);
        AST *newdecl = NewAST(AST_DECLARE_GLOBAL, typ, iddecl);
        P->varblock = AddToList(P->varblock, NewAST(AST_LISTHOLDER, newdecl, NULL));
    }
}

void
DeclareVariables(Module *P)
{
    int offset = 0;

    // FIXME: Spin always declares longs first, then words, then bytes
    // but other languages may have other preferences
    offset = DeclareVariablesOfSize(P, 4, offset); // also declares >= 4
    offset = DeclareVariablesOfSize(P, 2, offset);
    offset = DeclareVariablesOfSize(P, 1, offset);

    // round up to next LONG boundary
    offset = (offset + 3) & ~3;
    P->varsize = offset;
}

void
DeclareObjects(AST *newobjs)
{
    AST *ast;
    AST *obj;

    for (ast = newobjs; ast; ast = ast->right) {
        if (ast->kind != AST_OBJECT) {
            ERROR(ast, "Internal error: expected an OBJECT");
            break;
        }
        obj = ast->left;
        if (obj->kind == AST_OBJDECL) {
            obj = obj->left;
        }
        if (obj->kind == AST_IDENTIFIER) {
            AddSymbol(&current->objsyms, obj->d.string, SYM_OBJECT, ast);
        } else if (obj->kind == AST_ARRAYDECL) {
            AST *id = obj->left;
            AddSymbol(&current->objsyms, id->d.string, SYM_OBJECT, ast);
        } else {
            ERROR(ast, "Internal error: bad object definition");
        }
    }
}

/* helper function for parsing pasm FILE directives */
AST *
GetFullFileName(AST *baseString)
{
    const char *basename = baseString->d.string;
    char *newname;
    AST *ret;
    newname = find_file_on_path(&gl_pp, basename, NULL, current->fullname);
    ret = NewAST(AST_STRING, NULL, NULL);
    ret->d.string = newname;
    return ret;
}

/*
 * recursively assign offsets to all objects in modules
 */
void
AssignObjectOffsets(Module *P)
{
    Module *Q;
    AST *ast, *obj;
    Symbol *sym = NULL;
    int offset;
    int count;
    Module *save=current;
    
    current = P;
    offset = P->varsize;
    for (ast = P->objblock; ast; ast = ast->right) {
        if (ast->kind != AST_OBJECT) {
            ERROR(ast, "Internal error: expected an OBJECT");
            return;
        }
        obj = ast->left;
        if (obj->kind == AST_IDENTIFIER) {
            sym = FindSymbol(&P->objsyms, obj->d.string);
            count = 1;
        } else if (obj->kind == AST_ARRAYDECL) {
            sym = FindSymbol(&P->objsyms, obj->left->d.string);
            count = EvalConstExpr(obj->right);
        } else if (obj->kind == AST_OBJDECL) {
            sym = FindSymbol(&P->objsyms, obj->left->d.string);
            count = 0;
        }
        if (sym == NULL) {
            ERROR(ast, "Internal error, cannot find object symbol");
            return;
        }
        Q = GetObjectPtr(sym);
        AssignObjectOffsets(Q);
        sym->offset = offset;
        offset += count * Q->varsize;
    }
    offset = (offset+3) & ~3;
    P->varsize = offset;
    current = save;
}

/*
 * process a parsed module
 */
static void
ProcessModule(Module *P)
{
    P->botcomment = GetComments();

    if (P->body) {
        AST *funcdecl = NewAST(AST_FUNCDECL, AstIdentifier("program"), NULL);
        AST *funcvars = NewAST(AST_FUNCVARS, NULL, NULL);
        AST *funcdef = NewAST(AST_FUNCDEF, funcdecl, funcvars);
        DeclareFunction(ast_type_void, 1, funcdef, P->body, NULL, NULL);
    }
    
    /* now declare all the symbols that weren't already declared */
    DeclareConstants(&P->conblock);
    DeclareVariables(P);
    DeclareLabels(P);
    DeclareFunctions(P);
}

/*
 * parse a spin file
 * This is the main entry point for the compiler
 * "name" is the file name; if it has no .spin suffix
 * we'll try it with one
 * if "gl_depth" is >= 0 then print the file name
 */
int gl_depth = 0;

static void
doparse(int language)
{
  if (language == LANG_BASIC) {
    basicyydebug = spinyydebug;
    basicyyparse();
  } else {
    spinyyparse();
  }
}

Module *
ParseFile(const char *name)
{
    FILE *f = NULL;
    Module *P, *save, *Q, *LastQ;
    char *fname = NULL;
    char *parseString = NULL;
    char *langptr;
    int language = LANG_SPIN;
    
    // check language to process
    langptr = strrchr(name, '.');
    if (langptr) {
      if (!strcmp(langptr, ".bas") || !strcmp(langptr, ".basic")) {
	language = LANG_BASIC;
      } else if (!strcmp(langptr, ".c")) {
	language = LANG_C;
      }
    } else {
      langptr = ".spin";
    }
    if (current)
      fname = find_file_on_path(&gl_pp, name, langptr, current->fullname);
    if (!fname)
      fname = strdup(name);

    f = fopen(fname, "r");
    if (!f) {
        fprintf(stderr, "ParseFile: ");
        perror(name);
        free(fname);
        exit(1);
    }
    save = current;
    P = NewModule(fname, language);

    if (gl_printprogress) {
        int n = gl_depth;
        const char *tail;
        while (n > 0) {
            printf("|-");
            --n;
        }
        tail = FindLastDirectoryChar(fname);
        if (tail) {
            tail++;
        } else {
            tail = fname;
        }
        printf("%s\n", tail);
        gl_depth++;
    }
    
    /* if we have already visited an object with this name, skip it */
    /* also finds the last element in the list, so we can append to
       the list easily */
    LastQ = NULL;
    for (Q = allparse; Q; Q = Q->next) {
        if (!strcmp(P->basename, Q->basename)) {
            free(fname);
            free(P);
            fclose(f);
            if (gl_printprogress) {
                gl_depth--;
            }
            return Q;
        }
        LastQ = Q;
    }
    if (LastQ)
        LastQ->next = P;
    else
        allparse = P;
    current = P;

    if (gl_preprocess) {
        void *defineState;

        pp_push_file(&gl_pp, fname);
        defineState = pp_get_define_state(&gl_pp);
        pp_run(&gl_pp);
        parseString = pp_finish(&gl_pp);
        pp_restore_define_state(&gl_pp, defineState);
        strToLex(&current->L, parseString, fname);
	doparse(language);
        free(parseString);
    } else {
        fileToLex(&current->L, f, fname);
        doparse(language);
    }
    fclose(f);

    if (gl_errors > 0) {
        free(fname);
        exit(1);
    }
    if (gl_printprogress) {
        gl_depth--;
    }
    ProcessModule(P);

    /* work to avoid conflicts with variables and constants */
    makeClassNameSafe(P);
    if (gl_gas_dat) {
        current->datname = (char *)malloc(strlen(P->classname) + 40);
        if (!current->datname) {
            fprintf(stderr, "Out of memory!\n");
            exit(2);
        }
        sprintf(current->datname, "_dat_%s_", P->classname);
    } else {
        current->datname = "dat";
    }

    if (gl_errors > 0) {
        free(fname);
        exit(1);
    }

    current = save;
    return P;
}

//
// now that we've figured out what's reachable, remove
// uncalled methods from modules
//
static void
doPruneMethods(Module *P)
{
    Function *pf;
    Function **oldptr = &P->functions;
    for(;;) {
        pf = *oldptr;
        if (!pf) break;
        if (pf->callSites == 0) {
            *oldptr = pf->next; // remove this function
        } else {
            oldptr = &pf->next;
        }
    }
}

//
// remove unused methods
// if "isBinary" is true we can eliminate any methods not called
// directly or indirectly from the main function
// Otherwise, we mark everything accessible from public functions
//
void
RemoveUnusedMethods(int isBinary)
{
    Module *P, *LastP;
    Function *pf;

    // mark everything unused
    for (P = allparse; P; P = P->next) {
        for (pf = P->functions; pf; pf = pf->next) {
            pf->callSites = 0;
        }
    }
    
    if (isBinary) {
        MarkUsed(GetMainFunction(allparse));
    } else {
        // mark stuff called via public functions
    }
    for (P = allparse; P; P = P->next) {
        for (pf = P->functions; pf; pf = pf->next) {
            if (pf->callSites == 0) {
                if (pf->is_public) {
                    MarkUsed(pf);
                } else if (pf->annotations) {
                    MarkUsed(pf);
                }
            }
            
        }
    }
    // Now remove the ones that are never called
    for (P = allparse; P; P = P->next) {
        doPruneMethods(P);
    }

    // finally remove modules that have no defined functions and
    // no constant definitions or dat sections
    LastP = NULL;
    P = allparse;
    while (P) {
        if (!P->functions && !P->datblock && !P->conblock && !P->varblock) {
            if (LastP) {
                LastP->next = P->next;
            } else {
                allparse = P->next;
            }
        }
        LastP = P;
        P = P->next;
    }
}

#define MAX_TYPE_PASSES 4

static void
FixupCode(Module *P, int isBinary)
{
    Module *Q, *LastQ;
    int changes;
    int tries = 0;

    // append the global module to the list
    if (allparse)
    {
        LastQ = NULL;
        for (Q = allparse; Q; Q = Q->next) {
            LastQ = Q;
        }
        LastQ->next = globalModule;
    }
    
    for (Q = allparse; Q; Q = Q->next) {
        if (Q->language == LANG_SPIN) {
            SpinTransform(Q);
        } else if (Q->language == LANG_BASIC) {
            BasicTransform(Q);
        }
        ProcessFuncs(Q);
    }
    do {
        changes = 0;
        for (Q = allparse; Q; Q = Q->next) {
            changes += InferTypes(Q);
        }
    } while (changes != 0 && tries++ < MAX_TYPE_PASSES);
    for (Q = allparse; Q; Q = Q->next) {
        PerformCSE(Q);
    }
    RemoveUnusedMethods(isBinary);
    AssignObjectOffsets(P);
}

Module *
ParseTopFile(const char *name, int outputBin)
{
    Module *P;
    current = allparse = NULL;
    P = ParseFile(name);
    if (P && gl_errors == 0) {
        FixupCode(P, outputBin);
    }
    return P;
}

Function *
GetMainFunction(Module *P)
{
    if (P->language == LANG_BASIC) {
        // look for function named "program"
        Function *f;
        for (f = P->functions; f; f = f->next) {
            if (!strcmp(f->name, "program")) {
                return f;
            }
        }
    }
    return P->functions;
}
