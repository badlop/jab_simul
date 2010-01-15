%{
#include <string.h>
#include "namespace_type.h"
#include "y.tab.h"

static YY_BUFFER_STATE state;
extern char* tmpbuf;
extern int tmpbufsize;

extern int yynumid;

int nms_init(char* string, char* buffer,int buffsize, int par_numid) {
 state=yy_scan_string(string);
 tmpbuf=buffer;
 tmpbufsize=buffsize;
 yynumid=par_numid;
 return 0;
}

int nms_finito() {
 yy_delete_buffer(state);
 return 0;
}

%}

%x ST_EXPRESSION

%option noyywrap
%option yylineno

%%

<INITIAL>\%\( {
	BEGIN(ST_EXPRESSION);
        return BEGIN_EXPR;
}

<INITIAL>. {yylval.c=yytext[0];
            return NOTHING_SPECIAL;
}

<INITIAL><<EOF>> {
       nms_finito();
       return 0;
}


<ST_EXPRESSION>\( {
       return LNAWIAS;
}

<ST_EXPRESSION>\) {
       return PNAWIAS;
}

<ST_EXPRESSION>[0-9]* {
       yylval.i=atoi(yytext);
       return NUMBER;
}

<ST_EXPRESSION>(NUM|num|ID|id|POS|pos) {
       yylval.i=yynumid;
       return INDEX;
}

<ST_EXPRESSION>(RND|rnd) {
       yylval.i=random();
       return RND;
}

<ST_EXPRESSION>\+ {
       return OP_PLUS;
}

<ST_EXPRESSION>- {
       return OP_MINUS;
}

<ST_EXPRESSION>\* {
       return OP_MUL;
}

<ST_EXPRESSION>\/ {
       return OP_DIV;
}

<ST_EXPRESSION>% {
       return OP_MOD;
}

<ST_EXPRESSION>\ \n {}

<ST_EXPRESSION>:[^\)]*\) {
       BEGIN(INITIAL);
       strcpy(yylval.s,&yytext[1]);
       yylval.s[strlen(yylval.s)-1]=0;
       return END_EXPR;
}
