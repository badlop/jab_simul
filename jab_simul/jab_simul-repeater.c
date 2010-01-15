#include "jab_simul-main.h"
#include "jab_simul-roster.h"

/*
  Polskie fonty w xml'ach:
a_¹
c_æ
e_ê
n_ñ
o_ó
s_�
z_¿
x_�

A_¥
C_�
E_�???
N_�
O_�
S_�
Z_¯
X_�

  a ¹
  c æ
  e ê
  l ³
  o ó
  z ¿
  S �
*/

struct status_str statusy[]={
  {"Dostêpny",                    "chat"},
  {"Chcê pogadaæ",               "chat"},
  {"Szukam przyjació³",          "chat"},
  {"Szukam znajomych",             "chat"},
  {"Szukam dziewczyny",            "chat"},
  {"Szukam faceta",                "chat"},
  {"Wyszed³em na obiad",          "away"},
  {"Wyszed³em wyrzuciæ �mieci",  "away"},
  {"Wyszed³em do kibelka",        "away"},
  {"Rozmawiam przez telefon",      "away"},
  {"Zmywam naczynia",              "away"},
  {"Jestem w ³ó¿ku",            "away"},
  {"Zasn¹³em nad klawiatur¹",   "away"},
  {"Niedostêpny na d³u¿ej",     "xa"},
  {"�piê",                        "xa"},
  {"Pracujê",                     "xa"},
  {"Wyszed³em po zakupy",         "xa"},
  {"Wyjecha³em na wakacje",       "xa"},
  {"Nie przeszkadzaæ",            "dnd"},
  {"Pracujê",                     "dnd"},
  {"Programujê",                  "dnd"},
  {"Nie jestem w nastroju",        "dnd"},
  {"",""} //To zawsze powinno byc na koncu
};

/* **************** procedurki pomocnicze ********************** */

char* makeUserName_defined_in_xml(char* buf, int maxsize, xmlnode x, user_properities sender) {
  // Procedurka rozbiera xml'e typu:
  //  "<range><from>100</from><to>110</to></range>" lub
  //  "<from_roster>" lub 
  //  "<jid>dalen@jabber.wp.pl</jid>"
  // Pierwsza postac oznacza, ze user ma zostac wylosowany z 
  // przestrzeni userow (w podanym zakresie).
  // Druga - losujemy z rostera danego usera
  // Trzecia - dajemy bezposrednio podana nazwe
  xmlnode range=xmlnode_get_tag(x,"range");
  char* id;
  if (xmlnode_get_tag(x,"from_roster") && sender->roster_count>0) {
    //Losujemy z rostera
    int num=random()%sender->roster_count;
    roster_item ri=xhash_get_nth(sender->roster_hash,num);
    snprintf(buf,maxsize,ri->jid);
    user_debug(sender,DET_DL,"Wylosowany user: %s\n",buf);
    return buf;
  } else if (range) {
    int from=j_atoi(xmlnode_get_tag_data(range,"from"),0);
    int to=j_atoi(xmlnode_get_tag_data(range,"to"),0);
    if (to>=from && sender->conn) {
      int num=random()%(to-from+1)+from;
      jpolld_instance i=sender->conn->t->i;
      if (i->user_names_space[num].enabled) {
	snprintf(buf,maxsize,"%s@%s",i->user_names_space[num].user,i->user_names_space[num].server);
	return buf;
      }
    } 
  } else if ((id=xmlnode_get_tag_data(x,"jid"))) {
    snprintf(buf,maxsize,"%s",id);
    return buf;
  }
  //Nic sie nie udalo wpisac
  return NULL;
}

char* makeBytes_defined_in_xml(user_properities user,pool p,char* buf, int maxsize, xmlnode x, char* to) {
  // Procedurka rozbiera xml'e typu:
  //  "<text>aaaaaaa...</text>" lub
  //  "<file>filename</file>"
  //  "<random_stream len=[number]>"
  // + opcjonalnie <prepend_with_debug_info/>

  // i wpisuje do bufora. W przypadku pierwszym wpisuje po prostu podany text
  // (bez zadnych modyfikacji).
  // W przypadku drugim wpisuje do bufora zawartosc pliku (tez bez zmian).
  // W szczegolnosci pliki moga byc binarne, a z tym trzeba uwazac.
  // Mozna tez zlecic, zeby do bufora trafil losowy ciag znakow (o podanej dlugosci), bez zer w srodku; 
  // Jesli buf==0 to procedura uzywa podanego poola do zaallokowania
  // odpowiedniego bufora.
  // UWAGA: Jesli podamy <prepend_with_debug_info> to przed wlasciwym wpisem do bufora
  //  wpisuje dodatkowa informacje debugowa w formacie [timestamp from to].
  //  wtedy jest wlasnie wykorzystany parametr  char* to
  char* ret=buf;
  char prepbuf[1000]="";
  if (xmlnode_get_tag(x,"prepend_with_debug_info")) {
    //Jak jest ta opcja, to przed wiadomoscia wpisujemy nazwe usera i timestamp - dane kontrolne
    char* tmp;
    struct timeval curtime;
    gettimeofday(&curtime,NULL);
    //Obcinamy - bez nazwy serwera
    to=pstrdup(p,to);
    if ((tmp=strchr(to,'@'))) *tmp=0;
    sprintf(prepbuf,"[%Li %s %s] ",ut(curtime),user->id.user?user->id.user:"?",to);
  }
  if (xmlnode_get_tag(x,"text")) {
    char* text=xmlnode_get_tag_data(x,"text");
    if (!ret) {
      maxsize=j_strlen(prepbuf)+j_strlen(text)+1;
      ret=pmalloc(p,maxsize);
    }
    snprintf(ret,maxsize,"%s%s",prepbuf,text?text:"");
  } else if (xmlnode_get_tag(x,"file") && ((buf&&maxsize>0) || p)) {
    char* filename=xmlnode_get_tag_data(x,"file");
    FILE* f=fopen(filename,"r");
    if (f) {
      unsigned long count=0;
      if (buf&&maxsize>0) {
	count=snprintf(buf,maxsize,"%s",prepbuf);
	count+=fread(buf+count,1,maxsize-1-count,f);
      }
      else {
	//Allokujmy bufor na CALY plik (+ debug info)
	struct stat s;
	fstat(fileno(f),&s);
	maxsize=s.st_size+1+strlen(prepbuf);
	ret=pmalloc(p,maxsize);
	// wpisujemy ew. informacje debugowa
	count=snprintf(ret,maxsize,"%s",prepbuf);
	// i wczytujemy...
	while (!feof(f)) count+=fread(ret+count,1,maxsize-count,f);
      }
      ret[count]=0;
      user_debug(user,DET_DL,"User %s: Wczytano %lu bajtow z pliku %s.\n",
		 user->id.user,count,filename);
      fclose(f);
    }
  } else if (xmlnode_get_tag(x,"random_stream")) {
    int len=j_atoi(xmlnode_get_attrib(x,"len"),1);
    char* temp;
    if (!ret) {
      maxsize=j_strlen(prepbuf)+len;
      ret=pmalloc(p,maxsize);
    }
    temp=ret+snprintf(ret,maxsize,"%s",prepbuf);
    for (;temp<ret+maxsize-1;temp++) *temp=random()%255+1;
    *temp=0;
  } else snprintf(ret,maxsize,"");
  return ret;
}

/* Zestaw handlerow eventow czasowych */
/* Sluza one symulacji dzialan rzeczywistego uzytkownika, dodaje sie je do 
   repeatera, ustawia odpowiednia czestotliwosc i powinno zaskoczyc. Czestatliwosci
   sa przyblizone, bo repeater specjalnie sam wprowadza losowe odchylenia. */

/* Probujemy nawiazac polaczenie z serwerem, jesli jeszcze go nie ma */
int ev_connect_count=0;
void ev_connect(event_repeater ev,user_properities user, struct timeval* curtime) {
  //First connect
  if (!user->conn) {
    xmlnode data=(xmlnode)(ev->arg);
    user->digest_auth=(xmlnode_get_tag(data,"digest")!=NULL);
    user_connect(user);
    ev_connect_count++;
  }
}

/* Jesli jest polaczenie z serwerem to je natychmiast zamykamy */
int ev_kill_connection_count=0;
void ev_kill_connection(event_repeater ev,user_properities user, struct timeval* curtime) {
  if (user->conn) {
    user->flags|=UPF_EXPECT_KILL; //Ustawiamy flage
    user_conn_kill(user);
    ev_connect_count++;
  }
}

/* Dodajemy nowego uzytkownika do rostera. W zaleznosci od konfiguracji
 nowy uzytkownik moze byc tworzony na kilka sposobow - patrz makeUserName_from_xml */
int ev_add_roster_count=0;
void ev_add_roster(event_repeater ev,user_properities user, struct timeval* curtime) {
  if (user->roster_hash) { //Dopoki nie pociagnie rostera dopoty nie bedzie probowal nic dodac.
    char jid_full[2000]="";
    xmlnode data=(xmlnode)(ev->arg);
    xmlnode user_node=xmlnode_get_tag(data,"user");
    int max_roster_count=
      j_atoi(xmlnode_get_tag_data(data,"max_roster_count"),1000000);
    if (user->roster_count>=max_roster_count) {
      user_debug(user,ROST_DL,"Limit reached, add_roster skipped\n");
      return;
    }
    user_debug(user,ROST_DL,"User %s: Adding new roster item: ",user->id.user);
    if (user_node && makeUserName_defined_in_xml(jid_full,sizeof(jid_full),user_node,user)) {
      roster_item ri=xhash_get(user->roster_hash,jid_full);
      if (!ri) {
	//Wysylamy xml'a
	if (!user_do_adddel_roster_item(user,curtime,jid_full,jid_full,0)) {
	  ev_add_roster_count++;

	  /* Nie dodajemy tego teraz. Potwierdzenie to zrobi
	  //Dodajemy do lokalnej struktury
	  ri=roster_item_create(jid_full,NULL,SUBS_NONE,user->priv_rost.priv_rost_list);
	  xhash_put(user->roster_hash,ri->jid,ri);
	  user->roster_count++;
	  glob_roster_count++;
	  */
	  user_debug(user,ROST_DL,"OK(%i)\n",user->roster_count);

	  // Na razie ze wzgledu na zgodnosc z orginalnym Kontaktem, nie czekamy na potwierdzenie, tylko
	  // od razu slemy presence'a
	  user_do_presence(user, curtime, "subscribe", jid_full, user->Status, user->Show);
	} 
      }
      else user_debug(user,ROST_DL,"%s already in roster.Skipped\n",jid_full);
    }
    else user_debug(user,ROST_DL,"Nobody to add ...\n");
  }
}

/* Usuwamy losowego usera z rostera */
int ev_del_roster_count=0;
void ev_del_roster(event_repeater ev,user_properities user, struct timeval* curtime) {
  if (user->roster_hash && user->roster_count>0) { //Usuwamy tyklko, jak mamy co ...
    //Losujemy z rostera
    int num=random()%user->roster_count;
    roster_item ri=xhash_get_nth(user->roster_hash,num);
    user_debug(user,DET_DL,"Wylosowany user: %s\n",ri->jid);
    user_debug(user,ROST_DL,"User %s: Deleting roster item ...",user->id.user);
    //Wysylamy xml'a
    if (!user_do_adddel_roster_item(user,curtime,ri->jid,NULL,1)) {
      ev_del_roster_count++;
      
      /* Nie robimy tego teraz. Potwierdzenie samo to zrobi za nas. 
      //Usuwamy z rostera
      xhash_zap(user->roster_hash,ri->jid);
      roster_item_delete(ri);
      user->roster_count--;
      glob_roster_count--; */
      user_debug(user,ROST_DL,"OK(%i)\n",user->roster_count);
    }
  }
}

/* Wysylamy wiadomosc do uzytkownika. TODO: Powinien wybierac losowego
   ze swojego rostera, ewentualnie z okreslonej przestrzeni uzytkownikow, a nie
   tak sztywno jak teraz. */
int ev_send_message_count=0;
void ev_send_message(event_repeater ev,user_properities user, struct timeval* curtime) {
  char jid_full[2000]="";
  pool p=pool_new();
  xmlnode data=(xmlnode)(ev->arg);
  xmlnode user_node=xmlnode_get_tag(data,"user");
  if (user_node && makeUserName_defined_in_xml(jid_full,sizeof(jid_full),user_node,user)) {
    char* message;
    message=makeBytes_defined_in_xml(user,p,0,0,data,jid_full);
    if (!user_do_send_message(user,curtime,jid_full,message)) ev_send_message_count++;
  }
  else user_debug(user,INFO_DL,"Nobody to send to ...\n");
  pool_free(p);
}

int ev_send_raw_bytes_count=0;
void ev_send_raw_bytes(event_repeater ev,user_properities user, struct timeval* curtime) {
  pool p=pool_new();
  xmlnode data=(xmlnode)(ev->arg);
  char* bytes=makeBytes_defined_in_xml(user,p,0,0,data,NULL);
  user_debug(user,INFO_DL,"User %s: Sending raw bytes(%s) !\n",user->id.user,bytes);
  user_do_send_raw_bytes(user,curtime,bytes);
  pool_free(p);
  user->flags|=UPF_EXPECT_KILL; //Ustawiamy flage
}

int ev_change_status_count=0;
void ev_change_status(event_repeater ev,user_properities user, struct timeval* curtime) {
  xmlnode data=(xmlnode)(ev->arg);
  user->Status=xmlnode_get_tag_data(data,"status");
  user->Show=xmlnode_get_tag_data(data,"show");
  if (!(user->Status && user->Show)) {
    //Brakuje konfiguracji przynajmniej jednego z elementow, wiec go losujemy
    const int liczba_statusow=sizeof(statusy)/sizeof(statusy[0])-1;
    int r=random()%(liczba_statusow);
    //printf("liczba_statusow:%i  Wylosowany:%i\n",liczba_statusow,r);
    if (!user->Status) user->Status=statusy[r].Status;
    if (!user->Show)   user->Show=  statusy[r].Show;
  }
  
  if (!user_do_presence(user,curtime,NULL,NULL,user->Status,user->Show)) ev_change_status_count++;
}

int ev_logout_count=0;
void ev_logout(event_repeater ev,user_properities user, struct timeval* curtime) {
  xmlnode iq_list,iq_num;
  user_debug(user,INFO_DL,"User %s: ev_logout\n",user->id.user);
  if (user->priv_rost.loaded) {
    user->priv_rost.numextcontacts++; //Symulujemy zwiekszenie wersji
    iq_list=priv_roster_list2xml(user);
    iq_num=priv_roster_numextcontacts2xml(user);
    user_debug(user,PROST_DL,"%s\n",xmlnode2str(xmlnode_get_tag(xmlnode_get_tag(iq_list,"query"),"wpmsg")));
    user_debug(user,PROST_DL,"%s\n",xmlnode2str(xmlnode_get_tag(xmlnode_get_tag(iq_num,"query"),"wpmsg")));
    jpolld_conn_write(user->conn, iq_list, 1);
    jpolld_conn_write(user->conn, iq_num, 1);
  }
  jpolld_conn_write_str(user->conn, "</stream:stream>",1);
  user_finish_conn(user,"Logged out");
  //TODO:
  //user_store_xml_info(user,id,handle_login_response,curtime);
  //user_store_xml_info(user,id,handle_login_response,curtime);
}

/* ********************************************************************************* */
/* ********************************************************************************* */
/* ********************************************************************************* */


// Na podstawie podanej listy repeaterow tworzymy blizniacza liste.
// Uwaga: kopiowane sa wszystkie wskazniki 9oprocz next), tak wiec
// w nowej kolejce wskazuja one na te same obszary pamieci !
//  Ma to znaczenie glownie przy uzywaniu pola arg, ktore jest
// zawsze wspolne.
void copy_repeaters(pool p, event_repeater* plist_to, event_repeater listfrom) {
  for (;listfrom;listfrom=listfrom->next) {
    (*plist_to)=(event_repeater)pmalloc(p,sizeof(_event_repeater));
    *(*plist_to)=*listfrom;
    (*plist_to)->activation_tm+=
      ((*plist_to)->frequency>0)?
      (long long)1000*(random()%((*plist_to)->frequency/1000)):1; //Dodajemy losowe, inicjalne przesuniecie w czasie
    plist_to=&((*plist_to)->next);
  }
}

//Na podstawie wczytanego kawalka xml'a konfiguracyjnego
// tworzymy nowy element, ktory mozna pozniej dodac
// do listy repeaterow.
// Procedura zajmuje sie rozpoznawaniem roznych typow repeaterow (eventow) - zawiera
// ich deklaracje - 
// i w zaleznosci od tego w rozny sposob allokuje dane i przydziela handlery.
#define BEGIN_REPEATERS if (0) {
#define REPEATER(name,par_handler,par_arg,us_state)} else if (j_strcmp(ev_name,name)==0) {\
    repeater->event_handler=par_handler;\
    repeater->arg=par_arg;\
    repeater->user_state=us_state;
#define END_REPEATERS }

#define REP_FL_ALWAYS 0x01

event_repeater create_repeater_from_xml(xmlnode xml_repeater, struct timeval* curtime) {
  char* ev_name=xmlnode_get_tag_data(xml_repeater,"name");
  event_repeater repeater=(event_repeater)pmalloc(main_config.p,sizeof(_event_repeater));
  repeater->frequency=(long long)1000*(long long)j_atoi(xmlnode_get_tag_data(xml_repeater,"frequency"),-1);
  repeater->counter=j_atoi(xmlnode_get_tag_data(xml_repeater,"counter"),-1);
  //Przeliczylismy [ms] na [us]
  repeater->event_handler=0;
  repeater->next=0;
  repeater->arg=0;
  repeater->activation_tm=ut(*curtime);
  repeater->user_state=UP_LOGGED_IN;
  debug(WARN_DL,"Mamy event %s (freg:%i) !\n",ev_name,(int)(repeater->frequency/1000));

  //Tutaj mamy deklaracje wszystkich rozpoznawalnych po nazwie eventow
  BEGIN_REPEATERS;
  // Repeater zmieniajacy status
  REPEATER("change_status",  ev_change_status,   xmlnode_dup_pool(main_config.p,xml_repeater), UP_LOGGED_IN);
  // Repeater wysylajacy wiadomosc
  REPEATER("send_message",   ev_send_message,    xmlnode_dup_pool(main_config.p,xml_repeater), UP_LOGGED_IN);
  // Repeater wysylajacy hardcore...
  REPEATER("send_raw_bytes", ev_send_raw_bytes,  xmlnode_dup_pool(main_config.p,xml_repeater), UP_EVER);
  //Repeater dodajacy usera do rostera
  REPEATER("add_roster",     ev_add_roster,      xmlnode_dup_pool(main_config.p,xml_repeater), UP_LOGGED_IN);
  //Repeater usuwajacy losowego usera z rostera (bez argumentow)
  REPEATER("del_roster",     ev_del_roster,      NULL,                                         UP_LOGGED_IN);
  //Repeater logujacy - na razie nie zaimplementowany
  REPEATER("connect",        ev_connect,         xmlnode_dup_pool(main_config.p,xml_repeater), UP_CAN_LOGIN);
  //Repeater natychmiast zamykajacy polaczenie
  REPEATER("kill_connection",ev_kill_connection, NULL,                                         UP_EVER);
  //Repeater wylogowywujacy - na razie nie zaimplementowany
  REPEATER("logout",         ev_logout,          NULL,                                         UP_LOGGED_IN);
  END_REPEATERS;

  return repeater;
}
