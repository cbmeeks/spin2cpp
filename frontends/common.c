/*
 * Spin to C/C++ translator
 * Copyright 2011-2021 Total Spectrum Software Inc.
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

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
#undef ERROR // thanks, wingdi.h
#endif

#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include "common.h"
#include "preprocess.h"
#include "version.h"

/* declarations common to all front ends */

Module *current = NULL;
Module *allparse = NULL;
Module *systemModule;
SymbolTable *currentTypes;

int gl_p2;
int gl_isutf8 = 1;
int gl_have_lut;
int gl_errors;
int gl_warnings_are_errors;
int gl_verbosity;
int gl_max_errors;
int gl_colorize_output;
int gl_output;
int gl_outputflags;
int gl_nospin;
int gl_gas_dat;
int gl_normalizeIdents;
int gl_debug;
int gl_expand_constants;
int gl_optimize_flags;
int gl_dat_offset;
int gl_warn_flags = WARN_ASM_USAGE;
int gl_exit_status = 0;
int gl_printprogress = 0;
int gl_infer_ctypes = 0;
int gl_listing = 0;
int gl_fixedreal = 0;
unsigned int gl_hub_base = 0x400;
int gl_no_coginit = 0;
int gl_lmm_kind = LMM_KIND_ORIG;
int gl_interp_kind = INTERP_KIND_P1ROM;
int gl_relocatable = 0;

int gl_default_baud = 0;

// bytes and words are unsigned by default, but longs are signed by default
// this is confusing, and someday we should make all of these explicit
// but it's a legacy from Spin
AST *ast_type_word, *ast_type_long, *ast_type_byte;
AST *ast_type_signed_word, *ast_type_signed_byte;
AST *ast_type_unsigned_long;
AST *ast_type_float, *ast_type_string;
AST *ast_type_ptr_long;
AST *ast_type_ptr_word;
AST *ast_type_ptr_byte;
AST *ast_type_ptr_void;
AST *ast_type_generic;
AST *ast_type_const_generic;
AST *ast_type_void;
AST *ast_type_bitfield;
AST *ast_type_long64, *ast_type_unsigned_long64;
AST *ast_type_float64;
AST *ast_type_generic_funcptr;
AST *ast_type_sendptr;
AST *ast_type_recvptr;

const char *gl_progname = "spin2cpp";
char *gl_header1 = NULL;
char *gl_header2 = NULL;

typedef struct alias {
    const char *name;
    const char *alias;
} Aliases;

Aliases spinalias[] = {
    { "call", "_call" },
    { "clkfreq", "__clkfreq_var" },
    { "clkmode", "__clkmode_var" },
    { "clkset", "_clkset" },
    { "cogid", "_cogid" },
    { "cogstop", "_cogstop" },
    
    { "strsize", "__builtin_strlen" },

    { "lockclr", "_lockclr" },
    { "lockset", "_lockset" },
    { "locknew", "_locknew" },
    { "lockret", "_lockret" },


    { "reboot", "_reboot" },

    /* ugh, don't know if we should continue to support these */
    { "_pinw", "_pinwrite" },
    { "_pinl", "_drvl" },
    { "_pinh", "_drvh" },
    
    /* obsolete aliases */
    { "dirl_", "_dirl" },
    { "dirh_", "_dirh" },
    { "drvl_", "_drvl" },
    { "drvh_", "_drvh" },
    { "drvnot_", "_drvnot" },
    { "drv_", "_drv" },

    { NULL, NULL },
};
Aliases spin2alias[] = {
    /* special constants */
    { "clkmode_", "__clkmode_con" },
    { "clkfreq_", "__clkfreq_con" },
    
    /* other symbols */
    { "cnt", "_getcnt" },
    { "cogchk", "_cogchk" },

    { "locktry", "_locktry" },
    { "lockrel", "_lockclr" },
    
    { "pinw", "_pinwrite" },
    { "pinl", "_drvl" },    
    { "pinh", "_drvh" },
    { "pint", "_drvnot" },
    { "pinr", "_pinread" },
    { "pinf", "_fltl" },
    { "pinstart", "_pinstart" },
    { "pinsetup", "_pinsetup" },
    { "pinclear", "_pinclear" },
    
//    { "pinrnd", "_drvrnd" },
    { "pinwrite", "_pinwrite" },
    { "pinlow", "_drvl" },    
    { "pinhigh", "_drvh" },
    { "pintoggle", "_drvnot" },
    { "pinread", "_pinread" },
    { "pinfloat", "_fltl" },
    { "pinmode", "_pinmode" },
    
    { "getct", "_getcnt" },
    { "getms", "_getms" },
    { "getrnd", "_getrnd" },
    { "getsec", "_getsec" },
    { "hubset", "_hubset" },
    
    { "wrpin", "_wrpin" },
    { "wxpin", "_wxpin" },
    { "wypin", "_wypin" },
    
    { "akpin", "_akpin" },
    { "rdpin", "_rdpin" },
    { "rqpin", "_rqpin" },

    { "polxy", "_polxy" },
    { "qcos",  "_qcos" },
    { "qsin",  "_qsin" },
    { "rotxy", "_rotxy" },
    { "xypol", "_xypol" },
    
    { "cogatn", "_cogatn" },
    { "pollatn", "_pollatn" },
    { "waitatn", "_waitatn" },
    
    { "muldiv64", "_muldiv64" },
    { "waitx", "_waitx" },
    { "waitms", "_waitms" },
    { "waitus", "_waitus" },
    { "pollct", "_pollct" },
    { "waitct", "_waitcnt" },
    
    { "recv", "__recvptr" },    
    { "send", "__sendptr" },
    
    /* obsolete aliases */
    { "outl_", "_outl" },
    { "outh_", "_outh" },

    { "fltl_", "__builtin_propeller_fltl" },
    { "flth_", "__builtin_propeller_flth" },

    { "wrpin_", "__builtin_propeller_wrpin" },
    { "wxpin_", "__builtin_propeller_wxpin" },
    { "wypin_", "__builtin_propeller_wypin" },

    { NULL, NULL },
};
Aliases basicalias[] = {
    /* ugh, not sure if we want to keep supporting the clk* variables,
     * but changing them to functions is difficult
     */
    { "clkfreq", "__clkfreq_var" },
    { "clkmode", "__clkmode_var" },
    /* the rest of these are OK, I think */
    { "chain",  "_execve" },
    { "clkset", "_clkset" },
    { "cpuchk",  "_cogchk" },
    { "cpuid",   "_cogid" },
    { "cpuwait", "_cogwait" },
    { "cpustop", "_cogstop" },
    { "getcnt",  "_getcnt" },
    { "geterr", "_geterror" },
    { "getrnd", "_getrnd" },
    { "getms", "_getms" },
    { "getus", "_getus" },
    { "getsec", "_getsec" },
    { "len", "__builtin_strlen" },
    { "mount", "_mount" },
    { "pausems", "_waitms" },
    { "pausesec", "_waitsec" },
    { "pauseus", "_waitus" },
    { "pinfloat", "_fltl" },
    { "pinlo", "_drvl" },
    { "pinhi", "_drvh" },
    { "pinread", "_pinread" },
    { "pinrnd", "_drvrnd" },
    { "pinset", "_drvw" },
    { "pintoggle", "_drvnot" },
    { "rnd", "_basic_rnd" },
    { "strerror$", "_strerror" },
    { "val", "__builtin_atof" },
    { "val%", "__builtin_atoi" },

    /* math functions */
    { "acos", "__builtin_acosf" },
    { "asin", "__builtin_asinf" },
    { "atan", "__builtin_atanf" },
    { "atan2", "__builtin_atan2f" },
    { "cos", "__builtin_cosf" },
    { "exp", "__builtin_expf" },
    { "log", "__builtin_logf" },
    { "pow", "__builtin_powf" },
    { "sin", "__builtin_sinf" },
    { "tan", "__builtin_tanf" },

    { "round", "_float_round" },
    
    { NULL, NULL },
};
Aliases calias[] = {
    /* these are obsolete but we'll support them for now */
    { "_clkfreq", "__clkfreq_var" },
    { "_clkmode", "__clkmode_var" },

    /* new propeller2.h standard */
    { "_cnt",  "_getcnt" },
    { "_cnth",  "_getcnth" },
    { "_cnthl",  "_getcnthl" },
    { "_clockfreq", "__builtin_clkfreq" },
    { "_clockmode", "__builtin_clkmode" },
    { "_isqrt", "_sqrt" },
    { "_lockrel", "_lockclr" },
    { "_pinl", "_drvl" },
    { "_pinh", "_drvh" },
    { "_pinnot", "_drvnot" },
    { "_pinw", "_drvw" },

    /* some math functions */
    { "__builtin_round", "_float_round" },

    /* useful I/O functions */
    { "mount", "_mount" },
    { NULL, NULL },
};

// forward declarations
static int CalcClkFreqP1(Module *P);
static int CalcClkFreqP2(Module *P);
static void DeclareBaud(Module *P);

//
// check for system module
//
bool
IsSystemModule(Module *P)
{
    return P == systemModule;
}

//
// create aliases appropriate to the language
//
static void
addAliases(SymbolTable *tab, Aliases *A)
{
    while (A && A->name) {
        AddSymbol(tab, A->name, SYM_WEAK_ALIAS, (void *)A->alias, NULL);
        A++;
    }
}

static void
initSymbols(Module *P, int language)
{
    Aliases *A;

    /* NOTE: we do not want the Spin aliases polluting the
     * C namespace, so do not add the aliases to the
     * system (global) module
     */
    if (!systemModule || P == systemModule) {
        return;
    }
    if (IsBasicLang(language)) {
        A = basicalias;
    } else if (IsCLang(language)) {
        A = calias;
    } else {
        A = spinalias;
    }
    addAliases(&P->objsyms, A);
    if (gl_p2 && (IsBasicLang(language)||IsSpinLang(language))) {
        addAliases(&P->objsyms, spin2alias);
    } else if (language == LANG_SPIN_SPIN2) {
        addAliases(&P->objsyms, spin2alias);
    }
}

/*
 * allocate a new parser state
 */ 
Module *
NewModule(const char *fullname, int language)
{
    Module *P;
    char *s;
    char *root;

    P = (Module *)calloc(1, sizeof(*P));
    if (!P) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    P->mainLanguage = P->curLanguage = language;
    P->longOnly = 1;
    /* set up the base file name */
    P->fullname = fullname;
    P->basename = strdup(fullname);
    s = strrchr(P->basename, '.');
    if (s) {
        /* make sure sub-object names cannot conflict with the main object name, even if the roots match (like foo.spin and foo.c) */
        if (allparse && ( (strncmp(s, ".spin", 5) != 0) || !IsSpinLang(allparse->mainLanguage) ) ) {
            *s = '_';
        } else {
            *s = 0;
        }
    }
    root = strrchr(P->basename, '/');
#if defined(WIN32) || defined(WIN64)
    if (!root) {
      root = strrchr(P->basename, '\\');
    }
#endif
    if (!root)
      root = P->basename;
    else
      P->basename = root+1;
    /* set up the class name */
    P->classname = (char *)calloc(1, strlen(P->basename)+1);
    strcpy(P->classname, P->basename);

    /* link the global symbols */
    if (systemModule) {
        P->objsyms.next = &systemModule->objsyms;
    } else if (IsBasicLang(language)) {
        P->objsyms.next = &basicReservedWords;
    } else if (IsCLang(language)) {
        P->objsyms.next = &cReservedWords;
    } else {
        P->objsyms.next = &spinCommonReservedWords;
    }
    /* set appropriate case sensititity */
    if (LangCaseInSensitive(language)) {
        P->objsyms.flags |= SYMTAB_FLAG_NOCASE;
    }
    /* set up default symbols */
    initSymbols(P, language);
    P->body = NULL;
    return P;
}

/*
 * declare constant symbols
 */
static Symbol *
EnterConstant(Module *P, const char *name, AST *expr)
{
    Symbol *sym;

    // check to see if the symbol already has a definition
    sym = FindSymbol(&P->objsyms, name);
    if (sym) {
        if (sym->kind == SYM_CONSTANT || sym->kind == SYM_FLOAT_CONSTANT) {
            int32_t origval;
            int32_t newval;

            origval = EvalConstExpr((AST *)sym->val);
            newval = EvalConstExpr(expr);
            if (origval != newval) {
                ERROR(expr, "Redefining %s with a different value", name);
                return NULL;
            }
            return NULL; /* did not create new symbol */
        }
    }
    if (IsFloatConst(expr)) {
        sym = AddSymbol(&P->objsyms, name, SYM_FLOAT_CONSTANT, (void *)expr, NULL);
    } else {
        sym = AddSymbol(&P->objsyms, name, SYM_CONSTANT, (void *)expr, NULL);
    }
    return sym;
}

void
DeclareConstants(Module *P, AST **conlist_ptr)
{
    AST *conlist;
    AST *upper, *ast, *id;
    AST *next;
    AST *completed_declarations = NULL;
    int default_val;
    int default_val_ok = 0;
    int default_skip;
    int n;
    
    conlist = *conlist_ptr;

    // first do all the simple assignments
    // this is necessary because Spin will sometimes allow out-of-order
    // assignments

    do {
        n = 0; // no assignments yet
        default_val = 0;
        default_skip = 1;
        default_val_ok = 1;
        upper = conlist;
        while (upper) {
            next = upper->right;
            if (upper->kind == AST_LISTHOLDER) {
                ast = upper->left;
                if (ast->kind == AST_COMMENTEDNODE)
                    ast = ast->left;

                switch (ast->kind) {
                case AST_ASSIGN:
                    if (IsConstExpr(ast->right)) {
                        if (!IsIdentifier(ast->left)) {
                            ERROR(ast, "Internal error, bad constant declaration");
                            return;
                        }                            
                        EnterConstant(P, GetIdentifierName(ast->left), ast->right);
                        n++;
                        // now pull the assignment out so we don't see it again
                        RemoveFromList(conlist_ptr, upper);
                        upper->right = NULL;
                        completed_declarations = AddToList(completed_declarations, upper);
                        conlist = *conlist_ptr;
                    } else {
                        AST *typ;
                        typ = ExprType(ast->right);
                        if (typ && (IsStringType(typ) || IsPointerType(typ))) {
                            if (!IsIdentifier(ast->left)) {
                                ERROR(ast, "Internal error, bad constant declaration");
                                return;
                            }
                            typ = NewAST(AST_MODIFIER_CONST, typ, NULL);
                            // pull it out and put it in the DAT section
                            DeclareOneGlobalVar(current, ast, typ, 1);
                            RemoveFromList(conlist_ptr, upper);
                            upper->right = NULL;
                            conlist = *conlist_ptr;
                        }
                    }
                    break;
                case AST_ENUMSET:
                    if (IsConstExpr(ast->left)) {
                        default_val = EvalConstExpr(ast->left);
                        default_val_ok = 1;
                        default_skip = ast->right ? EvalConstExpr(ast->right) : 1;
                        RemoveFromList(conlist_ptr, upper);
                        upper->right = NULL;
                        completed_declarations = AddToList(completed_declarations, upper);
                        conlist = *conlist_ptr;
                    } else {
                        default_val_ok = 0;
                    }
                    break;
                case AST_ENUMSKIP:
                    if (default_val_ok) {
                        id = ast->left;
                        if (id->kind != AST_IDENTIFIER) {
                            ERROR(ast, "Internal error, expected identifier in constant list");
                        } else {
                            EnterConstant(P, id->d.string, AstInteger(default_val));
                            default_val += EvalConstExpr(ast->right);
                        }
                        n++;
                        // now pull the assignment out so we don't see it again
                        RemoveFromList(conlist_ptr, upper);
                        upper->right = NULL;
                        completed_declarations = AddToList(completed_declarations, upper);
                        conlist = *conlist_ptr;
                    }
                    break;
                case AST_IDENTIFIER:
                    if (default_val_ok) {
                        EnterConstant(P, ast->d.string, AstInteger(default_val));
                        default_val += default_skip;
                        n++;
                        // now pull the assignment out so we don't see it again
                        RemoveFromList(conlist_ptr, upper);
                        upper->right = NULL;
                        completed_declarations = AddToList(completed_declarations, upper);
                        conlist = *conlist_ptr;
                    }
                    break;
                default:
                    /* do nothing */
                    break;
                }
            }
            upper = next;
        }
    } while (n > 0);

    default_val = 0;
    default_val_ok = 1;
    default_skip = 1;
    for (upper = conlist; upper; upper = upper->right) {
        if (upper->kind == AST_LISTHOLDER) {
            ast = upper->left;
            if (ast->kind == AST_COMMENTEDNODE)
                ast = ast->left;
            switch (ast->kind) {
            case AST_ENUMSET:
                default_val = EvalConstExpr(ast->left);
                default_skip = ast->right ? EvalConstExpr(ast->right) : 1;
                break;
            case AST_IDENTIFIER:
                EnterConstant(P, ast->d.string, AstInteger(default_val));
                default_val += default_skip;
                break;
            case AST_ENUMSKIP:
                id = ast->left;
                if (id->kind != AST_IDENTIFIER) {
                    ERROR(ast, "Internal error, expected identifier in constant list");
                } else {
                    EnterConstant(P, id->d.string, AstInteger(default_val));
                    default_val += EvalConstExpr(ast->right);
                }
                break;
            case AST_ASSIGN:
                EnterConstant(P, ast->left->d.string, ast->right);
                default_val = EvalConstExpr(ast->right) + default_skip;
                break;
            case AST_COMMENT:
                /* just skip it for now */
                break;
            default:
                ERROR(ast, "Internal error: bad AST value %d", ast->kind);
                break;
            }
        } else {
            ERROR(upper, "Expected list in constant, found %d instead", upper->kind);
        }
    }
    completed_declarations = AddToList(completed_declarations, conlist);
    *conlist_ptr = completed_declarations;

    /* for the top level module, calculate frequency and declare constants if necessary */
    if (IsTopLevel(P)) {
        if (gl_p2) {
            CalcClkFreqP2(P);
        } else {
            CalcClkFreqP1(P);
        }
        DeclareBaud(P);
    }
}

#if 0
/*
 * transform AST_SRCCOMMENTs into comments with the line data of the 
 * next non-comment AST
 */
static void
TransformSrcCommentsBlock(AST *ast, AST *upper)
{
    while (ast) {
        if (ast->kind == AST_SRCCOMMENT) {
            ast->kind = AST_COMMENT;
            ast->d.string = "+++";
        }
        TransformSrcCommentsBlock(ast->left, ast);
        upper = ast;
        ast = ast->right;
    }
}
#endif

AST *
NewObject(AST *identifier, AST *string)
{
    AST *ast;
    const char *filename;

    if (string) {
        filename = string->d.string;
    } else {
        filename = NULL;
    }

    ast = NewAST(AST_OBJECT, identifier, NULL);
    if (filename) {
        ast->d.ptr = ParseFile(filename);
    }
    return ast;
}

AST *
NewAbstractObject(AST *identifier, AST *string)
{
    return NewObject( NewAST(AST_OBJDECL, identifier, 0), string );
}

int
DifferentLineNumbers(AST *a, AST *b)
{
    LineInfo *infoa = GetLineInfo(a);
    LineInfo *infob = GetLineInfo(b);
    if (infoa->lineno != infob->lineno) {
        return 1;
    }
    if (strcmp(infoa->fileName, infob->fileName) != 0) {
        return 1;
    }
    return 0;
}

#ifdef WIN32
static const WORD colorWindows[] = {
    0x07, // PRINT_NORMAL
    0x0B, // PRINT_NOTE
    0x0E, // PRINT_WARNING
    0x0C, // PRINT_ERROR
    0x05, // PRINT_DEBUG
    0x0F, // PRINT_ERROR_LOCATION
};
#else 
static const char *colorANSI[] = {
    "\033[0m", // PRINT_NORMAL
    "\033[0;1;36m", // PRINT_NOTE
    "\033[0;1;33m", // PRINT_WARNING
    "\033[0;1;31m", // PRINT_ERROR
    "\033[0;35m", // PRINT_DEBUG
    "\033[0;1m", // PRINT_ERROR_LOCATION
};
#endif

enum printColorKind current_print_color;

void SETCOLOR(enum printColorKind color) {
    if (!gl_colorize_output) return;
    #ifdef WIN32
        SetConsoleTextAttribute(GetStdHandle(STD_ERROR_HANDLE),colorWindows[color]);
    #else
        fprintf(stderr,colorANSI[color]);
    #endif
    current_print_color = color;
}

void
ERRORHEADER(const char *fileName, int lineno, const char *msg)
{
    if (fileName && lineno) {
        enum printColorKind save = current_print_color;
        SETCOLOR(PRINT_ERROR_LOCATION);
        fprintf(stderr, "%s:%d: ", fileName, lineno);
        SETCOLOR(save);
    } 
    fprintf(stderr, "%s: ", msg);

}

void
ERROR(AST *instr, const char *msg, ...)
{
    va_list args;
    LineInfo *info = GetLineInfo(instr);

    SETCOLOR(PRINT_ERROR);

    if (info)
        ERRORHEADER(info->fileName, info->lineno, "error");
    else
        ERRORHEADER(NULL, 0, "error");

    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");
    gl_errors++;
    RESETCOLOR();
}

void
SYNTAX_ERROR(const char *msg, ...)
{
    va_list args;

    SETCOLOR(PRINT_ERROR);

    if (current)
        ERRORHEADER(current->Lptr->fileName, current->Lptr->lineCounter, "error");
    else
        ERRORHEADER(NULL, 0, "error");

    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");
    gl_errors++;
    RESETCOLOR();
}

void
LANGUAGE_WARNING(int language, AST *ast, const char *msg, ...)
{
    va_list args;
    const char *banner;
    
    if (!(gl_warn_flags & WARN_LANG_EXTENSIONS)) {
        return;
    }
    if (!current || IsSystemModule(current)) {
        return;
    }
    if (language != LANG_ANY && language != current->curLanguage) {
        return;
    }
    if (gl_warnings_are_errors) {
        banner = "ERROR";
        SETCOLOR(PRINT_ERROR);
        gl_errors++;
    } else {
        banner = "warning";
        SETCOLOR(PRINT_WARNING);
    }
    if (ast) {
        LineInfo *info = GetLineInfo(ast);
        if (info) {
            ERRORHEADER(info->fileName, info->lineno, banner);
        } else {
            ERRORHEADER(NULL, 0, banner);
        }
    } else if (current) {
        ERRORHEADER(current->Lptr->fileName, current->Lptr->lineCounter, banner);
    } else {
        ERRORHEADER(NULL, 0, banner);
    }
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");
    RESETCOLOR();
}

void
WARNING(AST *instr, const char *msg, ...)
{
    va_list args;
    LineInfo *info = GetLineInfo(instr);
    const char *banner;

    if (gl_warnings_are_errors) {
        gl_errors++;
        banner = "ERROR";
        SETCOLOR(PRINT_ERROR);
    } else {
        banner = "warning";
        SETCOLOR(PRINT_WARNING);
    }
    if (info)
        ERRORHEADER(info->fileName, info->lineno, banner);
    else
        ERRORHEADER(NULL, 0, banner);

    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");
    RESETCOLOR();
}

void
NOTE(AST *instr, const char *msg, ...)
{
    va_list args;
    LineInfo *info = GetLineInfo(instr);

    SETCOLOR(PRINT_NOTE);

    if (info)
        ERRORHEADER(info->fileName, info->lineno, "note");
    else
        ERRORHEADER(NULL, 0, "note: ");

    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");
    RESETCOLOR();
}

void
DEBUG(AST *instr, const char *msg, ...)
{
    va_list args;
    if (gl_verbosity <= 0) return;
    LineInfo *info = GetLineInfo(instr);

    SETCOLOR(PRINT_DEBUG);
    if (info)
        ERRORHEADER(info->fileName, info->lineno, "info");
    else
        ERRORHEADER(NULL, 0, "info");

    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");
    RESETCOLOR();
}

void
ERROR_UNKNOWN_SYMBOL(AST *ast)
{
    const char *name;
    Label *labelref;
    
    if (IsIdentifier(ast)) {
        name = GetVarNameForError(ast);
    } else if (ast->kind == AST_VARARGS || ast->kind == AST_VA_START) {
        name = "__vararg";
    } else {
        name = "";
    }
    ERROR(ast, "Unknown symbol %s", name);
    // add a definition for this symbol so we don't get this error again
    if (curfunc) {
        AddLocalVariable(curfunc, ast, NULL, SYM_LOCALVAR);
    } else {
        labelref = (Label *)calloc(1, sizeof(*labelref));
        AddSymbol(&systemModule->objsyms, name, SYM_LABEL, labelref, NULL);
    }
}

AST *
GenericFunctionPtr(int numresults)
{
    AST *fptr = NULL;
    AST *exprlist = NULL;

    if (numresults == 0) {
        exprlist = ast_type_void;
    } else if (numresults == 1) {
        exprlist = NULL;
    } else {
        while (numresults > 0) {
            exprlist = NewAST(AST_TUPLE_TYPE, NULL, exprlist);
            --numresults;
        }
    }
    fptr = NewAST(AST_FUNCTYPE, exprlist, NULL);
    fptr = NewAST(AST_PTRTYPE, fptr, NULL);
    return fptr;
}

void
Init()
{
    ast_type_long64 = NewAST(AST_INTTYPE, AstInteger(8), NULL);
    ast_type_long = NewAST(AST_INTTYPE, AstInteger(4), NULL);
    ast_type_word = NewAST(AST_UNSIGNEDTYPE, AstInteger(2), NULL);
    ast_type_byte = NewAST(AST_UNSIGNEDTYPE, AstInteger(1), NULL);

    ast_type_unsigned_long64 = NewAST(AST_UNSIGNEDTYPE, AstInteger(8), NULL);
    ast_type_unsigned_long = NewAST(AST_UNSIGNEDTYPE, AstInteger(4), NULL);
    ast_type_signed_word = NewAST(AST_INTTYPE, AstInteger(2), NULL);
    ast_type_signed_byte = NewAST(AST_INTTYPE, AstInteger(1), NULL);

    ast_type_float = NewAST(AST_FLOATTYPE, AstInteger(4), NULL);
    ast_type_float64 = NewAST(AST_FLOATTYPE, AstInteger(8), NULL);

    ast_type_generic = NewAST(AST_GENERICTYPE, AstInteger(4), NULL);
    ast_type_const_generic = NewAST(AST_MODIFIER_CONST, ast_type_generic, NULL);
    ast_type_void = NewAST(AST_VOIDTYPE, AstInteger(0), NULL);

    ast_type_ptr_long = NewAST(AST_PTRTYPE, ast_type_long, NULL);
    ast_type_ptr_word = NewAST(AST_PTRTYPE, ast_type_word, NULL);
    ast_type_ptr_byte = NewAST(AST_PTRTYPE, ast_type_byte, NULL);
    ast_type_ptr_void = NewAST(AST_PTRTYPE, ast_type_void, NULL);

    ast_type_bitfield = NewAST(AST_BITFIELD, NULL, NULL);
    
    // string is pointer to const byte
    ast_type_string = NewAST(AST_PTRTYPE, NewAST(AST_MODIFIER_CONST, ast_type_byte, NULL), NULL);

    // a generic function returning a single (unknown) value
    ast_type_generic_funcptr = GenericFunctionPtr(1);

    // a generic function for Spin2 SEND type functionality
    ast_type_sendptr = NewAST(AST_MODIFIER_SEND_ARGS, GenericFunctionPtr(0), NULL);
    ast_type_recvptr = GenericFunctionPtr(1);
    
    initSpinLexer(gl_p2);

    /* fill in the global symbol table */
    InitGlobalModule();
}

const char *
FindLastDirectoryChar(const char *fname)
{
    const char *found = NULL;
    if (!fname) return NULL;
    while (*fname) {
        if (*fname == '/'
#ifdef WIN32
            || *fname == '\\'
#endif
            )
        {
            found = fname;
        }
        fname++;
    }
    return found;
}

//
// use the directory portion of "directory" (if any) and then add
// on the basename
//
char *
ReplaceDirectory(const char *basename, const char *directory)
{
  char *ret = (char *)malloc(strlen(basename) + strlen(directory) + 2);
  char *dot;
  if (!ret) {
    fprintf(stderr, "FATAL: out of memory\n");
    exit(2);
  }
  strcpy(ret, directory);
  dot = (char *)FindLastDirectoryChar(ret);
  if (dot) {
      *dot++ = '/';
  } else {
      dot = ret;
  }
  strcpy(dot, basename);
  return ret;
}

char *
ReplaceExtension(const char *basename, const char *extension)
{
  char *ret = (char *)malloc(strlen(basename) + strlen(extension) + 1);
  char *dot;
  if (!ret) {
    fprintf(stderr, "FATAL: out of memory\n");
    exit(2);
  }
  strcpy(ret, basename);
  dot = strrchr(ret, '/');
  if (!dot) dot = ret;
  dot = strrchr(dot, '.');
  if (dot) *dot = 0;
  strcat(ret, extension);
  return ret;
}

//
// append an extension
//
char *
AddExtension(const char *basename, const char *extension)
{
    char *ret = (char *)malloc(strlen(basename) + strlen(extension) + 1);
    if (!ret) {
      fprintf(stderr, "FATAL: out of memory\n");
      exit(2);
    }
    strcpy(ret, basename);
    strcat(ret, extension);
    return ret;
}

//
// add a propeller checksum to a binary file
// may also pad the image out to form a .eeprom
// image, if eepromSize is non-zero
//
int
DoPropellerChecksum(const char *fname, size_t eepromSize)
{
    FILE *f = fopen(fname, "r+b");
    unsigned char checksum = 0;
    int c, r;
    uint32_t len;
    uint32_t padbytes;
    uint32_t maxlen;
    uint32_t reserveSize = 0;
    Symbol *sym;
    int save_casesensitive;
    
    if (!f) {
        perror(fname);
        return -1;
    }
    fseek(f, 0L, SEEK_END);
    len = ftell(f);  // find length of file

    // pad file to multiple of 4, if necessary
    if (gl_p2) {
        padbytes = 0;
    } else {    
        padbytes = ((len + 3) & ~3) - len;
    }
    if (padbytes) {
        while (padbytes > 0) {
            fputc(0, f);
            padbytes--;
            len++;
        }
    }

    // check for special symbols
    current = GetTopLevelModule();
    // we are at the end of compilation, check for stack info in a
    // case insensitive manner
    save_casesensitive = gl_caseSensitive;
    gl_caseSensitive = 0;
    sym = FindSymbol(&current->objsyms, "_STACK");
    if (sym && sym->kind == SYM_CONSTANT) {
        reserveSize += LONG_SIZE * EvalConstExpr((AST *)sym->val);
    }
    sym = FindSymbol(&current->objsyms, "_FREE");
    if (sym && sym->kind == SYM_CONSTANT) {
        reserveSize += LONG_SIZE * EvalConstExpr((AST *)sym->val);
    }
    gl_caseSensitive = save_casesensitive;
    // do sanity check on length
    if (eepromSize) {
        maxlen = eepromSize;
    } else if (gl_p2) {
        maxlen = 512 * 1024;
    } else {
        maxlen = 32768;
    }
    if ( (len + reserveSize) > maxlen) {
        if (reserveSize) {
            WARNING(NULL, "final output size of %d bytes + %d reserved bytes exceeds maximum of %d by %d bytes", len, reserveSize, maxlen,(len + reserveSize) - maxlen);
        } else {
            WARNING(NULL, "final output size of %d bytes exceeds maximum of %d by %d bytes", len, maxlen, len - maxlen);
        }
    }
    // if P2, no checksum needed
    if (gl_p2) {
        fclose(f);
        return 0;
    }
#ifdef NEVER
    // this part is obsolete now, the OutputSpinDummyFooter function
    // does it
    // update header fields
    fseek(f, 8L, SEEK_SET); // seek to 16 bit vbase field
    fputc(len & 0xff, f);
    fputc( (len >> 8) & 0xff, f);
    // update dbase = vbase + 2 * sizeof(int)
    fputc( (len+8) & 0xff, f);
    fputc( ((len+8)>>8) & 0xff, f);
    // update initial program counter
    fputc( 0x18, f );
    fputc( 0x00, f );
    // update dcurr = dbase + 2 * sizeof(int)
    fputc( (len+16) & 0xff, f);
    fputc( ((len+16)>>8) & 0xff, f);
#endif
    
    fseek(f, 0L, SEEK_SET);
    for(;;) {
        c = fgetc(f);
        if (c < 0) break;
        checksum += (unsigned char)c;
    }
    fflush(f);
    checksum = 0x14 - checksum;
    r = fseek(f, 5L, SEEK_SET);
    if (r != 0) {
        perror("fseek");
        return -1;
    }
    //printf("writing checksum 0x%x\n", checksum);
    r = fputc(checksum, f);
    if (r < 0) {
        perror("fputc");
        return -1;
    }
    fflush(f);
    if (eepromSize && eepromSize >= len + 8) {
        fseek(f, 0L, SEEK_END);
        fputc(0xff, f); fputc(0xff, f);
        fputc(0xf9, f); fputc(0xff, f);
        fputc(0xff, f); fputc(0xff, f);
        fputc(0xf9, f); fputc(0xff, f);
        len += 8;
        while (len < eepromSize) {
            fputc(0, f);
            len++;
        }
    }
    fclose(f);
    return 0;
}

//
// check the version
//
void
CheckVersion(const char *str)
{
    int majorVersion = 0;
    int minorVersion = 0;
    int revVersion = 0;

    while (isdigit(*str)) {
        majorVersion = 10 * majorVersion + (*str - '0');
        str++;
    }
    if (*str == '.') {
        str++;
        while (isdigit(*str)) {
            minorVersion = 10 * minorVersion + (*str - '0');
            str++;
        }
    }
    if (*str == '.') {
        str++;
        while (isdigit(*str)) {
            revVersion = 10 * revVersion + (*str - '0');
            str++;
        }
    }

    if (majorVersion < VERSION_MAJOR)
        return; // everything OK
    if (majorVersion == VERSION_MAJOR) {
        if (minorVersion < VERSION_MINOR)
            return; // everything OK
        if (minorVersion == VERSION_MINOR) {
            if (revVersion <= VERSION_REV) {
                return;
            }
        }
    }
    fprintf(stderr, "ERROR: required version %d.%d.%d but current version is %s\n", majorVersion, minorVersion, revVersion, VERSIONSTR);
    exit(1);
}

AST *
NewCommentedInstr(AST *instr)
{
    AST *comment = GetComments();
    AST *ast;

    ast = NewAST(AST_INSTRHOLDER, instr, NULL);
    if (comment && (comment->d.string || comment->kind == AST_SRCCOMMENT)) {
        ast = NewAST(AST_COMMENTEDNODE, ast, comment);
    }
    return ast;
}

/* add a list element together with accumulated comments */
AST *
CommentedListHolder(AST *ast)
{
    AST *comment;

    if (!ast)
        return ast;

    comment = GetComments();

    if (comment) {
        ast = NewAST(AST_COMMENTEDNODE, ast, comment);
    }
    ast = NewAST(AST_LISTHOLDER, ast, NULL);
    return ast;
}

/* determine whether a loop needs a yield, and if so, insert one */
AST *
CheckYield(AST *body)
{
    AST *ast = body;

    if (!body)
        return AstYield();
    while (ast) {
        if (ast->left)
            return body;
        ast = ast->right;
    }
    return AddToList(body, AstYield());
}


/* push/pop current type symbols */
void PushCurrentTypes(void)
{
    SymbolTable *tab = (SymbolTable *)calloc(1, sizeof(SymbolTable));

    tab->next = currentTypes;
    tab->flags = currentTypes->flags;
    currentTypes = tab;
}

void PopCurrentTypes(void)
{
    if (currentTypes) {
        currentTypes = currentTypes->next;
    }
}

/* enter a single alias */
void
EnterLocalAlias(SymbolTable *table, AST *globalName, const char *localName)
{
    const char *newName = GetIdentifierName(globalName);
    AddSymbol(table, localName, SYM_REDEF, (void *)globalName, newName);
}

/* fixup declarations in a list */
/* basically this is to handle a case like
     char buf[100], *p = buf
   where the parser doesn't know that we've renamed the identifiers yet;
   after we add a new mapping for "buf" we need to update the reference
   to "buf" later in the list
   replace any identifiers with user name "oldName" in the list with "newIdent"
*/
static void
ReplaceIdentifiers(AST **parent, const char *oldName, AST *newIdent)
{
    AST *item = *parent;
    if (!item) return;
    if (IsIdentifier(item)) {
        if (!strcmp(oldName, GetUserIdentifierName(item))) {
            *parent = newIdent;
        }
    } else {
        ReplaceIdentifiers(&item->left, oldName, newIdent);
        if (item->kind != AST_METHODREF) {
            ReplaceIdentifiers(&item->right, oldName, newIdent);
        }
    }
}

static void
RemapIdentifiers(AST *list, AST *newIdent, const char *oldName)
{
    AST *decl;

    while (list) {
        decl = list->left;
        list = list->right;

        if (!decl) {
            continue;
        }
        ReplaceIdentifiers(&decl, oldName, newIdent);
    }
}

/* check a single declaration for typedefs or renamed variables */
static AST *
MakeOneDeclaration(AST *origdecl, SymbolTable *table, AST *restOfList)
{
    AST *decl = origdecl;
    AST *ident;
    AST *identinit = NULL;
    AST **identptr;
    Symbol *sym;
    const char *name;
    if (!decl) return decl;

    if (decl->kind == AST_DECLARE_ALIAS) {
        ERROR(decl, "internal error, DECLARE_ALIAS not supported yet\n");
        return decl;
    }
    if (decl->kind != AST_DECLARE_VAR) {
        ERROR(decl, "internal error, expected DECLARE_VAR");
        return decl;
    }
    ident = decl->right;
    identptr = &decl->right;
    decl = decl->left;
    if (!decl) {
        return decl;
    }
    if (!ident) {
        return origdecl;
    }
    if (ident->kind == AST_ASSIGN) {
        identinit = ident->right;
        identptr = &ident->left;
        ident = *identptr;
    }
    if (ident->kind == AST_LOCAL_IDENTIFIER) {
        // AST_LOCALIDENTIFIER(left,right) == (alias, user_name)
        ident = ident->right;
    }
    if (ident->kind != AST_IDENTIFIER) {
        ERROR(decl, "internal error: expected identifier in declaration");
        return NULL;
    }
    name = ident->d.string;
    sym = FindSymbol(table, name);
    if (sym) {
        WARNING(ident, "Redefining %s", name);
    }
            
    if (decl->kind == AST_TYPEDEF) {
        AddSymbolPlaced(table, name, SYM_TYPEDEF, decl->left, NULL, decl);
        return NULL;
    } else {
        const char *oldname = name;
        const char *newName = NewTemporaryVariable(oldname, NULL);
        AST *newIdent = AstIdentifier(newName);

        AddSymbolPlaced(table, oldname, SYM_REDEF, (void *)newIdent, newName, decl);
        if (identptr) {
            *identptr = NewAST(AST_LOCAL_IDENTIFIER, newIdent, ident);
            RemapIdentifiers(restOfList, newIdent, name);
            if (identinit) {
                RemapIdentifiers(identinit, newIdent, name);
            }
        } else {
            ERROR(decl, "internal error could not find identifier ptr");
        }
    }        
    return origdecl;
}

/* make declarations into a symbol table; if it's a type then add it to currentTypes */
/* returns the list of declarations that really need working on (typedefs are removed) */
AST *
MakeDeclarations(AST *origdecl, SymbolTable *table)
{
    AST *decl = origdecl;
    AST *item;

    if (!decl) return decl;
    if (decl->kind != AST_STMTLIST) {
        origdecl = decl = NewAST(AST_STMTLIST, decl, NULL);
    }
    while (decl && decl->kind == AST_STMTLIST) {
        item = MakeOneDeclaration(decl->left, table, decl->right);
        if (!item) {
            /* remove this from the list */
            decl->left = NULL;
        }
        decl = decl->right;
    }
    return origdecl;
}

/* add a subclass C to a class P */
void
AddSubClass(Module *P, Module *C)
{
    Module **ptr;
    Module *Q;
    ptr = &P->subclasses;
    while ( 0 != (Q = *ptr)) {
        ptr = &Q->subclasses;
    }
    *ptr = C;
    C->subclasses = 0;
}

/* declare a symbol that's an alias for an expression */
void
DeclareMemberAlias(Module *P, AST *ident, AST *expr)
{
    const char *name = GetIdentifierName(ident);
    const char *userName = GetUserIdentifierName(ident);
    Symbol *sym = FindSymbol(&P->objsyms, name);
    if (sym && sym->kind == SYM_VARIABLE) {
        ERROR(ident, "Redefining %s", userName);
        return;
    }
    AddSymbolPlaced(&P->objsyms, name, SYM_ALIAS, expr, NULL, ident);
}

// Declare typed global variables
// if inDat is 1, put them in the DAT section, otherwise make them
// member variables
// "ast" is a list of declarations
void
DeclareTypedGlobalVariables(AST *ast, int inDat)
{
    AST *idlist, *typ;
    AST *ident;

    if (!ast) return;
    if (ast->kind == AST_SEQUENCE) {
        ERROR(ast, "Internal error, unexpected sequence");
        return;
    }
    idlist = ast->right;
    typ = ast->left;
    if (!idlist) {
        return;
    }
    if (typ && typ->kind == AST_EXTERN) {
        return;
    }
    if (IsBasicLang(current->curLanguage)) {
        // BASIC does not require pointer notation for pointers to functions
        AST *subtype = RemoveTypeModifiers(typ);
        if (subtype && subtype->kind == AST_FUNCTYPE) {
            typ = NewAST(AST_PTRTYPE, typ, NULL);
        }
    }
    if (idlist->kind == AST_LISTHOLDER) {
        while (idlist) {
            ident = idlist->left;
            DeclareOneGlobalVar(current, ident, typ, inDat);
            idlist = idlist->right;
        }
    } else {
        DeclareOneGlobalVar(current, idlist, typ, inDat);
    }
    return;
}

// returns true if P is the top level module for this project
int
IsTopLevel(Module *P)
{
    return P == allparse;
}

// fetch the top level module
Module *
GetTopLevelModule(void)
{
    return allparse;
}

//
// check for whether a variable of type T must go on the stack
//
#define LARGE_SIZE_THRESHOLD 12

int TypeGoesOnStack(AST *typ)
{
    AST *subtyp;
    if (!typ) return 0;
    typ = RemoveTypeModifiers(typ);
    if (TypeSize(typ) > LARGE_SIZE_THRESHOLD) {
        return 1;
    }
    switch (typ->kind) {
    case AST_ARRAYTYPE:
        /* array of longs only are OK */
        subtyp = BaseType(typ);
        if (!subtyp || TypeSize(subtyp) == 4) {
            return 0;
        }
        return 1;
    case AST_OBJECT:
    {
        Module *P = GetClassPtr(typ);
        if (P && P->longOnly) {
            return 0;
        }
        return 1;
    }
    default:
        return 0;
    }
}

static int
GetExprlistLen(AST *list)
{
    int len = 0;
    AST *sub;
    while (list) {
        sub = list->left;
        list = list->right;
        if (sub->kind == AST_EXPRLIST) {
            len += GetExprlistLen(sub);
        } else if (sub->kind == AST_STRING) {
            len += strlen(sub->d.string);
        } else {
            len++;
        }
    }
    return len;
}

//
// find any previous declaration of name
//
static AST *
FindDeclaration(AST *datlist, const char *name)
{
    AST *ident;
    AST *declare;
    
    while (datlist) {
        if (datlist->kind == AST_COMMENTEDNODE
            && datlist->left
            && datlist->left->kind == AST_DECLARE_VAR)
        {
            declare = datlist->left;
            ident = declare->right;
            if (ident->kind == AST_ASSIGN) {
                ident = ident->left;
            }
            if (ident->kind == AST_LOCAL_IDENTIFIER) {
                ident = ident->left;
            }
            if (ident->kind == AST_IDENTIFIER) {
                if (!strcmp(name, ident->d.string)) {
                    return declare;
                }
            } else {
                ERROR(ident, "Internal error, expected identifier while searching for %s", name);
                return NULL;
            }
        }
        datlist = datlist->right;
    }
    return NULL;
}

void
DeclareOneGlobalVar(Module *P, AST *ident, AST *type, int inDat)
{
    AST *ast;
    AST *declare;
    AST *initializer = NULL;
    AST **initptr = NULL;
    Symbol *olddef;
    SymbolTable *table = &P->objsyms;
    const char *name = NULL;
    const char *user_name = "variable";
    int is_typedef = 0;
    AST *rawtype;
    
    if (!type) {
        type = InferTypeFromName(ident);
    }
    rawtype = RemoveTypeModifiers(type);
    
    // this may be a typedef
    if (type->kind == AST_TYPEDEF) {
        type = type->left;
        is_typedef = 1;
    }
    if (type->kind == AST_STATIC) {
        // FIXME: this case probably shouldn't happen any more??
        WARNING(ident, "internal error: did not expect static in code");
        type = type->left;
    }
    if (type->kind == AST_FUNCTYPE && !is_typedef) {
        // dummy declaration...
        return;
    }
    if (ident->kind == AST_ASSIGN) {
        if (is_typedef) {
            ERROR(ident, "typedef cannot have initializer");
        }
        initializer = ident->right;
        initptr = &ident->right;
        ident = ident->left;
    }
    if (ident->kind == AST_ARRAYDECL) {
        type = MakeArrayType(type, ident->right);
        type->d.ptr = ident->d.ptr;
        ident = ident->left;
    }
    if (!IsIdentifier(ident)) {
        ERROR(ident, "Internal error, expected identifier");
        return;
    }
    name = GetIdentifierName(ident);
    user_name = GetUserIdentifierName(ident);
    olddef = FindSymbol(table, name);
    if (is_typedef) {
        if (olddef) {
            ERROR(ident, "Redefining symbol %s", user_name);
        }
        AddSymbolPlaced(currentTypes, name, SYM_TYPEDEF, type, NULL, ident);
        return;
    }
    if (olddef) {
        ERROR(ident, "Redefining symbol %s", user_name);
    }
    // if this is an array type with no size, there must be an
    // initializer
    if (rawtype->kind == AST_ARRAYTYPE && !rawtype->right) {
        if (!initializer) {
            ERROR(ident, "global array %s declared with no size and no initializer", user_name);
            rawtype->right = AstInteger(1);
        } else {
            if (initializer->kind == AST_EXPRLIST) {
                initializer = FixupInitList(rawtype, initializer);
                *initptr = initializer;
                rawtype->right = AstInteger(AstListLen(initializer));
            } else if (initializer->kind == AST_STRINGPTR) {
                rawtype->right = AstInteger(GetExprlistLen(initializer->left) + 1);
            } else {
                rawtype->right = AstInteger(1);
            }
        }
    }
    if (!inDat) {
        /* make this a member variable */
        DeclareOneMemberVar(P, ident, type, 0);
        return;
    }
    // look through the globals to see if there's already a definition
    // if there is, and that definition has an initializer, we have a conflict
    declare = FindDeclaration(P->datblock, name);
    if (declare && declare->right) {
        if (declare->right->kind == AST_ASSIGN && initializer) {
            if (!AstBodyMatch(declare->right->right, initializer)) {
                ERROR(initializer, "Variable %s is initialized twice",
                      user_name);
                WARNING(declare->right->right, "Previous initialization was here");
            }
        } else if (initializer) {
            declare->right = AstAssign(DupAST(ident), initializer);
        }
    } else {
        if (initializer) {
            ident = AstAssign(ident, initializer);
        }
        declare = NewAST(AST_DECLARE_VAR, type, ident);
        ast = NewAST(AST_COMMENTEDNODE, declare, NULL);
        P->datblock = AddToList(P->datblock, ast);
    }
    return;
}

#define MAGIC_CLASS_SIZE 9999999

static int
DeclareMemberVariablesOfSize(Module *P, int basetypesize, int offset)
{
    AST *upper;
    AST *ast;
    AST *curtype;
    int curtypesize = basetypesize;
    int isUnion = P->isUnion;
    AST *varblocklist;
    int oldoffset = offset;
    unsigned sym_flags = 0;

    if (P->defaultPrivate) {
        sym_flags = SYMF_PRIVATE;
    }
    varblocklist = P->pendingvarblock;
    for (upper = varblocklist; upper; upper = upper->right) {
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
        case AST_DECLARE_VAR:
        case AST_DECLARE_VAR_WEAK:
            curtype = ast->left;
            idlist = ast->right;
            if (IsSpinLang(P->mainLanguage) && IsClassType(BaseType(curtype))) {
                curtypesize = MAGIC_CLASS_SIZE;
            } else {
                curtypesize = CheckedTypeSize(curtype); // make sure module variables are declared
            }
            if (ast->d.ival) {
                // variable should be private
                sym_flags = SYMF_PRIVATE;
            } else {
                sym_flags = 0;
            }
            if (curtype->kind == AST_ASSIGN) {
                if (basetypesize == 4 || basetypesize == 0) {
                    ERROR(ast, "Member variables cannot have initial values");
                    return offset;
                }
            }
            break;
        case AST_DECLARE_BITFIELD:
            /* skip */
            continue;
        case AST_COMMENT:
            /* skip */
            continue;
        default:
            ERROR(ast, "bad type  %d in variable list\n", ast->kind);
            return offset;
        }
        if (isUnion) {
            curtypesize = (curtypesize+3) & ~3;
            if (curtypesize > P->varsize) {
                P->varsize = curtypesize;
            }
        }
        if (basetypesize == MAGIC_CLASS_SIZE) {
            if (curtypesize == basetypesize) {
                offset = (offset + 3) & ~3; // long align
                offset = EnterVars(SYM_VARIABLE, &P->objsyms, curtype, idlist, offset, P->isUnion, sym_flags);
            }
        } else if (basetypesize == 0 && curtypesize != MAGIC_CLASS_SIZE) {
            // round offset up to necessary alignment
            // If you change this, be sure to change code for aligning
            // initializers, too!
            if (1 /*!gl_p2*/) { /* FIXME: what about PACKED structs */
                if (curtypesize == 2) {
                    offset = (offset + 1) & ~1;
                } else if (curtypesize >= 4) {
                    offset = (offset + 3) & ~3;
                }
            }
            // declare all the variables
            offset = EnterVars(SYM_VARIABLE, &P->objsyms, curtype, idlist, offset, P->isUnion, sym_flags);
        } else if (basetypesize == curtypesize) {
            offset = EnterVars(SYM_VARIABLE, &P->objsyms, curtype, idlist, offset, P->isUnion, sym_flags);
        }
    }
    if (curtypesize != 4 && offset != oldoffset) {
        P->longOnly = 0;
    }
    return offset;   
}

AST *
DeclareOneMemberVar(Module *P, AST *ident, AST *type, int is_private)
{
    AST *r = 0;
    if (!type) {
        type = InferTypeFromName(ident);
    }
    if (1) {
        AST *iddecl = NewAST(AST_LISTHOLDER, ident, NULL);
        AST *newdecl = NewAST(AST_DECLARE_VAR, type, iddecl);
        newdecl->d.ival = is_private;
        r = NewAST(AST_LISTHOLDER, newdecl, NULL);
        P->pendingvarblock = AddToList(P->pendingvarblock, r);
        if (IsSpinLang(P->mainLanguage) && IsClassType(type)) {
            // add a symbol for this because constants like x.foo can be accessed
            // in declarations of other variables
            const char *name = GetIdentifierName(ident);
            AddSymbol(&P->objsyms, name, SYM_VARIABLE, (void *)type, NULL);
        }
    }
    return r;
}

AST *
MaybeDeclareMemberVar(Module *P, AST *identifier, AST *typ, int is_private, unsigned flags)
{
    AST *ret = 0;
    AST *sub;
    const char *name;
    sub = identifier;
    if (sub && sub->kind == AST_ASSIGN) {
        sub = sub->left;
    }
    while (sub && sub->kind == AST_ARRAYDECL) {
        sub = sub->left;
    }
    if (!sub || sub->kind != AST_IDENTIFIER) {
        return 0;
    }
    name = GetIdentifierName(sub);
    Symbol *sym = FindSymbol(&P->objsyms, name);
    if (sym && sym->kind == SYM_VARIABLE) {
        return 0;
    }
    if (!typ) {
        typ = InferTypeFromName(identifier);
    }
    if (!AstUses(P->pendingvarblock, identifier)) {
        AST *iddecl = NewAST(AST_LISTHOLDER, identifier, NULL);
        AST *newdecl = NewAST(AST_DECLARE_VAR, typ, iddecl);
        newdecl->d.ival = is_private;
        ret = NewAST(AST_LISTHOLDER, newdecl, NULL);
        P->pendingvarblock = AddToList(P->pendingvarblock, ret);
    }
    return ret;
}

void
DeclareMemberVariables(Module *P)
{
    int offset;

    if (P->isUnion) {
        offset = 0;
    } else {
        offset = P->varsize;
    }
    if (P->mainLanguage == LANG_SPIN_SPIN1) {
        // Spin always declares longs first, then words, then bytes
        // but other languages may have other preferences
        offset = DeclareMemberVariablesOfSize(P, 4, offset); // also declares >= 4
        offset = DeclareMemberVariablesOfSize(P, 2, offset);
        offset = DeclareMemberVariablesOfSize(P, 1, offset);
        // declare objects
        offset = DeclareMemberVariablesOfSize(P, MAGIC_CLASS_SIZE, offset);
    } else if (P->mainLanguage == LANG_SPIN_SPIN2) {
        // declare everything except objects
        offset = DeclareMemberVariablesOfSize(P, 0, offset);
        // declare objects
        offset = DeclareMemberVariablesOfSize(P, MAGIC_CLASS_SIZE, offset);
    } else {
        // for non-Spin languages, everything declared together
        offset = DeclareMemberVariablesOfSize(P, 0, offset);
    }
    if (!P->isUnion) {
        // round up to next LONG boundary
        offset = (offset + 3) & ~3;
        P->varsize = offset;
    }
    if (P->pendingvarblock) {
        P->finalvarblock = AddToList(P->finalvarblock, P->pendingvarblock);
        P->pendingvarblock = NULL;
    }
}


Symbol *AddSymbolPlaced(SymbolTable *table, const char *name, int type, void *val, const char *user_name, AST *def)
{
    Symbol *sym = AddSymbol(table, name, type, val, user_name);
    if (sym) {
        sym->def = (void *)def;
    } else if (IsSpinLang(current->mainLanguage) && type == SYM_VARIABLE && IsClassType((AST *)val)) {
        // objects may have already been defined, don't object if they are
        sym = LookupSymbolInTable(table, name);
        if (sym && sym->kind == SYM_VARIABLE && sym->val == val) {
            //WARNING(def, "duplicate def");
            sym->def = def;
        } else {
            sym = NULL;
        }
    }
    return sym;
}

int32_t EvalConstSym(Symbol *sym)
{
    AST *ast;
    ast = (AST *)sym->val;
    return EvalConstExpr(ast);
}

// find _clkmode and _clkfreq settings for P1
static int
CalcClkFreqP1(Module *P)
{
    // look up in P->objsyms
    Symbol *clkmodesym = P ? FindSymbol(&P->objsyms, "_clkmode") : NULL;
    Symbol *sym;
    AST *ast;
    int32_t clkmode, clkfreq, xinfreq;
    int32_t multiplier = 1;
    uint8_t clkreg;
    
    if (!clkmodesym || clkmodesym->kind == SYM_ALIAS || clkmodesym->kind == SYM_WEAK_ALIAS) {
        return 0;  // nothing to do
    }
    ast = (AST *)clkmodesym->val;
    if (clkmodesym->kind != SYM_CONSTANT) {
        WARNING(ast, "_clkmode is not a constant");
        return 0;
    }
    clkmode = EvalConstExpr(ast);
    // now we need to figure out the frequency
    clkfreq = 0;
    sym = FindSymbol(&P->objsyms, "_clkfreq");
    if (sym && sym->kind != SYM_WEAK_ALIAS) {
        if (sym->kind == SYM_CONSTANT) {
            clkfreq = EvalConstExpr((AST*)sym->val);
        } else {
            WARNING((AST*)sym->val, "_clkfreq is not a constant");
        }
    }
    xinfreq = 0;
    sym = FindSymbol(&P->objsyms, "_xinfreq");
    if (sym) {
        if (sym->kind == SYM_CONSTANT) {
            xinfreq = EvalConstExpr((AST*)sym->val);
        } else {
            WARNING((AST*)sym->val, "_xinfreq is not a constant");
        }
    }
    // calculate the multiplier
    clkreg = 0;
    if (clkmode & RCFAST) {
        // nothing to do here
    } else if (clkmode & RCSLOW) {
        clkreg |= 0x01;   // CLKSELx
    } else if (clkmode & (XINPUT)) {
        clkreg |= (1<<5); // OSCENA
        clkreg |= 0x02;   // CLKSELx
    } else {
        clkreg |= (1<<5); // OSCENA
        clkreg |= (1<<6); // PLLENA
        if (clkmode & XTAL1) {
            clkreg |= (1<<3);
        } else if (clkmode & XTAL2) {
            clkreg |= (2<<3);
        } else {
            clkreg |= (3<<3);
        }
        if (clkmode & PLL1X) {
            multiplier = 1;
            clkreg |= 0x3;  // CLKSELx
        } else if (clkmode & PLL2X) {
            multiplier = 2;
            clkreg |= 0x4;  // CLKSELx
        } else if (clkmode & PLL4X) {
            multiplier = 4;
            clkreg |= 0x5;  // CLKSELx
        } else if (clkmode & PLL8X) {
            multiplier = 8;
            clkreg |= 0x6;  // CLKSELx
        } else if (clkmode & PLL16X) {
            multiplier = 16;
            clkreg |= 0x7;  // CLKSELx
        }
    }
    
    // validate xinfreq and clkfreq
    if (xinfreq == 0) {
        if (clkfreq == 0) {
            ERROR(NULL, "Must set at least one of _XINFREQ or _CLKFREQ");
            return 0;
        }
    } else {
        int32_t calcfreq = xinfreq * multiplier;
        if (clkfreq != 0) {
            if (calcfreq != clkfreq) {
                ERROR(NULL, "Inconsistent values for _XINFREQ and _CLKFREQ");
                return 0;
            }
        }
        clkfreq = calcfreq;
    }

    // define built in constants for these
    AddInternalSymbol(&P->objsyms, "__clkfreq_con", SYM_CONSTANT, AstInteger(clkfreq), NULL);
    AddInternalSymbol(&P->objsyms, "__clkmode_con", SYM_CONSTANT, AstInteger(clkreg), NULL);
    return 1;
}

/* calculate frequencies for P2 */
static int
CalcClkFreqP2(Module *P)
{
    // look up in P->objsyms
    Symbol *clkmodesym = P ? FindSymbol(&P->objsyms, "_clkmode") : NULL;
    Symbol *clkfreqsym = P ? FindSymbol(&P->objsyms, "_clkfreq") : NULL;
    Symbol *xtlfreqsym = P ? FindSymbol(&P->objsyms, "_xtlfreq") : NULL;
    Symbol *xinfreqsym = P ? FindSymbol(&P->objsyms, "_xinfreq") : NULL;
    Symbol *errfreqsym = P ? FindSymbol(&P->objsyms, "_errfreq") : NULL;

    double clkfreq;
    double xinfreq = 20000000.0;  // default crystal frequency
    double errtolerance = 100000.0;
    uint32_t clkmode = 0;
    uint32_t finalfreq = 0;
    uint32_t zzzz = 11; // 0b10_11
    uint32_t pppp;
    double error;

    if (IsSpinLang(P->mainLanguage)) {
        clkfreq = 20000000.0; // actually we want RCFAST mode
    } else {
        clkfreq = 160000000.0;
    }
    if (xinfreqsym) {
        if (xtlfreqsym) {
            ERROR(NULL, "Only one of _xtlfreq or _xinfreq may be specified");
            return 0;
        }
        clkfreq = xinfreq = (double)EvalConstSym(xinfreqsym);
        zzzz = 7; // 0b01_11
    } else if (xtlfreqsym) {
        clkfreq = xinfreq = (double)EvalConstSym(xtlfreqsym);
        if (xinfreq >= 16000000.0) {
            zzzz = 11; // 0b10_11
        } else {
            zzzz = 15; // 0b11_11
        }
    }
    if (clkmodesym && clkmodesym->kind == SYM_CONSTANT) {
        if (xinfreqsym || xtlfreqsym) {
            ERROR(NULL, "_xinfreq and _xtlfreq are redundant with _clkmode");
            return 0;
        }
        if (!clkfreqsym) {
            ERROR(NULL, "_clkmode definition requires _clkfreq as well");
            return 0;
        }
        clkmode = EvalConstSym(clkmodesym);
        finalfreq = EvalConstSym(clkfreqsym);
        goto set_symbols;
    } else {
        clkmodesym = 0;
    }
    if (clkfreqsym && clkfreqsym->kind == SYM_CONSTANT) {
        clkfreq = (double)EvalConstSym(clkfreqsym);
    } else {
        clkfreqsym = 0;
    }
    if (errfreqsym) {
        errtolerance = (double)EvalConstSym(errfreqsym);
    }
    
    // figure out clock mode based on frequency
    uint32_t divd;
    double e, post, mult, Fpfd, Fvco, Fout;
    double result_mult = 0;
    double result_Fout = 0;
    uint32_t result_pppp = 0, result_divd = 0;
    error = 1e9;
    for (pppp = 0; pppp <= 15; pppp++) {
        if (pppp == 0) {
            post = 1.0;
        } else {
            post = pppp * 2.0;
        }
        for (divd = 64; divd >= 1; --divd) {
            Fpfd = round(xinfreq / (double)divd);
            mult = round(clkfreq * post / Fpfd);
            Fvco = round(Fpfd * mult);
            Fout = round(Fvco / post);
            e = fabs(Fout - clkfreq);
            if ( (e <= error) && (Fpfd >= 250000) && (mult <= 1024) && (Fvco > 99e6) && ((Fvco <= 201e6) || (Fvco <= clkfreq + 1e6)) ) {
                result_divd = divd;
                result_mult = mult;
                result_pppp = (pppp-1) & 15;
                result_Fout = Fout;
                error = e;
            }
        }
    }
    if (error > errtolerance) {
        ERROR(NULL, "Unable to find clock settings for freq %f Hz with input freq %f Hz", clkfreq, xinfreq);
        return 0;
    }
    uint32_t D, M;
    D = result_divd - 1;
    M = ((uint32_t)result_mult) - 1;
    clkmode = zzzz | (result_pppp<<4) | (M<<8) | (D<<18) | (1<<24);

    
    finalfreq = (uint32_t)round(result_Fout);
set_symbols:
    // define built in constants for these
    AddInternalSymbol(&P->objsyms, "__clkfreq_con", SYM_CONSTANT, AstInteger(finalfreq), NULL);
    AddInternalSymbol(&P->objsyms, "__clkmode_con", SYM_CONSTANT, AstInteger(clkmode), NULL);
    
    return 1;
}

/*
 * utility functions to fetch previously calculated clock frequency
 */
int GetClkFreq(Module *P, unsigned int *clkfreqptr, unsigned int *clkmodeptr)
{
    Symbol *freqsym, *modesym;

    freqsym = P ? FindSymbol(&P->objsyms, "__clkfreq_con") : NULL;
    modesym = P ? FindSymbol(&P->objsyms, "__clkmode_con") : NULL;

    if (!freqsym || !modesym) {
        return 0;
    }
    if (freqsym->kind != SYM_CONSTANT) {
        return 0;
    }
    if (modesym->kind != SYM_CONSTANT) {
        return 0;
    }
    *clkfreqptr = EvalConstSym(freqsym);
    *clkmodeptr = EvalConstSym(modesym);
    return 1;
}

//
// Declare a __default_baud__ symbol for the baud rate
//
void DeclareBaud(Module *P)
{
    int baud = 0;  // undefined
    Symbol *sym;
    if (gl_debug && IsSpinLang(P->mainLanguage)) {
        sym = FindSymbol(&P->objsyms, "debug_baud");
        if (sym && sym->kind == SYM_CONSTANT) {
            baud = EvalConstSym(sym);
        }
    }
    if (baud == 0) {
        baud = gl_default_baud;
        if (baud == 0) {
            baud = (gl_p2) ? 230400 : 115200;
        }
    }
    AddInternalSymbol(&P->objsyms, "__default_baud__", SYM_CONSTANT, AstInteger(baud), NULL);
    AddInternalSymbol(&systemModule->objsyms, "__default_baud__", SYM_CONSTANT, AstInteger(baud), NULL);
}
