#include "jab_simul-main.h"
#include <regex.h>
#include "jab_simul-roster.h"

#define CONN_TIMEOUT 15000    // defaultowe 15 [ms]
#define LOGIN_TIMEOUT 400000  // defaultowe 400 [ms]

#define EVENTS_FREQ_DISPERTION 30 // [%] Rozrzut czasow eventow w procentach

char stream_open[] =
"<stream:stream to=\"%s\" xmlns=\"jabber:client\" xmlns:stream=\"http://etherx.jabber.org/streams\">";
char stream_close[] =
"</stream:stream>";
char login_plain[] =
"<iq type=\"set\" id=\"%s\"><query xmlns=\"jabber:iq:auth\"><username>%s</username><password>%s</password><resource>%s</resource></query></iq>";
char login_digest[] =
"<iq type=\"set\" id=\"%s\"><query xmlns=\"jabber:iq:auth\"><username>%s</username><digest>%s</digest><resource>%s</resource></query></iq>";

/*** Troche stuffu do prowadzenia statystyk ***/

#define STATS 10
#define STNULL {0,0,0,0,0}
typedef 
struct s_stat {
  int stat_pos;
  int stat_size;
  int stat_intable;
  long long *stat_times;
  long long stat_sum_times;
} s_stat;

s_stat stat_t[STATS];

void init_stat(int no,int size) {
  stat_t[no].stat_size=size;
  stat_t[no].stat_times=pmalloco(main_config.p,sizeof(stat_t[no].stat_times[0])*stat_t[no].stat_size);
  stat_t[no].stat_pos=0;
  stat_t[no].stat_sum_times=0;
  stat_t[no].stat_intable=0;
}

void add_stat(int no,long long time) {
  stat_t[no].stat_sum_times-=stat_t[no].stat_times[stat_t[no].stat_pos];
  stat_t[no].stat_times[stat_t[no].stat_pos++]=time;
  stat_t[no].stat_sum_times+=time;
  if (stat_t[no].stat_intable<stat_t[no].stat_size) stat_t[no].stat_intable++;
  if (stat_t[no].stat_pos>=stat_t[no].stat_size) stat_t[no].stat_pos=0;
}

long long get_div_stat(int no) {
  return stat_t[no].stat_sum_times/(stat_t[no].stat_intable?stat_t[no].stat_intable:1);
}

long long get_stat(int no) {
  return stat_t[no].stat_sum_times;
}
/*** Troche stuffu do prowadzenia statystyk ***/


char* makeJCOM_ID(user_properities user,char* buf,int bufsize) {
  snprintf(buf,bufsize,"JCOM_%u",user->tmpid++);
  return buf;
}

xmlnode query_header(user_properities user, char* type,char* xmlns, xmlnode* pxmlns_node) {
  xmlnode iq_node=xmlnode_new_tag("iq");
  debug(DET_DL,"%s\n",__FUNCTION__);
  if (iq_node) {
    char idbuf[10];
    xmlnode query_node;
    xmlnode_put_attrib(iq_node,"type",type);
    xmlnode_put_attrib(iq_node,"id",makeJCOM_ID(user,idbuf,sizeof(idbuf)));
    query_node=xmlnode_insert_tag(iq_node,"query");
    if (query_node) {
      xmlnode_put_attrib(query_node,"xmlns", xmlns);      
      *pxmlns_node=query_node;
    }
  }
  return iq_node;
}

int makeQuery(char* buf,int maxlen,char q,
	      const char* id,const char* query_ns,const char* wpmsg_ns) {
  char* tmp=buf;
  int len;
  if (id) 
    len=snprintf(tmp,maxlen,"<iq id=%c%s%c type=%cget%c>",q,id,q,q,q);
  else
    len=snprintf(tmp,maxlen,"<iq type=%cget%c>",q,q);
  tmp+=len;maxlen-=len;
  if (wpmsg_ns)
    len=snprintf(tmp,maxlen,"<query xmlns=%c%s%c><wpmsg xmlns=%c%s%c/></query></iq>",q,query_ns,q,q,wpmsg_ns,q);
  else
    len=snprintf(tmp,maxlen,"<query xmlns=%c%s%c/></iq>",q,query_ns,q);
  tmp+=len;maxlen-=len;  
  return tmp-buf;
}

void user_store_xml_info
(user_properities user,char* id,t_xml_response_handler callback, struct timeval* time) {
  //Dodajemy do kolejki usera info o wyslanym xmlu
  user->conn->xml_need_responses_queue=xml_need_response_node_create
    (id,callback,time,user->conn->xml_need_responses_queue);	    
}


/****************************************************************************/

/* Glowna procedura cyklu zycia usera */

/****************************************************************************/

void user_do_your_job(user_properities user, struct timeval* curtime) {
  /*
  printf("%s %s\n\r",
	 user->state==UP_DISABLED?"DISABLED":
	 user->state==UP_CAN_LOGIN?"CAN_LOGIN":
	 user->state==UP_IS_LOGGING?"IS_LOGGING":
	 user->state==UP_LOGGED_IN?"LOGGED_IN":
	 "UNKNOWN USER STATE",(!user->conn)?"No connection":
	 user->conn->sstate==sock_CONN_IN_PROGRESS?"CONN_IN_PROGRESS":
	 user->conn->sstate==sock_CONNECTED?"CONNECTED":
	 user->conn->sstate==sock_ERROR?"ERROR":
	 user->conn->sstate==sock_NOT_CONNECTED?"NOT_CONNECTED":
	 "UNKNOWN SOCK STATE");fflush(stdout);
  */
  //if (user->conn && user->conn->state==state_CLOSING) user_conn_kill(user);
  switch (user->state) {
  case UP_DISABLED: return;   //disabled
  case UP_CAN_LOGIN: 
    if (user->conn && user->stream_opened) { 
      // we are logging if the connection is estabilished (or at least initiated)
      // The connection must be done by proper repeater (ev_connect)
      if (user->digest_auth) user_do_login_digest(user,curtime);
      else user_do_login_plain(user,curtime);
    }
    break;
  case UP_IS_LOGGING: 
    if (user->conn) {
      //Trzeba sprawdzic Timeout
      const long long c_timeout=(long long)user->connection_timeout*1000;
      const long long l_timeout=(long long)user->login_timeout*1000;
      long long us_passed=ut(*curtime)-ut(user->conn->creation_time);
      //printf("Time passed: %lli\n",us_passed);
      if (user->conn->sstate==sock_CONN_IN_PROGRESS) {
	if (us_passed>=c_timeout) {
	  debug(WARN_DL,"Connection Timeout !!!\n");
	  user_finish_conn(user,"Connection Timeout");
	  user->flags|=UPF_EXPECT_KILL; //Ustawiamy flage
	  //user->conn->state=state_CLOSING;
	  //Konczymy aplikacje, jak jest flaga
	  if (user->flags & UPF_EXIT_AFTER_LOGIN) {
	    debug(MONIT_DL,"Connection timeout - %lli>=%lli [ms] !!!\n",us_passed/1000,c_timeout/1000);
	    user->flags &=~UPF_EXIT_AFTER_LOGIN;
	    exit_app(0);
	  }
	}
      }
      if (us_passed>=l_timeout) {
	debug(WARN_DL,"Login Timeout !!!\n");
	//Konczymy aplikacje, jak jest flaga
	if (user->flags & UPF_EXIT_AFTER_LOGIN) {
	  debug(MONIT_DL,"Login timeout - %lli>=%lli [ms] !!!\n",us_passed/1000,l_timeout/1000);
	  user->flags &=~UPF_EXIT_AFTER_LOGIN;
	  exit_app(0);
	}
      }
    }
    break; //Mozemy jedynie czekac. Mozna by ustawic jakis timeout.
  case UP_LOGGED_IN:
    if (!user->conn) user->state=UP_CAN_LOGIN;
    //W zasadzie wszystko na razie zalatwiaja repeatery
    break;
  default:
    debug(ERR_DL,"This should not happen !!!!! (%s:%i)\n",__FILE__,__LINE__);
  }
  user_handle_repeater_queue(user,curtime);
}




/****************************************************************************/

/* Zestaw handlerow obslugujacych odpowiedzi */

void handle_login_response(xmlnode x, user_properities user, struct timeval* querytime, struct timeval* curtime) {
  if (j_strcmp(xmlnode_get_attrib(x, "type"), "error") == 0) {
    user_debug(user,WARN_DL,"User %s: Authorisation failed !\n",user->id.user);
    user->flags|=UPF_EXPECT_KILL; //Ustawiamy flage
    user_finish_conn(user,"Wrong auth");
    if (user->flags & UPF_EXIT_AFTER_LOGIN) {
      debug(MONIT_DL,"Logowanie nie powiodlo sie !\n");
      user->flags &=~UPF_EXIT_AFTER_LOGIN;
      exit_app(0);
    }
  } else {
    user_debug(user,INFO_DL,"User %s logged in\n",user->id.user);
    user->state=UP_LOGGED_IN;
    if (user->flags & UPF_EXIT_AFTER_LOGIN) {
      debug(MONIT_DL,"Login OK. Time: %i [ms]\n",(int)((ut(*curtime)-ut(user->conn->creation_time))/1000));
      user->flags &=~UPF_EXIT_AFTER_LOGIN;
      exit_app(1);
    }
    //Udalo sie logowanie, wiec slemy standardowe sprawy inicjalizacyjne
    user_do_fetch_properities(user,curtime);
  }
  user_debug(user,INFO_DL,"User %s: login handled\n",user->id.user);
}

int presences_after_roster_count=0;

void handle_roster_response(xmlnode x, user_properities user, struct timeval* querytime, struct timeval* curtime) {
  user_debug(user,DET_DL,"User %s: Response:\n%s\n",user->id.user,xmlnode2str(x));
  if (j_strcmp(xmlnode_get_attrib(x, "type"), "error") == 0) {
    user_debug(user,ERR_DL,"User %s: Problem z pobraniem rostera (%s)!\n",
	       user->id.user,xmlnode_get_tag_data(x,"error"));
  } else {
    xmlnode query = xmlnode_get_tag(x, "query");
    roster_hash_from_xml(user,query,1); //Ladyjemy usuwajac poprzedni
    user_debug(user,ROST_DL,"User %s: roster pociagniety. Liczba kontaktow: %i\n",
	       user->id.user,user->roster_count);
    
    //Ustawiamy status
    user_do_presence(user,curtime,NULL,NULL,user->Status,user->Show);
    presences_after_roster_count++;
  }
}

void handle_roster_set_response(xmlnode x, user_properities user, struct timeval* querytime, struct timeval* curtime) {
  user_debug(user,DET_DL,"User %s: Response:\n%s\n",user->id.user,xmlnode2str(x));
  if (j_strcmp(xmlnode_get_attrib(x, "type"), "error") == 0) {
    user_debug(user,ERR_DL,"User %s: To cos dziwnego... (%s)! %s:%i\n",
	       user->id.user,xmlnode_get_tag_data(x,"error"),__FILE__,__LINE__);
  } else {
    xmlnode query = xmlnode_get_tag(x, "query");
    roster_hash_from_xml(user,query,0);
    user_debug(user,ROST_DL,"User %s: Roster zmodyfikowany. Liczba kontaktow: %i\n",
	       user->id.user,user->roster_count);
  }
}

void handle_numextcontacts_response(xmlnode x, user_properities user, struct timeval* querytime, struct timeval* curtime) {
  user_debug(user,DET_DL,"User %s: Response:\n%s\n",user->id.user,xmlnode2str(x));
  if (j_strcmp(xmlnode_get_attrib(x, "type"), "error") == 0) {
    user_debug(user,WARN_DL,"User %s: Problem z pobraniem numextcontacts (%s) !\n",
	       user->id.user,xmlnode_get_tag_data(x,"error"));
  } else {
    xmlnode data = xmlnode_get_tag(xmlnode_get_tag(xmlnode_get_tag(x,"query"),"wpmsg"),"data");    
    if (!data) {
      user->priv_rost.numextcontacts=0;
      user_debug(user,PROST_DL,"user %s: numextcontacts ustawione na 0\n",
		 user->id.user);
    }
    else {
      user->priv_rost.numextcontacts=j_atoi(xmlnode_get_data(data),0); 
      user_debug(user,PROST_DL,"User %s: numextcontacts OK. Version: %i\n",
		 user->id.user,user->priv_rost.numextcontacts);
    }
    //Orginalny kontakt cos tu dodatkowo sprawdza, my zawsze prosimy o fetch extcontacts
    user_do_fetch_extcontacts(user, curtime);
  }
}

void handle_add_roster_item_response(xmlnode x, user_properities user, struct timeval* querytime, struct timeval* curtime) {
  char* jid_full;
  user_debug(user,DET_DL,"User %s: Response:\n%s\n",user->id.user,xmlnode2str(x));
  jid_full=xmlnode_get_attrib(xmlnode_get_tag(xmlnode_get_tag(x,"query"),"item"),"jid");
  if (j_strcmp(xmlnode_get_attrib(x, "type"), "error") == 0) {
    user_debug(user,ERR_DL,"User %s: Problem z dodaniem rostera %s !\n",
	       user->id.user,jid_full);
    user_debug(user,ERR_DL,"User %s: ERROR: %s\n",
	       user->id.user,xmlnode_get_tag_data(x,"error"));
  } else {
    user_debug(user,ROST_DL,"User %s: roster dodany OK\n",user->id.user);
    add_stat(RADD_STAT,ut(*curtime)-ut(*querytime));
  }
}

void handle_del_roster_item_response(xmlnode x, user_properities user, struct timeval* querytime, struct timeval* curtime) {
  char* jid_full;
  user_debug(user,DET_DL,"User %s: Response:\n%s\n",user->id.user,xmlnode2str(x));
  jid_full=xmlnode_get_attrib(xmlnode_get_tag(xmlnode_get_tag(x,"query"),"item"),"jid");
  if (j_strcmp(xmlnode_get_attrib(x, "type"), "error") == 0) {
    user_debug(user,ERR_DL,"User %s: Problem z usunieciem rostera %s !\n",
	       user->id.user,jid_full);
    user_debug(user,ERR_DL,"User %s: ERROR: %s\n",
	       user->id.user,xmlnode_get_tag_data(x,"error"));
  } else {
    user_debug(user,ROST_DL,"User %s: roster usuniety OK\n",user->id.user);
    add_stat(RDEL_STAT,ut(*curtime)-ut(*querytime));
  }
}
/* ********************************************************************************* */

int received_presences_count=0;
int presences_rostadd_count=0;

void handle_presence(xmlnode x, user_properities user, struct timeval* querytime, struct timeval* curtime) {
  char* jid_from;
  char* type;
  user_debug(user,DET_DL,"User %s: Presence:\n%s\n",user->id.user,xmlnode2str(x));
  jid_from=xmlnode_get_attrib(x,"from");
  type=xmlnode_get_attrib(x,"type");
  received_presences_count++;
  user->received_presences_count++;
  if (j_strcmp(type,"subscribe")==0) {
    user_do_presence(user,curtime,"subscribed",jid_from,NULL,NULL);
    presences_rostadd_count++;
  } else {
    char* status=xmlnode_get_tag_data(x,"status");
    roster_item ri=xhash_get(user->roster_hash,jid_from);
    user_debug(user,DET_DL,"User %s: %s change of availability\n",user->id.user,jid_from);
    if (ri) {
      if (j_strcmp(type,"subscribe")==0 || j_strcmp(status,"Disconnected")==0)
	ri->available=0; else ri->available=1;
      user_debug(user,DET_DL,"!!! User %s: %s changed availability to %i\n",user->id.user,
		 jid_from,ri->available);
    }
  }
}

int received_messages_count=0;
int received_messages_offline_count=0;
int received_messages_admin_count=0;
int forwarded_messages_count=0;

void handle_message(xmlnode x, user_properities user, struct timeval* querytime, struct timeval* curtime) {
  const char* msg_deb_regex="\\[([0-9]+) ([^ ]+) ([^]]+)\\].*";
  static regex_t c_msg_deb_regexp;
  static int compiled=0;

  char *jid_from;
  char *body;
  int is_offline,is_admin,forwarded_msg;
  regmatch_t msg_deb_matches[4];
  received_messages_count++;
  jid_from=xmlnode_get_attrib(x,"from");
  body=xmlnode_get_tag_data(x,"body");
  is_offline=!j_strcmp(xmlnode_get_tag_data(x,"x"),"Offline Storage");
  is_admin=!j_strcmp(jid_from,"Administrator");
  forwarded_msg=!j_strcmp(xmlnode_get_attrib(x,"forwarded"),"yes");
  user_debug(user,DET_DL,"User %s: Message from %s\n",user->id.user,jid_from);
  if (is_offline && !is_admin) {
    user_debug(user,INFO_DL,"Got offline message.\n");
    received_messages_offline_count++;
  }
  if (compiled==0) {
    compiled=1;
    debug(DET_DL,"var_regex: >%s<\n",msg_deb_regex);
    if (regcomp(&c_msg_deb_regexp,msg_deb_regex,REG_EXTENDED)) {
      compiled=-1; //err    
      debug(ERR_DL,"!!!! %s:%i\n",__FILE__,__LINE__);
    }
  }
  
  if ( !is_admin && body && compiled==1) {
    if (regexec(&c_msg_deb_regexp,body,4,msg_deb_matches,0)==0) {
      int error_msg=!j_strcmp(xmlnode_get_attrib(x,"type"),"error");
      pool tmp=pool_new();
      char* deb_from_name,* deb_to_name;
      long long deb_tm;
      char* body1=pstrdup(tmp,body);
      body1[msg_deb_matches[1].rm_eo]=0;
      body1[msg_deb_matches[2].rm_eo]=0;
      body1[msg_deb_matches[3].rm_eo]=0;
      //Czytamy timestamp momentu wyslania wiadomosci.
      sscanf(&body[msg_deb_matches[1].rm_so],"%Li",&deb_tm);
      if (error_msg ^ forwarded_msg) {
	// UWAGA: jak wystapil error, to wiadomosc wraca do nadawcy i trzeba to uwzglednic.
	//        Trzeba tez uwzglednic fakt, ze wiadomosc mogla byc juz forwardowana.
	deb_from_name=&body1[msg_deb_matches[3].rm_so];
	deb_to_name=&body1[msg_deb_matches[2].rm_so];
      } else {
	// Inaczej bierzemy normalnie.
	deb_from_name=&body1[msg_deb_matches[2].rm_so];
	deb_to_name=&body1[msg_deb_matches[3].rm_so];
      }
      //Na wszelki wypadek sprawdzamy, czy sender sie pokrywa.
      if (j_strncmp(jid_from,deb_from_name,j_strlen(deb_from_name))) {
	debug(ERR_DL,"Bad sender of the message !!!!\nNiby przyszlo od:\t%s\nA rzeczywiscie wyslal:\t%s\n",
	      jid_from,deb_from_name);
	debug(ERR_DL,"Msg xml:\n%s\n",xmlnode2str(x));
      }
      //Sprawdzamy tez, czy doszla tam gdzie powinna
      if (j_strncmp(user->id.user,deb_to_name,j_strlen(deb_to_name))) {
	debug(ERR_DL,"Bad receiver of the message !!!!\nDoszlo do:\t%s\nA powinno do:\t%s\n",
	      user->id.user,deb_to_name);
	debug(ERR_DL,"Msg xml:\n%s\n",xmlnode2str(x));
      }
      if (!is_offline) {
	if (!forwarded_msg) {
	  // Odsylamy wiadomosc do nadawcy coby se czas mogl policzyc.
	  xmlnode msg=xmlnode_dup(x);
	  jutil_tofrom(msg);
	  xmlnode_put_attrib(msg,"forwarded","yes");
	  jpolld_conn_write(user->conn,msg,0);
	  forwarded_messages_count++;
	} else {
	  //Liczymy czas odebrania wiadomosci
	  deb_tm=ut(*curtime)-deb_tm;
	  user->received_msgs_count++;
	  user->received_msgs_time_sum+=deb_tm;
	  add_mesg_stat(deb_tm);
	  user_debug(user,INFO_DL,"User %s: Message received after %Li [ms]. Avg: %Li [ms]\n",
		     user->id.user,deb_tm/1000,get_div_mesg_stat()/1000);
	}
      }
      pool_free(tmp);
    } else user_debug(user,ERR_DL,
		      "Message does not contain debug info !!!                                  \n%s\n",
		      xmlnode2str(x));
  } else if (is_admin) {
    user_debug(user,WARN_DL,"Message from admin !                                  \n%s\n",
	       xmlnode2str(x));
    received_messages_admin_count++;
  }
}

void handle_extcontacts(xmlnode x, user_properities user, struct timeval* querytime, struct timeval* curtime) {
  user_debug(user,PROST_DL,"User %s: %s\n%s\n",
	     user->id.user,__FUNCTION__,
	     xmlnode2str(xmlnode_get_tag(xmlnode_get_tag(x,"query"),"wpmsg")));
  priv_roster_from_xml(user,x);
}

/* ********************************************************************************* */
/* ********************************************************************************* */

// Glowna ogolna procedura obslugujaca XSTREAM_NODE"y przychadzace od serwera.
// Rozpoznaje odpowiedzi i wywoluje odpowiednie obslugi.
void handle_user_node(xmlnode x, user_properities user, struct timeval* curtime) {
  //Wyciagamy identyfikator wiadomosci
  char* id=xmlnode_get_attrib(x,"id");
  if (!id) {
    //Nie ma id'a. Ale byc moze jest to iq:private i trzeba identyfikowac inaczej
    xmlnode query=xmlnode_get_tag(x,"query");
    if (query && 0==j_strcmp(xmlnode_get_attrib(query,"xmlns"),"jabber:iq:private"))
      id=xmlnode_get_attrib(xmlnode_get_tag(query,"wpmsg"),"xmlns");
  }
  debug((id?INFO_DL:DET_DL),"Cos mamy... Id=%s\n",id);

  if (id) {
    // Cos przyszlo, byc moze odpowiedz na cos naszego ? Szukamy ...
    xml_need_response node=xml_need_response_node_get
      (id,&(user->conn->xml_need_responses_queue));
    if (node) {
      user_debug(user,INFO_DL,"User %s: Przyszla odpowiedz na Id=%s. Czas: %u ms\n",
	    user->id.user,id,(unsigned)(ut(*curtime)/1000-ut(node->send_time)/1000));
      //tak - to odpowiedz na cos naszego. 
      // Usunelismy to z kolejki. Teraz wywolujemy obsluge.
      if (node->callback) node->callback(x,user,&node->send_time,curtime);
      //i usuwamy na dobre
      xml_need_response_node_delete(node);
    } else {
      user_debug(user,INFO_DL,"User %s: Przyszlo cos nieznanego z idem !. Id=%s\n",user->id.user,id);
      user_debug(user,INFO_DL,"%.400s\n",xmlnode2str(x));
      //Tu by sie przydalo rozpoznac, co to jest. A przynajmniej:
      //messages
      //presences
    }
  } else if (j_strcmp(xmlnode_get_name(x),"presence")==0) {
    handle_presence(x, user, NULL, curtime);
  } else if (j_strcmp(xmlnode_get_name(x),"message")==0) {
    handle_message(x, user, NULL, curtime);
  } else if (j_strcmp(xmlnode_get_name(x),"iq")==0) {
    xmlnode query=xmlnode_get_tag(x,"query");
    char* type=xmlnode_get_attrib(x,"type");
    if (query) {
      char* xmlns=xmlnode_get_attrib(query,"xmlns");
      if (!j_strcmp(type,"set") && !j_strcmp(xmlns,"jabber:iq:roster"))
	handle_roster_set_response(x,user,NULL,curtime);
      else {
	user_debug(user,ERR_DL,"User %s: Nie wiem co z tym zrobic...\n",user->id.user);
	user_debug(user,ERR_DL,"%.400s\n",xmlnode2str(x));
      }
    } else {
      user_debug(user,ERR_DL,"User %s: Jakis dziwny iq przyszedl - bez query !\n",user->id.user);
      user_debug(user,ERR_DL,"%.400s\n",xmlnode2str(x));
    }
  } else if (!j_strcmp("stream:error",xmlnode_get_name(x))) {
    user_debug(user,WARN_DL,"User %s: Serwer zwrocil informacje o bledzie !\n",user->id.user);
    user_debug(user,MONIT_DL,"User %s: ERROR: %s\n",user->id.user,xmlnode_get_data(x));
  } else {
    user_debug(user,ERR_DL,"User %s: Przyszlo cos nieznanego bez ida !\n",user->id.user);
    user_debug(user,ERR_DL,"%.400s\n",xmlnode2str(x));
  }
}

/* ********************************************************************************* */
/* ********************************************************************************* */


/* Procedury wysylajace requesty do serwera */

int user_do_login_plain(user_properities user, struct timeval* curtime) {
  char xml_buf[1000];
  char id[10];
  makeJCOM_ID(user,id,sizeof(id));
  //Przesylamy dane do autoryzacji
  snprintf(xml_buf,sizeof(xml_buf),login_plain,id,user->id.user,user->passwd,user->id.resource);
  jpolld_conn_write_str(user->conn, xml_buf, 1);
  //Dodajemy do kolejki info, ze oczekujemy odpowiedzi, czy sie udalo
  user_store_xml_info(user,id,handle_login_response,curtime);	    
  user->state=UP_IS_LOGGING;
  user_debug(user,INFO_DL,"User %s: Prosba o zalogowanie wyslana\n",user->id.user);
  return 0;
}

int user_do_login_digest(user_properities user, struct timeval* curtime) {
  char xml_buf[1000];
  char id[10];
  char* digest;
  spool s;
  pool p=pool_new();
  makeJCOM_ID(user,id,sizeof(id));
  s = spool_new(p);
  spooler(s,user->sid,user->passwd,s);
  digest=shahash(spool_print(s));
  user_debug(user,INFO_DL,"User %s: sid:%s \tdigest:%s\n",
	     user->id.user,user->sid,digest);
  //Przesylamy dane do autoryzacji
  sprintf(xml_buf,login_digest,id,user->id.user,digest,user->id.resource);
  pool_free(p);
  jpolld_conn_write_str(user->conn, xml_buf, 1);
  //Dodajemy do kolejki info, ze oczekujemy odpowiedzi, czy sie udalo
  user_store_xml_info(user,id,handle_login_response,curtime);	    
  user->state=UP_IS_LOGGING;
  user_debug(user,INFO_DL,"User %s: Prosba o zalogowanie wyslana\n",user->id.user);
  return 0;
}

int user_do_fetch_properities(user_properities user, struct timeval* curtime) {
  //Wierne odwzorowanie danych slanych przez Kontakt
  char xml_buf[2000]; 
  char* tmp;
  int len;
  int bufsize=sizeof(xml_buf);
  //Wysylamy jeden xml
  makeQuery(xml_buf,bufsize,'\'',NULL,"jabber:iq:private","wpmsg:ignored");
  jpolld_conn_write_str(user->conn, xml_buf, 1);
  //Dodajemy do kolejki info, ze poprosilismy o wpmsg:ignored i oczekujemy odpowiedzi
  user_store_xml_info(user,"wpmsg:ignored",NULL,curtime);	    

  // i nastepne 3 od razu
  tmp=xml_buf;
  len=makeQuery(tmp,bufsize,'"',NULL,"jabber:iq:private","wpmsg:numextcontacts");tmp+=len;bufsize-=len;
  len=makeQuery(tmp,bufsize,'"',NULL,"jabber:iq:private","wpmsg:usersettings");tmp+=len;bufsize-=len;
  len=makeQuery(tmp,bufsize,'"',"doroster_0","jabber:iq:roster",NULL);tmp+=len;bufsize-=len;
  jpolld_conn_write_str(user->conn, xml_buf, 1);

  //Dodajemy do kolejki info, ze poprosilismy o wpmsg:numextcontacts i oczekujemy odpowiedzi
  user_store_xml_info(user,"wpmsg:numextcontacts",handle_numextcontacts_response,curtime);	    
  //Dodajemy do kolejki info, ze poprosilismy o wpmsg:usersettings i oczekujemy odpowiedzi
  user_store_xml_info(user,"wpmsg:usersettings",NULL,curtime);	    
  //Dodajemy do kolejki info, ze poprosilismy o fetch rostera i oczekujemy odpowiedzi
  user_store_xml_info(user,"doroster_0",handle_roster_response,curtime);
  debug(INFO_DL,"User %s: Zapytanie inicjalizacyjne wyslane\n",user->id.user);
  return 0;
}

int user_do_fetch_extcontacts(user_properities user, struct timeval* curtime) {
  int ret;
  //Wierne odwzorowanie danych slanych przez Kontakt
  char xml_buf[2000];
  //Wysylamy jeden xml
  makeQuery(xml_buf,sizeof(xml_buf),'\'',NULL,"jabber:iq:private","wpmsg:extcontacts");
  //Dodajemy do kolejki info, ze poprosilismy o wpmsg:extcontacts i oczekujemy odpowiedzi
  ret=jpolld_conn_write_str(user->conn, xml_buf, 1);
  if (!ret) {
    user_store_xml_info(user,"wpmsg:extcontacts",handle_extcontacts,curtime);	    
    user_debug(user,INFO_DL,"User %s: Zapytanie o extcontacts wyslane\n",user->id.user);
  }
  return ret;
}

int user_do_presence(user_properities user, struct timeval* curtime, char* type, char* jid_to, char* status, char* show) {
  int ret,presences_send;
  xmlnode x=xmlnode_new_tag("presence");
  if (jid_to) {
    xmlnode_put_attrib(x,"to",jid_to);
    presences_send=1;
  } else presences_send=user->roster_count; //To jest tak w przyblizeniu, ze do kazdego wysle presence'a.
  if (type) xmlnode_put_attrib(x,"type",type);
  if (status) xmlnode_insert_cdata(xmlnode_insert_tag(x,"status"),status,-1);
  if (show) xmlnode_insert_cdata(xmlnode_insert_tag(x,"show"),show,-1);
  user_debug(user,INFO_DL,"User %s: Presence wyslany (do:%s status:%s:%s)\n",
	     user->id.user,jid_to,status,show);
  //Nie oczekujemy odpowiedzi
  ret=jpolld_conn_write(user->conn, x, 0);
  if (!ret) user->sent_presences_count+=presences_send;
  return ret;
}

int user_do_adddel_roster_item(user_properities user, struct timeval* curtime,
			    char* jid_full, char* name, int del) {

  //Tworzymy i wysylamy xmla proszacego o dodanie nowego kontaktu
  xmlnode query;
  xmlnode iq=query_header(user,"set","jabber:iq:roster",&query);
  xmlnode item=xmlnode_insert_tag(query,"item");
  xmlnode_put_attrib(item,"jid",jid_full);
  if (!del) {
    //Dodawanie rostera
    xmlnode_put_attrib(item,"name",name);
    xmlnode_put_attrib(item,"subscription","none");
    xmlnode_put_attrib(item,"ask","subscribe");
    //Dodajemy do kolejki info, ze oczekujemy odpowiedzi, czy sie udalo dodac
    user_store_xml_info(user,xmlnode_get_attrib(iq,"id"),handle_add_roster_item_response,curtime);	    
  } else {
    //Usuwanie rostera
    xmlnode_put_attrib(item,"subscription","remove");
    //Dodajemy do kolejki info, ze oczekujemy odpowiedzi, czy sie udalo usunac
    user_store_xml_info(user,xmlnode_get_attrib(iq,"id"),handle_del_roster_item_response,curtime);	    
  }
  return jpolld_conn_write(user->conn, iq, 0);
}

int user_do_send_message(user_properities user, struct timeval* curtime,
			 char* jid_full, char* msg) {
  char* xml_buf;
  int ret;
  pool p;
  char id[10];
  if (!msg) {
    debug(ERR_DL,"Null msg !?!\n");
    return 0; 
  }
  p=pool_new();
  xml_buf=pmalloc(p,strlen(msg)+strlen(jid_full)+500);
  makeJCOM_ID(user,id,sizeof(id));
  //Tworzymy i wysylamy xmla z wiadomoscia
  //sprintf(xml_buf,"<message id=\"%s\" to=\"%s\"><body>%s</body></message>",id,jid_full,msg);
  sprintf(xml_buf,"<message to=\"%s\"><body>%s</body></message>",jid_full,msg);
  ret=jpolld_conn_write_str(user->conn, xml_buf, 0);
  pool_free(p);
  //Nie oczekujemy odpowiedzi
  return ret;
}

int user_do_send_raw_bytes(user_properities user, struct timeval* curtime,char* bytes) {
  return jpolld_conn_write_str(user->conn, bytes, 0);
  //Nie oczekujemy odpowiedzi .... ;)
}
/* ******************************************************************************* */
/* ******************************************************************************* */
/* ******************************************************************************* */


// Tworzy nowy element, ktory mozna dodac do listy oczekiwan na odpowiedz
xml_need_response xml_need_response_node_create
(char* id,t_xml_response_handler callback, 
 struct timeval* time,xml_need_response queue) {
  pool p;
  xml_need_response nr;
  p = pool_new();
  nr = pmalloco(p, sizeof(_xml_need_response));
  nr->p=p;
  nr->id=pstrdup(p,id);
  nr->callback=callback;
  nr->send_time=*time;
  nr->next=queue;
  debug(DET_DL,"Dodalismy nowy Id=%s do kolejki(%p)\n",id,queue);
  return nr;
}

// Szuka i od razu usuwa element z listy oczekiwan na odpowiedzi.
// Zwraca wskaznik na usuniety element.
// UWAGA: nie zwalnia pamieci zajmowanej przez ten element, dzieki temu
// mozna go jeszcze dalej wykorzystywac
xml_need_response xml_need_response_node_get(char* id,xml_need_response* queue) {
  xml_need_response ret;
  for(;*queue && j_strcmp((*queue)->id, id);queue=&((*queue)->next));
  ret=*queue;
  if (*queue) *queue=(*queue)->next; //Znaleziony wyrzucamy z listy
  return ret;
}

// Tworzymy strukture
user_properities user_create(_user_name* uns,
			     char* passwd,char* resource,char* fullname,
			     char* host,int port,
			     event_repeater ev_list, 
			     int ssl) {
  pool p;
  user_properities up;
  p = pool_new();
  up = pmalloco(p, sizeof(_user_properities));
  up->id.p=p;
  up->id.user=uns->user;            //Nie kopiujemy, tylko idze bezposrednio wskaznik do przestrzeni nazw !!!
  up->id.server=uns->server;        //Nie kopiujemy, tylko idze bezposrednio wskaznik do przestrzeni nazw !!!
  up->id.resource=resource&&resource[0]?pstrdup(p,resource):NULL; //Kopiujemy podany lub null
  up->id.full=fullname&&fullname[0]?pstrdup(p,fullname):NULL;     //Kopiujemy podany lub null
  up->host=host?pstrdup(p,host):  //Kopiujemy jak podany jakis inny lub ...
    up->id.server;                //Nie kopiujemy, tylko idze kopia wskaznika takiego samego jak w jid
  up->port=port;
  up->ssl = ssl;
  up->connection_timeout=CONN_TIMEOUT;
  up->login_timeout=LOGIN_TIMEOUT;
  up->passwd=pstrdup(p,passwd);
  up->next=NULL;
  up->conn=NULL;                  
  up->state=UP_DISABLED;         // Domyslnie user jest nieaktywny - disabled, czyli nic nie robi
  up->events_queue=NULL;         // Domyslnie user nie podejmuje zadnych akcji w czasie, 
                                 //  tylko sie loguje( jesli state=US_CAN_LOGIN )
  up->sooner_activation_tm=0;    // Za pierwszym razem na pewno sprawdzi eventy
  up->tmpid=0;                   // Licznik identyfikatorow wiadomosci wyzerowany
  up->flags=0;                   // Zadnych dodatkowych flag
  up->sniff_dir=NULL;            // Domyslnie nie logujemy socketow do plikow
  up->msg_resp_propab=0.0;       // Domyslnie nie odpowiada na przyslane wiadomosci
  up->received_msgs_time_sum=0;  // Nic jeszcze nie odebralismy
  up->received_msgs_count=0;     // Nic jeszcze nie odebralismy
  up->received_presences_count=0;
  up->sent_presences_count=0;
  up->last_roster_count=-1;
  up->roster_count=0;
  up->roster_hash=NULL;
  up->priv_rost.priv_rost_list=NULL;
  up->priv_rost.numextcontacts=0;
  up->priv_rost.grpp=NULL;
  up->priv_rost.Groups=NULL;
  up->priv_rost.gall=group_item_create("Wszyscy",1);
  up->priv_rost.loaded=0;
  up->Status="Online";
  up->Show=NULL;
  debug(WARN_DL,"Dodalismy nowego usera(%s:%s)\n",up->id.user,up->id.full);

  // Kopiujemy liste repeaterow - musi byc kopia, bo kazdy user ma indywidualnie
  // ustawiane czasy aktywacji eventow.
  copy_repeaters(p,&(up->events_queue),ev_list);
  return up;
}

//Usuwamy strukture
void user_delete(user_properities up) {
  up->flags|=UPF_EXPECT_KILL; //Ustawiamy flage
  user_conn_kill(up);  
  roster_hash_delete(up);
  pool_free(up->id.p);
}

extern int do_events;

#define NEVER 0xFFFFFFFF // max long long
void user_handle_repeater_queue(user_properities user,struct timeval* curtime) {
  event_repeater cur_er;
  long long curtimestamp=ut(*curtime);
  // Czy juz nadszedl czas na najblizszy event ?
  // (specjalnie pamietamy w oddzielnej zmiennej czas aktywacji najblizszego, zeby za 
  // kazdym razem nie przeszukiwac calej listy)
  if (user->sooner_activation_tm<=curtimestamp) {
    //Tak, nadszedl. Trzeba znalezc na ktory(ktore) juz czas i trzeba tez znalezc nastepny najblizszy
    user->sooner_activation_tm=NEVER;
    for (cur_er=user->events_queue;cur_er;cur_er=cur_er->next) {
      if (cur_er->activation_tm && cur_er->activation_tm<=curtimestamp) {
	//Aktywny ! Juz nadszedl czas !
		if (cur_er->frequency>=0) {
		  //Ustawiamy czas nastepnej aktywacji (wzgledem czasu obecnego, a nie aktywacji)
		  cur_er->activation_tm=curtimestamp+cur_er->frequency
			// dodajemy losowe odchylenie
			+cur_er->frequency/10000*(random()%(EVENTS_FREQ_DISPERTION*100)-EVENTS_FREQ_DISPERTION*50);
		}
		//Wywolujemy procedure obslugi (jesli jest i jesli user jest w odpowiednim stanie)
		// I jesli counter sie nie wyczerpal TODO wywalac wyczerpane countery
		if (cur_er->event_handler && 
			(cur_er->user_state==UP_EVER || cur_er->user_state==user->state) &&
			cur_er->counter!=0) {

		  if ( do_events==1 ||   
			   cur_er->user_state==UP_EVER || 
			   cur_er->user_state==UP_CAN_LOGIN) {
			cur_er->event_handler(cur_er,user,curtime);
			if (cur_er->counter>0) 
			  cur_er->counter--;
		  }
		}	
      }
      if (user->sooner_activation_tm>cur_er->activation_tm) { 
		//Znalezlismy jakis blizszy od poprzednio ustawionego, wiec ustawiamy
		user->sooner_activation_tm=cur_er->activation_tm;
		debug(DET_DL,"User %s: Current timestamp: %lli [ms]. Sooner timestamp: %lli [ms]\n",
			  user->id.user,(curtimestamp/1000),(user->sooner_activation_tm/1000));
      }
    }
  }
}


/* ********************************************** */
/* ********************************************** */
/* ********************************************** */


//Zwraca -1 jak sie nie uda
int user_connect(user_properities user) {
  int connfd,flags,loc;
  struct timeval curtime;
  sock_state sstate;
  jpolld_thread t=main_thread;

  next_pfd cur_npfd;
  user_debug(user,CONN_DL,"User %s: Connecting to %s:%i\n",user->id.user,user->host,user->port);
  gettimeofday(&curtime,NULL);
  connfd= make_clientfd(user->host,user->port,&sstate);  //UWAGA: Jest nieblokujace !
  if(connfd <= 0) {
    user_debug(user,INFO_DL,"User %s: Connfd messed up\n",user->id.user);
    close(connfd);
    connfd=-1;
    if (user->flags & UPF_EXIT_AFTER_LOGIN) {
      debug(MONIT_DL,"Connection failed\n");
      user->flags &=~UPF_EXIT_AFTER_LOGIN;
      exit_app(0);
    }
    return -1;
  }

  flags = fcntl(connfd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  if(fcntl(connfd, F_SETFL, flags) < 0)
    printf("[%d] Error setting conn flags\n", t->id);
  if(t->npfd == NULL) {
    /* XXX If we're over our MAX_PFDS stop listening */
    add_conns(t);
  }
  loc = t->npfd->pos;
  
  user_debug(user,CONN_DL,"[%d] Adding conn at %d\n", t->id, loc);
  t->pfds[loc].fd = connfd;
  t->pfds[loc].events = POLLIN;
  
  t->conns[loc] = jpolld_conn_create(t, loc, user, sstate, &curtime);	    /* sielim PATCH */
  user->conn=t->conns[loc];                                                 /* sielim PATCH */

  t->mpfd = (loc > t->mpfd) ? loc: t->mpfd;
  
  if(t->npfd == NULL) {
    user_debug(user,ERR_DL,"[%d] We have a grave error NULL npfd\n", t->id);
  } else {
    char xml_buf[300];
    cur_npfd = t->npfd;
    t->npfd = cur_npfd->next;
    free(cur_npfd);
    //Wysylamy xml'a otwierajacego stream
    snprintf(xml_buf,sizeof(xml_buf),stream_open,user->id.server);
    jpolld_conn_write_str(user->conn, xml_buf, 1);
  }  
  debug(CONN_DL,"[%d] Max PFD: %d\n", t->id, t->mpfd);
  return 0;
}

void user_finish_conn(user_properities user,char* whyinfo) {
  if (user && user->conn) {
    user->conn->state=state_CLOSING;
    debug(MONIT_DL,"User %s: Finishing because of: %s\n",user->id.user,whyinfo);
  } else debug(ERR_DL,"To sie nie powinno zdarzyc !!!(%s:%i)\n",__FILE__,__LINE__);
}
#ifdef user_conn_kill
# undef user_conn_kill
#endif
#ifdef jpolld_conn_kill
# undef jpolld_conn_kill
#endif

int jpolld_conn_unexpected_kill_count=0;
int user_conn_kill(user_properities user, char* file, int line) {
  jpolld_conn c=user->conn;
  if (c) {
    c->user=NULL;
    jpolld_conn_kill(c,file,line);
  }
  user->conn=NULL;
  user->state=UP_CAN_LOGIN;
  user->stream_opened=0;
  roster_hash_delete(user);
  priv_roster_delete(user);
  if (!(user->flags&UPF_EXPECT_KILL)) {
    jpolld_conn_unexpected_kill_count++;
    user_debug(user,ERR_DL,"User %s: Unexpected death of user connection (%s:%i)!\n",user->id.user,file,line);
  } else user->flags&=~UPF_EXPECT_KILL; //Zerujemy flage
  //TODO: Dodac uzaleznienie od dodatkowych flag, czy na pewno 
  // ma przechodzic w UP_CAN_LOGIN
  if (user->flags & UPF_EXIT_AFTER_LOGIN) {
    debug(MONIT_DL,"Connection killed !\n");
    user->flags &=~UPF_EXIT_AFTER_LOGIN;
    exit_app(0);
  }
  return 0;
}
