#ifndef __NMSTYPE_H__
#define __NMSTYPE_H__

typedef struct s_nmsval {
  char s[200];
  int i;
  char c;
} s_nmsval;

extern s_nmsval yylval;

#undef YYSTYPE
#define YYSTYPE s_nmsval

#endif
