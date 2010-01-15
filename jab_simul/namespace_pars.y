%{
#include <stdio.h>
#include "namespace_type.h"

  void yyerror(char* x) {
    printf("%s\n",x );
  }

  char* tmpbuf=NULL;
  int tmpbufsize;
  int yynumid;

%}

%token NOTHING_SPECIAL
%token BEGIN_EXPR
%token END_EXPR
%token OP_PLUS
%token OP_MINUS
%token OP_MUL
%token OP_DIV
%token OP_MOD
%token NUMBER
%token INDEX
%token RND
%token LNAWIAS
%token PNAWIAS
%token VARIABLE

%% /* Rules */

expr_list:        expr_list expr | expr;

expr:             NOTHING_SPECIAL {*tmpbuf++=$1.c;*tmpbuf=0;} 
                | BEGIN_EXPR math_expr END_EXPR {
		  char fmt[30];
		  sprintf(fmt,"%%%s",$3.s);
                  tmpbuf+=sprintf(tmpbuf,fmt,$2.i);
                  };

math_expr:        math_addexpr;

math_addexpr:     math_addexpr OP_PLUS math_mulexpr {$$.i=$1.i+$3.i;}| 
                  math_addexpr OP_MINUS math_mulexpr {$$.i=$1.i-$3.i;}|
                  math_mulexpr;

math_mulexpr:     math_mulexpr OP_MUL math_atom {$$.i=$1.i*$3.i;}|
                  math_mulexpr OP_DIV math_atom {$$.i=$1.i/$3.i;}|
                  math_mulexpr OP_MOD math_atom {$$.i=$1.i%$3.i;}|
                  math_atom;

math_atom:        NUMBER | VARIABLE | RND | INDEX | LNAWIAS math_expr PNAWIAS {$$=$2;};
