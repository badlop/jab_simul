#ifndef __jab_simul_roster_h__
#define __jab_simul_roster_h__

extern int glob_roster_count;
// informacje o grupach
typedef
struct group_item_struct {
  pool p;
  CString name;
  BOOL exp:1;
  BOOL hid:1;
  struct group_item_struct* next;
} _group_item,*group_item;

// lista grup
typedef slist CGroupArr;

typedef
struct priv_roster_item_struct {
  pool p;
  CString DisplayName;
  CString NickName;
  CString FirstName;
  CString LastName;
  CString SMS;
  CString Cyfra;
  CString Speak;	
  CString JID;
  CString PresenceText;
  CGroupArr Groups;
  unsigned short Ignored:1;
  unsigned short Flash:1;
  unsigned short SpeakPres:1;
  struct priv_roster_item_struct* next;
} _priv_roster_item,*priv_roster_item;

typedef
struct roster_item_struct {
  pool p;
  char* jid;
  char* nick;
#define SUBS_NONE 0
#define SUBS_TO   1
#define SUBS_FROM 2
#define SUBS_BOTH 3
  unsigned short subscribe:2;
  priv_roster_item priv;  /* Wskaznik na strukture z danymi niestandardowymi */
  short available;
} _roster_item,*roster_item;

roster_item roster_item_create(char* name,char* nick,unsigned int subscribe,priv_roster_item priv_list);
void roster_item_delete(roster_item);
void roster_hash_from_xml(user_properities user,xmlnode xml,int delete);
void roster_hash_delete(user_properities user);

group_item group_item_create(char* name,int exp);
void group_item_delete(group_item);

xmlnode priv_roster_list2xml(user_properities user);
xmlnode priv_roster_numextcontacts2xml(user_properities user);

void priv_roster_from_xml(user_properities user,xmlnode xml);
void priv_roster_delete(user_properities user);

#endif
