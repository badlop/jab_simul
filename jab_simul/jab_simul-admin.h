#ifndef __JPOLLD_ADMIN_H__
#define __JPOLLD_ADMIN_H__

int compare_ip(int);

int IsSpecialUser(const char* user);

#define SpUs_log(c, filename, fmt, x...) \
 {if (c->admin_flags & FL_SPUS) {\
  FILE* f=fopen(filename, "a+");\
  if (f) {fprintf(f, fmt, ## x);fclose(f);}}}

#define SpUs_logwriteuser(c, fmt, x...) \
 SpUs_log(c, main_config.fullxmluserlogpath, fmt, ## x)

#define SpUs_logreaduser(c, fmt, x...) \
 SpUs_log(c, main_config.fullxmluserlogpath, fmt, ## x)

#define SpUs_logwritemaster(c, fmt, x...) \
 SpUs_log(c, main_config.fullxmlmasterlogpath, fmt, ## x)

#define SpUs_logreadmaster(c, fmt, x...) \
 SpUs_log(c, main_config.fullxmlmasterlogpath, fmt, ## x)

#endif
