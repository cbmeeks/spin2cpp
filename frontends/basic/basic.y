/*
 * Spin compiler parser
 * Copyright (c) 2011-2018 Total Spectrum Software Inc.
 * See the file COPYING for terms of use.
 */

%define api.prefix {basicyy}

%{
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdlib.h>
#include "frontends/common.h"
  
/* Yacc functions */
    void basicyyerror(const char *);
    int basicyylex();

    extern int gl_errors;

    extern AST *last_ast;
    extern AST *CommentedListHolder(AST *); // in spin.y
    
#define YYERROR_VERBOSE 1
#define BASICYYSTYPE AST*

    
AST *GetIORegisterPair(const char *name1, const char *name2)
{
    Symbol *sym = FindSymbol(&basicReservedWords, name1);
    AST *reg1, *reg2;
    if (!sym) {
        ERROR(NULL, "Unknown ioregister %s", name1);
        return NULL;
    }
    reg1 = NewAST(AST_HWREG, NULL, NULL);
    reg2 = NULL;
    
    reg1->d.ptr = sym->val;
    if (name2 && gl_p2) {
        // for now, only use the register pairs on P2 (where it matters)
        // (FIXME? it could also come into play for P1V on FPGA...)
        sym = FindSymbol(&basicReservedWords, name2);
        if (sym) {
            reg2 = NewAST(AST_HWREG, NULL, NULL);
            reg2->d.ptr = sym->val;
        }
    }
    if (reg2) {
        reg1 = NewAST(AST_REGPAIR, reg1, reg2);
    }
    return reg1;
}

AST *GetPinRange(const char *name1, const char *name2, AST *range)
{
    AST *reg = GetIORegisterPair(name1, name2);
    AST *ast = NULL;
    if (reg) {
        ast = NewAST(AST_RANGEREF, reg, range);
    }
    return ast;
}

AST *
DeclareGlobalBasicVariables(AST *ast)
{
    AST *idlist, *typ;
    AST *ident;

    if (!ast) return ast;
    idlist = ast->left;
    typ = ast->right;
    while (idlist) {
        ident = idlist->left;
        MaybeDeclareGlobal(current, ident, typ);
        idlist = idlist->right;
    }
    return NULL;
}

AST *BASICArrayRef(AST *id, AST *expr)
{
    AST *ast;

    // BASIC arrays start at 1
    expr = AstOperator('-', expr, AstInteger(1));
    ast = NewAST(AST_ARRAYREF, id, expr);
    return ast;
}

AST *AstCharItem(int c)
{
    AST *expr = NewAST(AST_CHAR, AstInteger(c), NULL);
    return NewAST(AST_EXPRLIST, expr, NULL);
}

%}

%pure-parser

%token BAS_IDENTIFIER "identifier"
%token BAS_INTEGER    "integer number"
%token BAS_FLOAT      "number"
%token BAS_STRING     "literal string"
%token BAS_EOLN       "end of line"
%token BAS_EOF        "end of file"
%token BAS_TYPENAME   "class name"

/* keywords */
%token BAS_ABS        "abs"
%token BAS_AND        "and"
%token BAS_ANY        "any"
%token BAS_AS         "as"
%token BAS_ASC        "asc"
%token BAS_ASM        "asm"
%token BAS_BYTE       "byte"
%token BAS_CASE       "case"
%token BAS_CLASS      "class"
%token BAS_CONST      "const"
%token BAS_CONTINUE   "continue"
%token BAS_DECLARE    "declare"
%token BAS_DIM        "dim"
%token BAS_DIRECTION  "direction"
%token BAS_DO         "do"
%token BAS_DOUBLE     "double"
%token BAS_ELSE       "else"
%token BAS_END        "end"
%token BAS_ENDIF      "endif"
%token BAS_ENUM       "enum"
%token BAS_EXIT       "exit"
%token BAS_FOR        "for"
%token BAS_FROM       "from"
%token BAS_FUNCTION   "function"
%token BAS_GET        "get"
%token BAS_GOTO       "goto"
%token BAS_IF         "if"
%token BAS_INPUT      "input"
%token BAS_INTEGER_KW "integer"
%token BAS_LET        "let"
%token BAS_LONG       "long"
%token BAS_LOOP       "loop"
%token BAS_MOD        "mod"
%token BAS_NEXT       "next"
%token BAS_NOT        "not"
%token BAS_OPEN       "open"
%token BAS_OR         "or"
%token BAS_OUTPUT     "output"
%token BAS_POINTER    "pointer"
%token BAS_PRINT      "print"
%token BAS_PROGRAM    "program"
%token BAS_PUT        "put"
%token BAS_RETURN     "return"
%token BAS_SELECT     "select"
%token BAS_SHARED     "shared"
%token BAS_SHORT      "short"
%token BAS_SINGLE     "single"
%token BAS_SQRT       "sqrt"
%token BAS_STEP       "step"
%token BAS_STRING_KW  "string"
%token BAS_STRUCT     "struct"
%token BAS_SUB        "sub"
%token BAS_THEN       "then"
%token BAS_TO         "to"
%token BAS_UBYTE      "ubyte"
%token BAS_UINTEGER   "uinteger"
%token BAS_ULONG      "ulong"
%token BAS_USHORT     "ushort"
%token BAS_UNTIL      "until"
%token BAS_VAR        "var"
%token BAS_WEND       "wend"
%token BAS_WITH       "with"
%token BAS_WHILE      "while"
%token BAS_XOR        "xor"
%token BAS_LE         "<="
%token BAS_GE         ">="
%token BAS_NE         "<>"
%token BAS_SHL        "<<"
%token BAS_SHR        ">>"
%token BAS_NEGATE     "-"

%left BAS_OR BAS_XOR
%left BAS_AND
%left '<' '>' BAS_LE BAS_GE BAS_NE '='
%left '-' '+'
%left '*' '/' BAS_MOD
%left BAS_SHL BAS_SHR
%left BAS_NEGATE
%left '@'

%%
toplist:
 /* empty */
 | toplist1
;

toplist1:
 topstatement
 | toplist1 topstatement
;

eoln:
  BAS_EOLN
  ;

topstatement:
  statement
    {
        AST *stmtholder = NewCommentedStatement($1);
        current->body = AddToList(current->body, stmtholder);
    }
  | topdecl
;

pinrange:
  expr
    { $$ = NewAST(AST_RANGE, $1, NULL); }
  | expr ',' expr
    { $$ = NewAST(AST_RANGE, $1, $3); }
;

nonemptystatement:
  BAS_IDENTIFIER ':'
    { $$ = NewAST(AST_LABEL, $1, NULL); }
  | BAS_IDENTIFIER '=' expr eoln
    { $$ = AstAssign($1, $3); }
  | BAS_IDENTIFIER '(' expr ')' '=' expr eoln
    { $$ = AstAssign(BASICArrayRef($1, $3), $6); }
  | BAS_OUTPUT '(' pinrange ')' '=' expr eoln
    {
        $$ = AstAssign(GetPinRange("outa", "outb", $3), $6);
    }
  | BAS_DIRECTION '(' pinrange ')' '=' expr eoln
    {
        $$ = AstAssign(GetPinRange("dira", "dirb", $3), $6);
    }
  | BAS_DIRECTION '(' pinrange ')' '=' BAS_INPUT eoln
    {
        $$ = AstAssign(GetPinRange("dira", "dirb", $3), AstInteger(0));
    }
  | BAS_DIRECTION '(' pinrange ')' '=' BAS_OUTPUT eoln
    {
        $$ = AstAssign(GetPinRange("dira", "dirb", $3), AstInteger(-1));
    }
  | BAS_LET BAS_IDENTIFIER '=' expr eoln
    { MaybeDeclareGlobal(current, $2, InferTypeFromName($2));
      $$ = AstAssign($2, $4); }
  | BAS_IDENTIFIER '(' optexprlist ')' eoln
    { $$ = NewAST(AST_FUNCCALL, $1, $3); }
  | BAS_IDENTIFIER '.' BAS_IDENTIFIER '(' optexprlist ')' eoln
    { $$ = NewAST(AST_FUNCCALL, NewAST(AST_METHODREF, $1, $3), $5); }
  | BAS_IDENTIFIER optexprlist eoln
    { $$ = NewAST(AST_FUNCCALL, $1, $2); }
  | BAS_IDENTIFIER eoln
    { $$ = NewAST(AST_FUNCCALL, $1, NULL); }
  | BAS_RETURN eoln
    { $$ = AstReturn(NULL, $1); }
  | BAS_RETURN expr eoln
    { $$ = AstReturn($2, $1); }
  | BAS_GOTO BAS_IDENTIFIER eoln
    { $$ = NewAST(AST_GOTO, $2, NULL); }
  | BAS_PRINT printlist
    { $$ = NewAST(AST_PRINT, $2, NULL); }
  | BAS_PUT expr
    { $$ = NewAST(AST_PRINT,
                  NewAST(AST_EXPRLIST, NewAST(AST_HERE, $2, NULL), NULL),
                  NULL); }
  | ifstmt
    { $$ = $1; }
  | whilestmt
    { $$ = $1; }
  | doloopstmt
    { $$ = $1; }
  | forstmt
    { $$ = $1; }
;

statement:
    nonemptystatement
    { $$ = $1; }
  | eoln
    { $$ = NULL; }
;

printitem:
  expr
    { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
  | '\\' expr
    { $$ = NewAST(AST_EXPRLIST, NewAST(AST_CHAR, $2, NULL), NULL); }
;

rawprintlist:
  | printitem
  { $$ = $1; }
  | rawprintlist ';' printitem
  { $$ = AddToList($1, $3); }
  | rawprintlist ',' printitem
  { $$ = AddToList(AddToList($1, AstCharItem('\t')), $3); }
;
printlist:
  eoln
    { $$ = AstCharItem('\n'); }
  | rawprintlist eoln
    { $$ = AddToList($1, AstCharItem('\n')); }
  | rawprintlist ',' eoln
    { $$ = AddToList($1, AstCharItem('\t')); }
  | rawprintlist ';' eoln
    { $$ = $1; }
;

ifstmt:
  BAS_IF expr BAS_THEN eoln thenelseblock
    { $$ = NewCommentedAST(AST_IF, $2, $5, $1); }
  | BAS_IF expr nonemptystatement
    {
        AST *stmtlist = NewCommentedStatement($3);
        AST *elseblock = NewAST(AST_THENELSE, stmtlist, NULL);
        $$ = NewCommentedAST(AST_IF, $2, elseblock, $1);
    }
;

thenelseblock:
  statementlist endif
    { $$ = NewAST(AST_THENELSE, $1, NULL); }
  | statementlist BAS_ELSE eoln statementlist endif
    { $$ = NewAST(AST_THENELSE, $1, $4); }
  | statementlist BAS_ELSE nonemptystatement
    { $$ = NewAST(AST_THENELSE, $1, NewCommentedStatement($3)); }
;

endif:
  BAS_END eoln
  | BAS_END BAS_IF eoln
  | BAS_ENDIF eoln
;

whilestmt:
  BAS_WHILE expr eoln statementlist endwhile
    { AST *body = CheckYield($4);
      $$ = NewCommentedAST(AST_WHILE, $2, body, $1);
    }
;

endwhile:
  BAS_WEND eoln
  | BAS_END BAS_WHILE eoln
  | BAS_END eoln
  ;

doloopstmt:
  BAS_DO eoln optstatementlist BAS_LOOP eoln
    { AST *body = CheckYield($3);
      AST *one = AstInteger(1);
      $$ = NewCommentedAST(AST_WHILE, one, body, $1);
    }
  | BAS_DO BAS_WHILE expr eoln optstatementlist BAS_LOOP eoln
    { AST *body = CheckYield($5);
      AST *cond = $3;
      $$ = NewCommentedAST(AST_WHILE, cond, body, $1);
    }
  | BAS_DO BAS_UNTIL expr eoln optstatementlist BAS_LOOP eoln
    { AST *body = CheckYield($5);
      AST *cond = AstOperator(K_BOOL_NOT, NULL, $3);
      $$ = NewCommentedAST(AST_WHILE, cond, body, $1);
    }
  | BAS_DO eoln optstatementlist BAS_LOOP BAS_WHILE expr eoln
    { $$ = NewCommentedAST(AST_DOWHILE, $6, CheckYield($3), $1); }
  | BAS_DO eoln optstatementlist BAS_LOOP BAS_UNTIL expr eoln
    { $$ = NewCommentedAST(AST_DOWHILE, AstOperator(K_BOOL_NOT, NULL, $6), CheckYield($3), $1); }
  ;

forstmt:
  BAS_FOR BAS_IDENTIFIER '=' expr BAS_TO expr optstep eoln statementlist endfor
    {
      AST *from, *to, *step;
      AST *ident = $2;
      AST *closeident = $10;
      MaybeDeclareGlobal(current, ident, InferTypeFromName(ident));
      step = NewAST(AST_STEP, $7, $9);
      to = NewAST(AST_TO, $6, step);
      from = NewAST(AST_FROM, $4, to);
      $$ = NewCommentedAST(AST_COUNTREPEAT, $2, from, $1);
      // validate the "next i"
      if (closeident && !AstMatch(ident, closeident)) {
          fprintf(stderr, "%s:%d: error: ", current->L.fileName, current->L.lineCounter);
          fprintf(stderr, "Wrong variable in next: expected %s, saw %s\n", ident->d.string, closeident->d.string);
      }
    }
;

endfor:
  BAS_NEXT eoln
    { $$ = NULL; }
  | BAS_NEXT BAS_IDENTIFIER eoln
    { $$ = $2; }
;

optstep:
  /* nothing */
    { $$ = AstInteger(1); }
  | BAS_STEP expr
    { $$ = $2; }
;

optstatementlist:
  /* nothing */
    { $$ = NULL; }
  | statementlist
    { $$ = $1; }
;

statementlist:
  statement
    { $$ = NewCommentedStatement($1); }
  | dimension
    { $$ = NewCommentedStatement($1); }
  | statementlist statement
    { $$ = AddToList($1, NewCommentedStatement($2)); }
  | statementlist dimension
    { $$ = AddToList($1, NewCommentedStatement($2)); }
  ;

paramdecl:
  /* empty */
    { $$ = NULL; }
  | paramdecl1
    { $$ = $1; }
;
paramdecl1:
  paramitem
    { $$ = NewAST(AST_LISTHOLDER, $1, NULL); }
  | paramdecl1 ',' paramitem
  { $$ = AddToList($1, NewAST(AST_LISTHOLDER, $3, NULL)); }
  ;

paramitem:
  identifier
    { $$ = NewAST(AST_DECLARE_VAR, InferTypeFromName($1), $1); }
  | identifier '=' expr
    { $$ = NewAST(AST_DECLARE_VAR, InferTypeFromName($1), AstAssign($1, $3)); }
  | identifier BAS_AS typename
    { $$ = NewAST(AST_DECLARE_VAR, $3, $1); }
  | identifier '=' expr BAS_AS typename
    { $$ = NewAST(AST_DECLARE_VAR, $5, AstAssign($1, $3)); }
;

exprlist:
  expritem
 | exprlist ',' expritem
   { $$ = AddToList($1, $3); }
 ;

optexprlist:
  /* empty */
    {  $$ = NULL; }
  | exprlist
    { $$ = $1; }
;

expritem:
  expr
   { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
;

expr:
  BAS_INTEGER
    { $$ = $1; }
  | BAS_FLOAT
    { $$ = $1; }
  | BAS_STRING
    { $$ = NewAST(AST_STRINGPTR,
                  NewAST(AST_EXPRLIST, $1, NULL), NULL); }
  | lhs
    { $$ = $1; }
  | expr '+' expr
    { $$ = AstOperator('+', $1, $3); }
  | expr '-' expr
    { $$ = AstOperator('-', $1, $3); }
  | expr '*' expr
    { $$ = AstOperator('*', $1, $3); }
  | expr '/' expr
    { $$ = AstOperator('/', $1, $3); }
  | expr BAS_MOD expr
    { $$ = AstOperator(K_MODULUS, $1, $3); }
  | expr '=' expr
    { $$ = AstOperator(K_EQ, $1, $3); }
  | expr BAS_NE expr
    { $$ = AstOperator(K_NE, $1, $3); }
  | expr BAS_LE expr
    { $$ = AstOperator(K_LE, $1, $3); }
  | expr BAS_GE expr
    { $$ = AstOperator(K_GE, $1, $3); }
  | expr '<' expr
    { $$ = AstOperator('<', $1, $3); }
  | expr '>' expr
    { $$ = AstOperator('>', $1, $3); }
  | expr BAS_AND expr
    { $$ = AstOperator('&', $1, $3); }
  | expr BAS_OR expr
    { $$ = AstOperator('|', $1, $3); }
  | expr BAS_SHL expr
    { $$ = AstOperator(K_SHL, $1, $3); }
  | expr BAS_SHR expr
    { $$ = AstOperator(K_SAR, $1, $3); }
  | expr BAS_XOR expr
    { $$ = AstOperator('^', $1, $3); }
  | '-' expr %prec BAS_NEGATE
    { $$ = AstOperator(K_NEGATE, NULL, $2); }
  | BAS_NOT expr
    { $$ = AstOperator(K_BIT_NOT, NULL, $2); } 
  | BAS_IDENTIFIER '(' optexprlist ')'
    { $$ = NewAST(AST_FUNCCALL, $1, $3); }
  | BAS_IDENTIFIER '.' BAS_IDENTIFIER '(' optexprlist ')'
    { 
        $$ = NewAST(AST_FUNCCALL, NewAST(AST_METHODREF, $1, $3), $5);
    }
  | BAS_ABS '(' expr ')'
    { $$ = AstOperator(K_ABS, NULL, $3); }  
  | BAS_ASC '(' expr ')'
    { $$ = AstOperator(K_ASC, NULL, $3); }  
  | '@' expr
    { $$ = NewAST(AST_ADDROF, $2, NULL); }  
  | '(' expr ')'
    { $$ = $2; }
  | BAS_INPUT '(' pinrange ')'
    { $$ = GetPinRange("ina", "inb", $3); }
  | BAS_OUTPUT '(' pinrange ')'
    { $$ = GetPinRange("outa", "outb", $3); }
  | BAS_DIRECTION '(' pinrange ')'
    { $$ = GetPinRange("dira", "dirb", $3); }
;

lhs: identifier
    { $$ = $1; }
;

identifier:
  BAS_IDENTIFIER
    { $$ = $1; }
;

topdecl:
  subdecl
  | funcdecl
  | classdecl
  | dimension
    { $$ = DeclareGlobalBasicVariables($1); }
  | constdecl
  ;

constdecl:
  BAS_CONST BAS_IDENTIFIER '=' expr
  {
      AST *decl;
      decl = AstAssign($2, $4);
      decl = CommentedListHolder(decl);
      $$ = current->conblock = AddToList(current->conblock, decl);
  }
;

subdecl:
  BAS_SUB BAS_IDENTIFIER '(' paramdecl ')' eoln subbody
  {
    AST *funcdecl = NewAST(AST_FUNCDECL, $2, NULL);
    AST *funcvars = NewAST(AST_FUNCVARS, $4, NULL);
    AST *funcdef = NewAST(AST_FUNCDEF, funcdecl, funcvars);
    DeclareFunction(ast_type_void, 1, funcdef, $7, NULL, $1);
  }
  | BAS_SUB BAS_IDENTIFIER paramdecl eoln subbody
  {
    AST *funcdecl = NewAST(AST_FUNCDECL, $2, NULL);
    AST *funcvars = NewAST(AST_FUNCVARS, $3, NULL);
    AST *funcdef = NewAST(AST_FUNCDEF, funcdecl, funcvars);
    DeclareFunction(ast_type_void, 1, funcdef, $5, NULL, $1);
  }
  ;

funcdecl:
  BAS_FUNCTION BAS_IDENTIFIER '(' paramdecl ')' eoln funcbody
  {
    AST *name = $2;
    AST *funcdecl = NewAST(AST_FUNCDECL, name, NULL);
    AST *funcvars = NewAST(AST_FUNCVARS, $4, NULL);
    AST *funcdef = NewAST(AST_FUNCDEF, funcdecl, funcvars);
    AST *rettype = InferTypeFromName(name);
    DeclareFunction(rettype, 1, funcdef, $7, NULL, $1);
  }
  | BAS_FUNCTION BAS_IDENTIFIER '(' paramdecl ')' BAS_AS typename eoln funcbody
  {
    AST *name = $2;
    AST *funcdecl = NewAST(AST_FUNCDECL, name, NULL);
    AST *funcvars = NewAST(AST_FUNCVARS, $4, NULL);
    AST *funcdef = NewAST(AST_FUNCDEF, funcdecl, funcvars);
    AST *rettype = $7;
    DeclareFunction(rettype, 1, funcdef, $9, NULL, $1);
  }
  ;

subbody:
  statementlist endsub
  { $$ = $1; }
  ;

endsub:
  BAS_END eoln
  | BAS_END BAS_SUB eoln
;

funcbody:
  statementlist endfunc
  { $$ = $1; }
  ;

endfunc:
  BAS_END eoln
  | BAS_END BAS_FUNCTION eoln
;

classdecl:
  BAS_CLASS BAS_IDENTIFIER BAS_FROM BAS_STRING eoln
    {
        AST *newobj = NewAbstractObject( $2, $4 );
        DeclareObjects(newobj);
        current->objblock = AddToList(current->objblock, newobj);
        $$ = NULL;
    }
  ;

dimension:
  BAS_DIM identlist BAS_AS typename
    { $$ = NewAST(AST_DECLARE_VAR, $2, $4); }
  | BAS_DIM BAS_AS typename identlist
    { $$ = NewAST(AST_DECLARE_VAR, $4, $3); }
  | BAS_DIM identlist
    { $$ = NewAST(AST_DECLARE_VAR, $2, NULL); } 
  ;

identdecl:
  identifier
    { $$ = $1; }
  | identifier '(' expr ')'
    { $$ = NewAST(AST_ARRAYDECL, $1, $3); }
;

identlist:
  identdecl
  { $$ = NewAST(AST_LISTHOLDER, $1, NULL); }
  | identlist ',' identdecl
  { $$ = AddToList($1, NewAST(AST_LISTHOLDER, $3, NULL)); }
  ;

typename:
  basetypename
    { $$ = $1; }
  | basetypename BAS_POINTER
    { $$ = NewAST(AST_PTRTYPE, $1, NULL); }
  | basetypename BAS_CONST BAS_POINTER
    { $$ = NewAST(AST_MODIFIER_CONST, NewAST(AST_PTRTYPE, $1, NULL), NULL); }
  ;

basetypename:
  BAS_UBYTE
    { $$ = ast_type_byte; }
  | BAS_USHORT
    { $$ = ast_type_word; }
  | BAS_LONG
    { $$ = ast_type_long; }
  | BAS_BYTE
    { $$ = ast_type_signed_byte; }
  | BAS_SHORT
    { $$ = ast_type_signed_word; }
  | BAS_ULONG
    { $$ = ast_type_unsigned_long; }
  | BAS_INTEGER_KW
    { $$ = ast_type_long; }
  | BAS_UINTEGER
    { $$ = ast_type_unsigned_long; }
  | BAS_SINGLE
    { $$ = ast_type_float; }
  | BAS_DOUBLE
    { $$ = ast_type_float; }
  | BAS_STRING_KW
    { $$ = ast_type_string; }
  | BAS_ANY
    { $$ = ast_type_generic; }
  | BAS_TYPENAME
    { $$ = $1; }
  | BAS_CONST basetypename
    { $$ = NewAST(AST_MODIFIER_CONST, $2, NULL); }
  | BAS_CLASS BAS_FROM BAS_STRING
    {
        AST *tempnam = NewAST(AST_IDENTIFIER, NULL, NULL);
        const char *name = NewTemporaryVariable("_class_");
        AST *newobj;
        Symbol *sym;

        tempnam->d.string = name;
        newobj = NewAbstractObject( tempnam, $3 );        
        DeclareObjects(newobj);
        current->objblock = AddToList(current->objblock, newobj);
        sym = FindSymbol(&current->objsyms, tempnam->d.string);
        if (!sym || sym->type != SYM_OBJECT) {
            ERROR(NULL, "internal error in type check");
            newobj = NULL;
        } else {
            newobj = (AST *)sym->val;
        }
        $$ = newobj;
    }
;

%%
void
basicyyerror(const char *msg)
{
    extern int saved_basicyychar;
    int yychar = saved_basicyychar;
    
    ERRORHEADER(current->L.fileName, current->L.lineCounter);

    // massage bison's error messages to make them easier to understand
    while (*msg) {
        // say which identifier was unexpected
        if (!strncmp(msg, "unexpected identifier", strlen("unexpected identifier")) && last_ast && last_ast->kind == AST_IDENTIFIER) {
            fprintf(stderr, "unexpected identifier `%s'", last_ast->d.string);
            msg += strlen("unexpected identifier");
        }
        // if we get a stray character in source, sometimes bison tries to treat it as a token for
        // error purposes, resulting in $undefined as the token
        else if (!strncmp(msg, "$undefined", strlen("$undefined")) && yychar >= ' ' && yychar < 127) {
            fprintf(stderr, "%c", yychar);
            msg += strlen("$undefined");
        }
        else {
            fprintf(stderr, "%c", *msg);
            msg++;
        }
    }
    fprintf(stderr, "\n");     
    gl_errors++;
}
