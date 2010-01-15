#include <stdarg.h>
#include "jab_simul-main.h"

static inline int compare_cipher(int what,int filter) {
  //filter==-1 <=> *
  return (what==filter)||(filter==-1);
}

int compare_ip(int cip) {return 0;}

int IsSpecialUser(const char* user) {
  return !strcmp(user,main_config.fullxmlscannedloguser);
}
