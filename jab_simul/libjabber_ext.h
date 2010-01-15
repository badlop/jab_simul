#ifndef __libjabber_ext_h__
#define __libjabber_ext_h__

//#include <lib/lib.h>

/* xmlnode.c */
typedef
struct tag_iter_struct {
  xmlnode xml;
  xmlnode cur;
  char* tag_name;
} s_tag_iter,*tag_iter;

void tag_iter_init(tag_iter iter,xmlnode xml,char* tag_name);
xmlnode tag_iter_get_next(tag_iter iter);
xmlnode  xmlnode_get_nth_tag(xmlnode node,int num);

/* xhash.c */
int fit_prime(int size);
xht xhash_resize(xht h, int size);
void *xhash_get_nth(xht h, int num);

#endif
