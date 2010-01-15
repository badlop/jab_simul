#include "jab_simul-main.h"
#include "jab_simul-roster.h"
char* makeJCOM_ID(user_properities,char*,int);

int glob_roster_count=0;

int xmlnode_count_tags(xmlnode node) {
  xmlnode cur;
  int count;
  for (count=0,cur=xmlnode_get_firstchild(node);cur;cur=xmlnode_get_nextsibling(cur)) {
    if(xmlnode_get_type(cur) != NTYPE_TAG) continue;
    count++;
  }
  return count;
}

priv_roster_item find_priv_by_jid(priv_roster_item list, char* jid) {
  priv_roster_item cur;
  for (cur=list;cur;cur=cur->next) if (!j_strcmp(cur->JID,jid)) return cur;
  return NULL;
}

roster_item roster_item_create(char* jid,char* nick,unsigned int subscribe,priv_roster_item priv_list) {
  pool p;
  roster_item ri;
  p = pool_new();
  ri = pmalloco(p, sizeof(_roster_item));
  ri->p=p;
  ri->jid=jid?pstrdup(p, jid):"";
  ri->nick=nick?pstrdup(p, nick):ri->jid;
  ri->subscribe=subscribe;
  ri->priv=find_priv_by_jid(priv_list,ri->jid);
  return ri;
}

void roster_item_delete(roster_item ri) {
  pool_free(ri->p);
}

static void rost_it_del(xht h, const char *key, void *val, void* arg) {
  roster_item ri=(roster_item)val;
  roster_item_delete(ri);
}

/* Usuwamy caly roster */
void roster_hash_delete(user_properities user) {
  user_debug(user,DET_DL,"User %s: %s(%p->%p)\n",
	     __FUNCTION__,user->id.user,user,user->roster_hash);
  if (user->roster_hash) {
    user_debug(user,DET_DL,"Items in roster: %i\n",user->roster_count);
    xhash_walk(user->roster_hash,rost_it_del,NULL);
    xhash_free(user->roster_hash);
    user->roster_hash=NULL;
    glob_roster_count-=user->roster_count;
    user->last_roster_count=user->roster_count;
    user->roster_count=0;
    user_debug(user,ROST_DL,"Roster deleted\n");
  }
}

roster_item roster_item_from_xml(user_properities user,xmlnode xml) {
  char* sub=xmlnode_get_attrib(xml,"subscription");
  unsigned int subscr;
  subscr=
    !j_strcasecmp(sub,"from")?SUBS_FROM:
    !j_strcasecmp(sub,"to")?SUBS_TO:
    !j_strcasecmp(sub,"both")?SUBS_BOTH:SUBS_NONE;
  return roster_item_create(xmlnode_get_attrib(xml,"jid"),xmlnode_get_attrib(xml,"nick"),
			    subscr,user->priv_rost.priv_rost_list);
}

/* Wczytujemy z xml'a caly roster */
void roster_hash_from_xml(user_properities user,xmlnode xml,int delete) {
  roster_item ri,oldri;
  xmlnode cur;
  int count=xmlnode_count_tags(xml);
  s_tag_iter item_iter;
  if (delete)
    // Usuwamy poprzedni roster
    roster_hash_delete(user);
  // Tworzymy hasha o dopasowanym rozmiarze (nawet jak jest 0 to trzeba stworzyc jakiegos hasha)
  if (!user->roster_hash) {
    user->roster_hash=xhash_new(fit_prime(count));
    user_debug(user,DET_DL,"User %s: Roster hash created(%p->%p)\n",user->id.user,user,user->roster_hash);
  }
  // i przepisujemy wszystkie itemy (jak juz sa to je zamieniamy).
  //TODO: Trzeba sie dokladniej przyjrzec wszystkim atrybutom
  tag_iter_init(&item_iter,xml,"item");
  while ((cur=tag_iter_get_next(&item_iter))) {
    ri=roster_item_from_xml(user,cur);
    oldri=xhash_get(user->roster_hash,ri->jid);
    if (oldri) {
      //Usuwamy poprzednie dane z hasha, jesli takie siedza
      xhash_zap(user->roster_hash,ri->jid);
      roster_item_delete(oldri);
      user->roster_count--;
      glob_roster_count--;
    }
    // I sprawdzamy, jaka jest operacja - usuwania, czy dodawania.
    if (!j_strcasecmp(xmlnode_get_attrib(cur,"subscription"),"remove")) {
      user_debug(user,ROST_DL,"User %s: Roster item removed\n",user->id.user);
      roster_item_delete(ri);
    } else {
      xhash_put(user->roster_hash,ri->jid,ri);
      user->roster_count++;
      glob_roster_count++;
    }
  }
  user_debug(user,ROST_DL,"User %s: Roster hash(%p) filled with %i items\n",
	     user->id.user,user->roster_hash,user->roster_count);
  if (delete && user->last_roster_count!=-1 && user->last_roster_count!=user->roster_count)
    user_debug(user,ERR_DL,"User %s: Niespojnosc liczby rosterow po ponownym zalogowaniu (%i!=%i) !\n",
	       user->id.user,user->last_roster_count,user->roster_count);
}

xmlnode query_wpmsg_set_header(user_properities user,char* ns_wpmsg,xmlnode* pwpmsg_node) {
  xmlnode iq_node=xmlnode_new_tag("iq");
  debug(PROST_DL,"%s(%p)\n",__FUNCTION__,user);
  if (iq_node) {
    char idbuf[10];
    xmlnode query_node;
    xmlnode_put_attrib(iq_node,"type","set");
    xmlnode_put_attrib(iq_node,"id",makeJCOM_ID(user,idbuf,sizeof(idbuf)));
    query_node=xmlnode_insert_tag(iq_node,"query");
    if (query_node) {
      xmlnode wpmsg_node;
      xmlnode_put_attrib(query_node,"xmlns","jabber:iq:private");      
      wpmsg_node=xmlnode_insert_tag(query_node,"wpmsg");
      if (wpmsg_node) {
	xmlnode_put_attrib(wpmsg_node,"xmlns",ns_wpmsg);
	*pwpmsg_node=wpmsg_node;
      }
    }
  }
  return iq_node;
}

//Zapisujemy kontakty prywatne
xmlnode priv_roster_list2xml(user_properities user) {
  xmlnode wpmsg_node=NULL;
  xmlnode iq_node=query_wpmsg_set_header(user,"wpmsg:extcontacts",&wpmsg_node);
  debug(PROST_DL,"%s(%p)\n",__FUNCTION__,user);
  if (!user->priv_rost.loaded) return NULL;
  if (wpmsg_node) {	
    /* *********************** */
    xmlnode contacts_node;
    xmlnode groups_node;
    xmlnode data_node;
    
    /* CONTACTS BEGIN */
    contacts_node=xmlnode_insert_tag(wpmsg_node,"contacts");
    if (contacts_node) {
      priv_roster_item item;
      user_debug(user,PROST_DL,"Zapisujemy kontakty\n");
      for (item=user->priv_rost.priv_rost_list;item;item=item->next) {
	xmlnode item_node=xmlnode_insert_tag(contacts_node,"item");
	user_debug(user,DET_DL,"%s\n",item->DisplayName);
	if(item->JID) xmlnode_insert_cdata(xmlnode_insert_tag(item_node,"jid"),item->JID,-1);
	xmlnode_insert_cdata(xmlnode_insert_tag(item_node,"nick"),item->NickName,-1);
	if(item->FirstName) xmlnode_insert_cdata(xmlnode_insert_tag(item_node,"firstname"),item->FirstName,-1);
	if(item->LastName) xmlnode_insert_cdata(xmlnode_insert_tag(item_node,"lastname"),item->LastName,-1);
	if(item->DisplayName) xmlnode_insert_cdata(xmlnode_insert_tag(item_node,"displayname"),item->DisplayName,-1);
	if(item->SMS) xmlnode_insert_cdata(xmlnode_insert_tag(item_node,"sms"),item->SMS,-1);
	if(item->Cyfra) xmlnode_insert_cdata(xmlnode_insert_tag(item_node,"cyfra"),item->Cyfra,-1);
	if(item->Speak) xmlnode_insert_cdata(xmlnode_insert_tag(item_node,"speak"),item->Speak,-1);
	xmlnode_insert_cdata(xmlnode_insert_tag(item_node,"flash"),item->Flash?"1":"0",-1);
	xmlnode_insert_cdata(xmlnode_insert_tag(item_node,"speakpres"),item->SpeakPres?"1":"0",-1);
	if(item->PresenceText) xmlnode_insert_cdata(xmlnode_insert_tag(item_node,"prestext"),item->PresenceText,-1);
	if (item->Groups) {
	  xmlnode groups_node=xmlnode_insert_tag(item_node,"groups");
	  slist group;
	  // zapisuje grupy usera
	  for(group=item->Groups; group; group=group->next) {
	    group_item grp = (group_item)group->arg;
	    xmlnode_insert_cdata(xmlnode_insert_tag(groups_node,"groupitem"),grp->name,-1);
	  }
	}
      }	  
    }	
    /* CONTACTS END */
    
    /* GROUPS BEGIN */
    groups_node=xmlnode_insert_tag(wpmsg_node,"groups");
    if (groups_node) {
      group_item group;
      xmlnode groupall_node;
      // zapisuje wszystkie grupy
      for(group=user->priv_rost.Groups; group; group=group->next) {
	xmlnode group_node=xmlnode_insert_tag(groups_node,"group");
	xmlnode_insert_cdata(xmlnode_insert_tag(group_node,"name"),group->name,-1);
	xmlnode_insert_cdata(xmlnode_insert_tag(group_node,"exp"),group->exp?"1":"0",-1);
      }
      groupall_node=xmlnode_insert_tag(groups_node,"groupall");
      xmlnode_insert_cdata(xmlnode_insert_tag(groupall_node,"name"),user->priv_rost.gall->name,-1);
      xmlnode_insert_cdata(xmlnode_insert_tag(groupall_node,"exp"),user->priv_rost.gall->exp?"1":"0",-1);
    }
    
    /* DATA BEGIN */
    data_node=xmlnode_insert_tag(wpmsg_node,"data");
    if (data_node) {
      char buf[20];
      snprintf(buf,sizeof(buf),"%i",user->priv_rost.numextcontacts);
      xmlnode_insert_cdata(data_node,buf,-1);
    }
    /* DATA END */
    /* *********************** */
  }
  return iq_node;
}

//Zapisujemy numer wersji kontaktow prywatnych
xmlnode priv_roster_numextcontacts2xml(user_properities user) {
  xmlnode wpmsg_node=NULL;
  xmlnode iq_node=query_wpmsg_set_header(user,"wpmsg:numextcontacts",&wpmsg_node);
  debug(PROST_DL,"%s(%p)\n",__FUNCTION__,user);
  if (!user->priv_rost.loaded) return NULL;
  if (wpmsg_node) {	
    /* NUMEXTCONTACTS BEGIN */
    xmlnode data_node=xmlnode_insert_tag(wpmsg_node,"data");
    if (data_node) {
      char buf[20];
      snprintf(buf,sizeof(buf),"%i",user->priv_rost.numextcontacts);
      xmlnode_insert_cdata(data_node,buf,-1);
    }
    /* NUMEXTCONTACTS END */
  }
  return iq_node;
}

priv_roster_item priv_roster_item_create
(char* jid,char* displayname,char* nick,char* firstname,char* lastname,
 char* sms, char* cyfra, char* speak, int flash, int speak_pres,char* pres_text) {
  pool p;
  priv_roster_item pri;
  p = pool_new();
  pri = pmalloco(p, sizeof(_priv_roster_item));
  pri->p=p;

  pri->JID = pstrdup(p,jid);
  pri->NickName = pstrdup(p,nick);
  pri->DisplayName = pstrdup(p,displayname);
  if (!pri->DisplayName) pri->DisplayName=pri->NickName;
  pri->FirstName = pstrdup(p,firstname);
  
  pri->SMS = pstrdup(p,sms);
  pri->Cyfra = pstrdup(p,cyfra);
  pri->LastName = pstrdup(p,lastname);
  pri->Speak = pstrdup(p,speak);
  if (!pri->Speak) pri->Speak = "Standard";    
  
  pri->Flash = flash;
  pri->SpeakPres = speak_pres;
  pri->PresenceText = pstrdup(p,pres_text);   

  pri->Groups=NULL;
  pri->next=NULL;
  
  return pri;
}

void priv_roster_item_delete(priv_roster_item it) {
  pool_free(it->p);
}

void privLoadContacts(user_properities user,xmlnode cxml) {
  s_tag_iter contacts_iter;
  xmlnode cur_item;
  priv_roster_item *tail=&user->priv_rost.priv_rost_list;
  tag_iter_init(&contacts_iter,cxml,"item");
  user_debug(user,PROST_DL,"User %s: %s\n",user->id.user,__FUNCTION__);
  
  while ((cur_item=tag_iter_get_next(&contacts_iter))) {
    xmlnode groups;
    priv_roster_item pri=
      priv_roster_item_create(
			      xmlnode_get_tag_data(cur_item,"jid"),
			      xmlnode_get_tag_data(cur_item,"displayname"),
			      xmlnode_get_tag_data(cur_item,"nick"),
			      xmlnode_get_tag_data(cur_item,"firstname"),
			      xmlnode_get_tag_data(cur_item,"lastname"),
			      xmlnode_get_tag_data(cur_item,"sms"),
			      xmlnode_get_tag_data(cur_item,"cyfra"),
			      xmlnode_get_tag_data(cur_item,"speak"),
			      j_strcmp(xmlnode_get_tag_data(cur_item,"flash"),"0")!=0,
			      j_strcmp(xmlnode_get_tag_data(cur_item,"speak_pres"),"0")!=0,
			      xmlnode_get_tag_data(cur_item,"prestext")
			      );
    user_debug(user,PROST_DL,"User %s: priv item created(%s)\n",user->id.user,pri->DisplayName);
    groups=xmlnode_get_tag(cur_item,"groups");
    if (groups) {
      s_tag_iter groups_iter;
      xmlnode cur_group;
      slist* tail=&pri->Groups;
      tag_iter_init(&groups_iter,groups,"groupitem");
      while ((cur_group=tag_iter_get_next(&groups_iter))) {
	group_item git;
	char* gname=xmlnode_get_data(cur_group);
	for (git=user->priv_rost.Groups;git && j_strcmp(git->name,gname);git=git->next);
	if (git) {
	  slist node;
	  if (!user->priv_rost.grpp) user->priv_rost.grpp=pool_new();
	  node=pmalloc(user->priv_rost.grpp,sizeof(_slist));
	  node->arg=git;
	  node->next=NULL;
	  *tail=node;
	  tail=&node->next;
	  user_debug(user,PROST_DL,"User %s: Kontakt %s dodany do grupy %s\n",
		     user->id.user,pri->DisplayName,gname);	  
	} else 
	  user_debug(user,ERR_DL,"User %s: Nieznana nazwa grupy (%s) w kontakcie %s !!!\n",
		     user->id.user,gname,pri->DisplayName);
      }
    }
    *tail=pri;
    tail=&pri->next;
  }
}

void privLoadGroups(user_properities user,xmlnode gxml) {
  s_tag_iter groups_iter;
  xmlnode cur_item;
  xmlnode group_all;
  group_item *tail=&user->priv_rost.Groups;
  char* grpallname;
  int grpallexp;
  tag_iter_init(&groups_iter,gxml,"group");
  user_debug(user,PROST_DL,"User %s: %s\n",user->id.user,__FUNCTION__);
  
  while ((cur_item=tag_iter_get_next(&groups_iter))) {
    group_item gri=group_item_create(xmlnode_get_tag_data(cur_item,"name"),
				     j_strcmp(xmlnode_get_tag_data(cur_item,"exp"),"0")!=0);
    user_debug(user,PROST_DL,"User %s: group item created(%s,%i)\n",
	       user->id.user,gri->name,gri->exp);
    *tail=gri;
    tail=&gri->next;
  }
  group_all=xmlnode_get_tag(gxml,"groupall");
  if (group_all) {
    grpallname = xmlnode_get_tag_data(group_all,"name");
    if (!grpallname) grpallname="Wszyscy";
    grpallexp=j_strcmp(xmlnode_get_tag_data(group_all,"exp"),"0")!=0;
  } else {
    grpallname="Wszyscy";
    grpallexp=1;
  }
  user->priv_rost.gall=group_item_create(grpallname,grpallexp);
  user_debug(user,PROST_DL,"User %s: groupall item created(%s,%i)\n",
	     user->id.user,user->priv_rost.gall->name,user->priv_rost.gall->exp);
}
  
group_item group_item_create(char* name,int exp) {
  pool p;
  group_item gi;
  p = pool_new();
  gi = pmalloco(p, sizeof(_group_item));
  gi->p=p;
  gi->name=pstrdup(p, name);
  gi->exp=exp;
  gi->hid=0;     //Not used yet
  gi->next=NULL;
  debug(DET_DL,"group_item_create(%s,%i)->%p\n",gi->name,gi->exp,gi);
  return gi;
}

void group_item_delete(group_item gi) {
  debug(DET_DL,"%s(%p)\n",__FUNCTION__,gi);
  pool_free(gi->p);
}


/* Wczytujemy z xml'a caly priv roster */

void priv_roster_from_xml(user_properities user,xmlnode xml) {
  xmlnode query=xmlnode_get_tag(xml,"query");
  xmlnode wpmsg=xmlnode_get_tag(query,"wpmsg");
  xmlnode groups=xmlnode_get_tag(wpmsg,"groups");
  xmlnode contacts=xmlnode_get_tag(wpmsg,"contacts");
  int numextcontacts;
  if (user->priv_rost.loaded) priv_roster_delete(user);
  privLoadGroups(user,groups);
  privLoadContacts(user,contacts);
  //Num extcontacts powinno juz byc wczytane wczesniej, a w rosterze mamy kopie.
  // Powinny byc takie same. Jak nie, to znaczy, ze cos jest nie tak.
  numextcontacts=j_atoi(xmlnode_get_tag_data(wpmsg,"data"),0);
  if (user->priv_rost.numextcontacts!=numextcontacts)
    user_debug(user,WARN_DL,"User %s: %s Niespojnosc numextcontacts (bylo:%i wczytal:%i) !!!\n",
	       user->id.user,__FUNCTION__,user->priv_rost.numextcontacts,numextcontacts);
  user->priv_rost.loaded=1;
}


void priv_roster_delete(user_properities user) {
  priv_roster_item ritem;
  group_item gitem;

  //Usuwamy pool pomocniczy do grup kontaktow
  if (user->priv_rost.grpp) pool_free(user->priv_rost.grpp);
  user->priv_rost.grpp=NULL;

  //Usuwamy kontakty
  ritem=user->priv_rost.priv_rost_list;
  while (ritem) {
    priv_roster_item next=ritem->next;
    priv_roster_item_delete(ritem);
    ritem=next;
  }
  user->priv_rost.priv_rost_list=NULL;

  //Usuwamy grupy
  gitem=user->priv_rost.Groups;
  while (gitem) {
    group_item next=gitem->next;
    group_item_delete(gitem);
    gitem=next;
  }
  user->priv_rost.Groups=NULL;

  //Usuwamy grupe glowna
  if (user->priv_rost.gall) {
    group_item_delete(user->priv_rost.gall);
    user->priv_rost.gall=NULL;
  }
  user->priv_rost.loaded=0;
}
