#include <lib/lib.h>
#include "libjabber_ext.h"

/* rozszerzenie xmlnode.c */
/* Filip Sielimowicz */

void tag_iter_init(tag_iter iter,xmlnode xml,char* tag_name) {
  iter->xml=xml;
  iter->tag_name=tag_name;
  iter->cur=xmlnode_get_firstchild(iter->xml);
}

xmlnode tag_iter_get_next(tag_iter iter) {
  xmlnode cur;
  for (cur=iter->cur;cur;cur=xmlnode_get_nextsibling(cur))
    if ((xmlnode_get_type(cur) == NTYPE_TAG) && 
	(!iter->tag_name || !j_strcmp(xmlnode_get_name(cur),iter->tag_name))) break;
  if (cur) iter->cur=xmlnode_get_nextsibling(cur); else iter->cur=NULL;
  return cur;
}

xmlnode xmlnode_get_nth_tag(xmlnode node,int num) {
  xmlnode cur;
  int count;
  for (count=0,cur=xmlnode_get_firstchild(node);cur && count<num;cur=xmlnode_get_nextsibling(cur)) {
    if(xmlnode_get_type(cur) != NTYPE_TAG) continue;
    count++;
  }
  return cur;
}

/* rozszerzenie xhash.c */

void *xhash_get_nth(xht h, int num) {
  int i,j;
  xhn n;
  
  if(h == NULL)
    return NULL;
  
  for(i = 0,j = 0; i < h->prime; i++)
    for(n = &h->zen[i]; n != NULL; n = n->next)
      if(n->key != NULL && n->val != NULL) {
	if (j==num) return n->val;
	else j++;
      }
  return NULL;
}

/* Wyciete z php */
/* Generated on an Octa-ALPHA 300MHz CPU & 2.5GB RAM monster */
static unsigned int PrimeNumbers[] =
	{5, 11, 19, 53, 107, 223, 463, 983, 1979, 3907, 7963, 16229, 32531, 65407, 130987, 262237, 524521, 1048793, 2097397, 4194103, 8388857, 16777447, 33554201, 67108961, 134217487, 268435697, 536870683, 1073741621, 2147483399};
static unsigned int nNumPrimeNumbers = sizeof(PrimeNumbers) / sizeof(uint);
/* Koniec wycinki z php */

int fit_prime(int size) {
  unsigned int i;
  for (i=0;i<nNumPrimeNumbers && PrimeNumbers[i]<size;i++);
  return PrimeNumbers[i];
}

static void copy_node(xht h, const char *key, void *val, void *arg) {
  xht newh=(xht)arg;
  xhash_put(newh,key,val);
}

xht xhash_resize(xht h, int size) {
  int new_prime=fit_prime(size);
  if (new_prime!=h->prime) {
    xht newh=xhash_new(new_prime);
    xhash_walk(h,copy_node,newh);
    xhash_free(h);
    return newh;
  }
  else return h;
}
