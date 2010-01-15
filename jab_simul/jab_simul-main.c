#include "jab_simul-main.h"
#include <regex.h>
//#include "karma.h"
/* sielim PATCH begin */
/* ************************************** */
const char* changelog=
#include "changelog.inc"
;
jpolld_thread main_thread=NULL;
jpolld_instance main_instance=NULL;

#if USE_NCURSES
WINDOW *msgwin=NULL;
#endif

static FILE* debug_file=NULL;
int log_debug(int line,const char *msgfmt, ...) {
  //Pierwszy parametr ignorujemy
  int ret=0;
  va_list ap;
  if (!debug_file) {
    char name[1000];
    sprintf(name,"jab_simul%u.log",getpid());
    debug_file=fopen(name,"w+");
  }
  if (debug_file) {
    va_start(ap, msgfmt);
    ret=vfprintf(debug_file, msgfmt, ap);
  }
  return ret;
}

void delete_users_properities(jpolld_instance ins);

void show_users_stat_info(jpolld_instance ins) {
  int j;
  if (DEBUG_LEVEL!=MONIT_DL) fprintf(stderr,"User name              /\tSent/\tRec./\tTime[ms]/\tLast10[ms]\tPresRcvd\n");
  for (j=0;j<MAX_USERS;j++) if (ins->users[j]) {
    user_properities us=ins->users[j];
    long long rec_time=us->received_msgs_time_sum/(us->received_msgs_count?us->received_msgs_count:1);
    if (DEBUG_LEVEL!=MONIT_DL) fprintf(stderr,"%25s/\t% 4i/\t% 4i/\t% 6i/\t% 6i\t% 6i\n",us->id.user,0,
				       us->received_msgs_count,(int)(rec_time/1000),0,us->received_presences_count);
  }
}

void exit_app(int code) {
  if (main_instance) {
    delete_users_properities(main_instance);
    free(main_instance);
  }
#if USE_NCURSES
  endwin();
#endif
#ifdef POOL_DEBUG
  pool_stat(1);
#endif
  exit(code);
}

#ifdef POOL_DEBUG
void debug_log(char *zone, const char *msgfmt, ...) {
  va_list ap;
  
  va_start(ap, msgfmt);
  vfprintf(stderr, msgfmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
}
#endif

struct config_struct main_config;

#define MAX_TIMERS 10
#define MaxRegExMatches 20

int check_timer(int no,int interval,struct timeval* time) {
  // no - 0..MAX_TIMERS-1
  // interval - ms
  static struct timeval timer[MAX_TIMERS]={{-1,-1}};
  struct timeval curtime;
  long long diff;
  int retval;
  if (no<0 || no>=MAX_TIMERS) return 0; //ERROR
  if (timer[0].tv_sec==-1) {
    int i;
    for (i=0;i<MAX_TIMERS;i++) {
      timer[i]=(struct timeval){0,0};
    }
  }
  /********** Wlasciwa procedura sprawdzania czasu *************/
  if (!time) {
    gettimeofday(&curtime,NULL);
    time=&curtime;
  }
  diff=(long long)time->tv_sec * 1000 - (long long)timer[no].tv_sec * 1000 +
    time->tv_usec/1000 - timer[no].tv_usec/1000;
  if (diff>=(long long)interval) {
    retval=1;
    timer[no]=*time;
  } else retval=0;
  return retval;
}
int deb4357xtyf;
/* ************************************** */
/* sielim PATCH end */

void *thread_main(void *arg);
jpolld_thread thread_init(int id, int rport, jpolld_instance i);

/* sielim PATCH begin */
static void (*orig_SIGPIPE_handler)(int)=NULL;
static void (*orig_SIGSEGV_handler)(int)=NULL;
static void (*orig_SIGUSR1_handler)(int)=NULL;

void sigerr_handler(int sig) {
  debug(DET_DL,"A!\n");
  switch (sig) {
  case SIGTERM:
    syslog(LOG_INFO,"Received SIGTERM, exitting !");
    exit_app(1);
  case SIGINT:
    syslog(LOG_INFO,"Received SIGINT, exitting !");
    exit_app(1);
  case SIGPIPE:
    syslog(LOG_ERR,"Received SIGPIPE(%i,%p) !",sig,orig_SIGPIPE_handler);
    if (orig_SIGPIPE_handler) {
      printf("Running orginal SIGPIPE handler\n");
      orig_SIGPIPE_handler(sig);
    }
    break;
  case SIGSEGV:
#if USE_NCURSES
    endwin();
#endif
    syslog(LOG_CRIT,"Received SIGSEGV(%i,%p), exitting !",sig,orig_SIGSEGV_handler);
    signal(SIGSEGV,orig_SIGSEGV_handler);
    break;
  case SIGUSR1:
    show_users_stat_info(main_instance);
    if (orig_SIGUSR1_handler) {
      printf("Running orginal SIGUSR1 handler\n");
      orig_SIGUSR1_handler(sig);
    }
    break;
  default:
    syslog(LOG_ERR,"Received some bad signal(%i), exitting !",sig);
    exit_app(1);
  }
}
/* sielim PATCH end */

extern int nms_init(char*,char*,int,int); 
extern int yyparse();

int sprintf_pattern(char* buf, int buf_size,char* pattern,
		    int idnum,char* input_data_string,regmatch_t* input_matches_table) {
  nms_init(pattern,buf,buf_size,idnum);
  yyparse();
  debug(DET_DL,"%i: %s\n",idnum,buf);
  return 0;
}

int generate_user_names_space(jpolld_instance ins,xmlnode xcfg) {
  int j;
  xmlnode cur;
  s_tag_iter un_iter;
  /* Zerujemy */
  for (j=0;j<MAX_USERS;j++) {
    ins->user_names_space[j].user[0]=0;
    ins->user_names_space[j].server[0]=0;
    ins->user_names_space[j].enabled=0;
  }
  /* Generacja przestrzeni userow */
  
  tag_iter_init(&un_iter,xcfg,"user_names_generator");
  while ((cur=tag_iter_get_next(&un_iter))) {
    int range_from=0;
    int range_to=-1;
    // Odkad dokad (identyfikatory=indeksy w tablicy)
    xmlnode range=xmlnode_get_tag(cur, "range");
    char* name_pattern=xmlnode_get_tag_data(cur, "name");
    char* server_pattern=xmlnode_get_tag_data(cur, "server");
    debug(DET_DL,"Generator xmlnode:\n%s\n",xmlnode2str(cur));
    if (range) {
      range_from = j_atoi(xmlnode_get_tag_data(range, "from"),range_from);
      range_to = j_atoi(xmlnode_get_tag_data(range, "to"),range_to);
    }
    // TODO: Dorobic wczytywanie listy z pliku
    if (name_pattern && range_to>-1) {	  
      int i;
      for (i=range_from;i<=range_to;i++) {
	_user_name* us_name=&ins->user_names_space[i];
	sprintf_pattern(us_name->user,sizeof(us_name->user),name_pattern,i,NULL,NULL);
	if (server_pattern)
	  sprintf_pattern(us_name->server,sizeof(us_name->server),server_pattern,i,NULL,NULL);
	us_name->enabled=1;
	us_name->can_receive_message=1;
	us_name->can_be_added_to_roster=1;
      }
    }
  }
  return 0;
}

user_properities alive_users_list=NULL;

int set_users_properities(jpolld_instance ins,xmlnode xcfg) {
  int users_count=0; 
  //typedef struct tmpevstr {void* arg;struct tmpevstr* next;} tmpevstr;
  //TODO: To jest tymczasowe, trzeba to bedzie jakosc globalniej trzymac
  // i troche inaczej naliczac.
  struct timeval curtime;
  int j;
  s_tag_iter up_iter;
  xmlnode cur;
  gettimeofday(&curtime,NULL);
  /* Zerujemy */
  for (j=0;j<MAX_USERS;j++) ins->users[j]=NULL;
  /* Ustawianie wlasciwosci userow */
  tag_iter_init(&up_iter,xcfg,"user_properities");
  while ((cur=tag_iter_get_next(&up_iter))){
    s_tag_iter ev_iter;
    regex_t c_nameregexp;  
    regmatch_t name_matches[MaxRegExMatches];
    regex_t c_serverregexp;  
    regmatch_t server_matches[MaxRegExMatches];
    // Ktorych to dotyczy userow ?
    xmlnode filter=xmlnode_get_tag(cur, "filter");
    xmlnode properities=xmlnode_get_tag(cur, "properities");
    char* filter_name_regexp=".*";
    char* filter_server_regexp=".*";
    int filter_range_from=0;
    int filter_range_to=MAX_USERS;
    // Rozne wlasciwosci
    char* passwd_pattern;
    char* resource_pattern;
    char* fullname_pattern;
    char* host_pattern; //Moze nie byc, wtedy host bedzie taki jak w jid`zie
    int ssl;
    int port;
    // Timeouty
    int connection_timeout=-1;
    int login_timeout=-1;
    // Flagi
    int flags=0;
    // Katalog z logami socketow
    char* sniff_dir;
    // Zmienne sterujace odpowiadaniem na wiadomosci
    float msg_resp_propab;
    //char* resp_pattern;
    //Eventy czasowe
    event_repeater ev_list=NULL;
    xmlnode xml_repeater;
    event_repeater* ev_list_cur=&ev_list;
    
    if (filter) {
      char *tmp;
      xmlnode range=xmlnode_get_tag(filter,"range");
      tmp=xmlnode_get_tag_data(filter,"name");
      if (tmp) filter_name_regexp=tmp;
      tmp=xmlnode_get_tag_data(filter,"server");
      if (tmp) filter_server_regexp=tmp;
      if (range) {
	filter_range_from=j_atoi(xmlnode_get_tag_data(filter,"from"),filter_range_from);
	filter_range_to=j_atoi(xmlnode_get_tag_data(filter,"to"),filter_range_to);
      }
    }
    if (regcomp(&c_nameregexp,filter_name_regexp,REG_EXTENDED)) {
      printf("ERROR: Wrong regular expression(%s) in filter !\n",filter_name_regexp); 
      exit_app(1);
    }
    debug(INFO_DL,"Name regex: %s.\n",filter_name_regexp);
    if (regcomp(&c_serverregexp,filter_server_regexp,REG_EXTENDED)) {
      printf("ERROR: Wrong regular expression(%s) in filter !\n",filter_server_regexp); 
      exit_app(1);
    }
    debug(INFO_DL,"Server regex: %s.\n",filter_server_regexp);
    // TODO: Dorobic ewentualne inne filtry (np. po numerze ?)
    
    // ************************************ 
    //   Czytamy wlasciwosci danej grupy
    // ************************************ 
    
    passwd_pattern=xmlnode_get_tag_data(properities,"password");
    resource_pattern=xmlnode_get_tag_data(properities,"resource");
    fullname_pattern=xmlnode_get_tag_data(properities,"fullname");
    host_pattern=xmlnode_get_tag_data(properities,"host"); //Moze nie byc
    port=j_atoi(xmlnode_get_tag_data(properities,"port"),DEFAULT_PORT);
    ssl = xmlnode_get_tag(properities,"ssl") ? 1 : 0;
    connection_timeout=j_atoi(xmlnode_get_tag_data(properities,"connection_timeout"),-1);;
    login_timeout=j_atoi(xmlnode_get_tag_data(properities,"login_timeout"),-1);;
    
    // Czy to user testowy do sprawdzania czasu logowania ? 
    if (xmlnode_get_tag(properities,"exit_after_login")) flags|=UPF_EXIT_AFTER_LOGIN;
    // Czy user ma byc na podgladzie ? 
    if (xmlnode_get_tag(properities,"sniff")) {
      flags|=UPF_SNIFF;
      sniff_dir=pstrdup(main_config.p,xmlnode_get_tag_data(properities,"sniff"));
      if (sniff_dir) while (sniff_dir[0] && sniff_dir[strlen(sniff_dir)-1]=='/')
	sniff_dir[strlen(sniff_dir)-1]=0; //Kasujemy wszyskie slashe na koncu
    }
    
    // Konfigurujemy wstepnie ustawione eventy czasowe
    tag_iter_init(&ev_iter,properities,"event");
    while ((xml_repeater=tag_iter_get_next(&ev_iter))) {
      (*ev_list_cur)=create_repeater_from_xml(xml_repeater,&curtime);
      ev_list_cur=&((*ev_list_cur)->next);
    }
    
    // A teraz przepuszczamy cala przestrzen nazw userow przez filtr
    for (j=0;j<MAX_USERS;j++) if (ins->user_names_space[j].enabled && 
				  j>=filter_range_from && j<=filter_range_to &&
				  !regexec(&c_nameregexp,ins->user_names_space[j].user,
					   MaxRegExMatches,name_matches,0) &&
				  !regexec(&c_serverregexp,ins->user_names_space[j].server,
					   MaxRegExMatches,server_matches,0)) {
      //TODO: Powinien polaczyc tablice matches ze wszystkich wyrazen
      user_properities user;
      char passwd_buf[1000]="";
      char resource_buf[1000]="";
      char fullname_buf[1000]="";
      char host_buf[1000]="";
      
      //Przypasowal...
      debug(DET_DL,"Pasuje: %s@%s\n",ins->user_names_space[j].user,ins->user_names_space[j].server);
      // Ustawiamy rozne wlasciwosci
      // Password:
      if (passwd_pattern) sprintf_pattern(passwd_buf, sizeof(passwd_buf),passwd_pattern,j,
					  ins->user_names_space[j].user,name_matches/*TODO*/);	
      // Resource:
      if (resource_pattern) sprintf_pattern(resource_buf, sizeof(resource_buf),resource_pattern,j,
					    ins->user_names_space[j].user,name_matches/*TODO*/);	
      // Fullname:
      if (fullname_pattern) sprintf_pattern(fullname_buf, sizeof(fullname_buf),fullname_pattern,j,
					    ins->user_names_space[j].user,name_matches/*TODO*/);	
      // Host:
      if (host_pattern) sprintf_pattern(host_buf,sizeof(host_buf),host_pattern,j,
					ins->user_names_space[j].user,name_matches/*TODO*/); 
      //Dodajemy nowego usera do aktywnych (symulowanych) nadajac wlasciwosci
      user=user_create(&ins->user_names_space[j],passwd_buf,resource_buf,fullname_buf,
		       host_pattern?host_buf:NULL,port,ev_list, ssl);
      ins->users[users_count++]=user;
      user->state=UP_CAN_LOGIN;
      user->flags=flags;
      user->sniff_dir=sniff_dir;
      user->msg_resp_propab=msg_resp_propab;
      if (connection_timeout!=-1) user->connection_timeout=connection_timeout;
      if (login_timeout!=-1) user->login_timeout=login_timeout;
      debug(DET_DL,"Fullname: %s@%s  Password: %s   Resource: %s  Flags: %4X Timeouts:(%i,%i)\n",
	    user->id.user,user->id.server,user->passwd,user->id.resource,user->flags,
	    user->connection_timeout,user->login_timeout);
      //No i na koniec wklejamy do globalnej listy userow
      user->next=alive_users_list;
      alive_users_list=user;
    }
  }
  return 0;
}

void delete_users_properities(jpolld_instance ins) {
  int j;
  for (j=0;j<MAX_USERS;j++) if (ins->users[j]) user_delete(ins->users[j]);  
}

/**************************************************
 *
 * Configure - Get all our configuration settings
 *      cfgfile - what file to load up
 *
 * I really should put some error checking in =)
 *
 **************************************************/
extern int events_start_after;

int configure(jpolld_instance i,config cfg, char *cfgfile)
{
    xmlnode xcfg, tmp;
    cfg->p = pool_new();
#ifdef POOL_DEBUG
    pool_stat(1);
#endif
    xcfg = xmlnode_file(cfgfile);
    if( !xcfg)
    {
      fprintf(stderr,"Unable to parse config file: %s\n", cfgfile);
      syslog(LOG_ERR,"Unable to parse config file: %s", cfgfile); /* sielim PATCH */
      exit_app(1);
    }
    debug(INFO_DL,"%s Parsed OK\n", cfgfile);

    /* Setup the rate information, or disable it if not found */
    tmp = xmlnode_get_tag(xcfg, "rate");
    if(tmp == NULL)
    {
        cfg->rate_time = -1;
    } else {
        cfg->rate_time = j_atoi(xmlnode_get_tag_data(tmp, "time"), 25);
        cfg->max_points = j_atoi(xmlnode_get_tag_data(tmp, "max"), 5);
    }

    cfg->maxusers = j_atoi(xmlnode_get_tag_data(xcfg, "maxusers"), 1024);
	events_start_after = j_atoi(xmlnode_get_tag_data(xcfg, "events_after"), 1);

    /* sielim PATCH begin */
    cfg->fullxmlscannedloguser[0]=0;
    cfg->fullxmlmasterlogpath[0]=0;
    cfg->fullxmluserlogpath[0]=0;
    cfg->log_route_errors=0;
    cfg->log_full_master_socket=0;
    cfg->log_stat_socket.count=0;
    cfg->log_stat_socket.write_log_interval=SOCKS_STAT_WRITE_LOG_INTERVAL;
    cfg->log_stat_socket.refresh_interval=SOCKS_STAT_REFRESH_INTERVAL;
    {
      xmlnode admin = xmlnode_get_tag(xcfg, "admin");
      if (admin) {
	xmlnode log_route_errors = xmlnode_get_tag(admin, "log_route_errors");
	xmlnode log_full_master_socket = xmlnode_get_tag(admin, "log_full_master_socket");
	xmlnode log_stat_socket = xmlnode_get_tag(admin, "log_stat_socket");
	xmlnode specialusers = xmlnode_get_tag(admin, "specialusers");
	if (log_route_errors) {
	  cfg->log_route_errors=1;
	  debug(WARN_DL,"Logging all route errors !\n");
	}
	if (log_full_master_socket) {
	  cfg->log_full_master_socket=1;
	  debug(WARN_DL,"Logging full master socket !\n");
	}
	if (log_stat_socket) {
	  cfg->log_stat_socket.count = 
	    j_atoi(xmlnode_get_tag_data(log_stat_socket, "count"),0);
	  cfg->log_stat_socket.write_log_interval=
	    j_atoi(xmlnode_get_tag_data(log_stat_socket, "write_log_interval"),
		   SOCKS_STAT_WRITE_LOG_INTERVAL);
	  cfg->log_stat_socket.refresh_interval=
	    j_atoi(xmlnode_get_tag_data(log_stat_socket, "refresh_interval"),SOCKS_STAT_REFRESH_INTERVAL);
	  debug(WARN_DL,"Logging socket statistics ! Count=%i(write:%i,refresh:%i)\n",
		cfg->log_stat_socket.count,
		cfg->log_stat_socket.write_log_interval,
		cfg->log_stat_socket.refresh_interval);
	}
	if (specialusers) {
	  xmlnode fullxmlscan = xmlnode_get_tag(specialusers, "fullxml");
	  if (fullxmlscan) {
	    char* user=xmlnode_get_tag_data(fullxmlscan, "user");
	    char* logmaster=xmlnode_get_tag_data(fullxmlscan, "logmaster");
	    char* loguser=xmlnode_get_tag_data(fullxmlscan, "loguser");
	    debug(WARN_DL,"User %s is beeing sniffed !\n",user);
	    if (user) strcpy(cfg->fullxmlscannedloguser, user);
	    if (logmaster) strcpy(cfg->fullxmlmasterlogpath, logmaster);
	    if (loguser) strcpy(cfg->fullxmluserlogpath, loguser);
	  }
	}
      }
    }
    generate_user_names_space(i,xcfg);
    set_users_properities(i,xcfg);
    init_mesg_stat(200);
    init_diff_stat(20);
    init_stat(RADD_STAT,200);
    init_stat(RDEL_STAT,200);
    /* sielim PATCH end */
    xmlnode_free(xcfg);
    return 0;
}

#if USE_NCURSES
WINDOW* screen=NULL;
#endif

int main(int argc, char *argv[])
{
    int id;
    /* dpms PATCH cut*/
    int fd;
    if (argc>=2 && (!strcmp(argv[1],"-v") || !strcmp(argv[1],"--version"))) {
      printf("jabb_simul - symulator klientow jabbera. Wersja: %s\n"
	     "Type jabb_simul -c to see whole changelog.\n(C) by Wirtualna Polska S.A.\n",JAB_SIMUL_VER);
      exit_app(0);
    } 
    if (argc>=2 && (!strcmp(argv[1],"-c") || !strcmp(argv[1],"--changelog"))) {
      printf("jabb_simul - symulator klientow jabbera. Wersja: %s\nCHANGELOG:"
	     "\n**************************************************\n%s"
	     "\n**************************************************\n"
	     "(C) by Wirtualna Polska S.A.\n",JAB_SIMUL_VER,changelog);
      exit_app(0);
    } 
    openlog("jab_simul",LOG_PID,LOG_DAEMON);                   /* sielim PATCH */

    /* Ustawiamy sygnaly */
    signal(SIGINT, sigerr_handler);                         /* sielim PATCH */
    signal(SIGTERM, sigerr_handler);                        /* sielim PATCH */
    orig_SIGSEGV_handler=signal(SIGSEGV, sigerr_handler);   /* sielim PATCH del */
    orig_SIGPIPE_handler=signal(SIGPIPE, sigerr_handler);   /* sielim PATCH */
    orig_SIGUSR1_handler=signal(SIGUSR1, sigerr_handler);   /* sielim PATCH */

#if USE_NCURSES
    screen=initscr();
    msgwin=newwin(0,0,13,0);
    scrollok(msgwin,1);
#endif

    /*
    Ok so what we want to do here is create a few threads for our groups.  Each
    thread has it's own thread information so that it is independant of the
    others.
    */

    srandom(time(NULL));
    main_instance = malloc(sizeof(_jpolld_instance));
    debug(DET_DL,"Instance allocated ptr: %p\n",main_instance);
    /* dpms PATCH begin */
    configure(main_instance,&main_config, (argc>=2) ? argv[1] : "jab_simul.xml");
    /* dpms PATCH end */

    jab_ssl_init();


    fd=-1;
    /* XXX Make host a valid option */
    id = 0;

    ++id;
    main_thread = thread_init(id, 0, main_instance);
    /* sielim PATCH begin */
#if FORK_AS_DAEMON
    //Zapuszczamy proces dziecka w tle, a matka wraca z komunikatem OK
    {
      int pid=fork();
      if (pid==-1) {
	syslog(LOG_ERR,"Could not start daemon, fork failed (%s) !",strerror(errno));
	return 1; //ERROR
      }
      if (pid>0) return 0;   //Closing parent
      // Here goes a child process
      syslog(LOG_NOTICE,"Daemon started.");
      debug(WARN_DL,"%i\n",getpid());
      thread_main(t);
    }
    syslog(LOG_NOTICE,"Daemon finished.");
#else
    if (DEBUG_LEVEL!=MONIT_DL) syslog(LOG_NOTICE,"Started.");
    debug(WARN_DL,"%i\n",getpid());
    thread_main(main_thread);
    if (DEBUG_LEVEL!=MONIT_DL) syslog(LOG_NOTICE,"Finished.");
#endif
    /* sielim PATCH end */
    delete_users_properities(main_instance);
    free(main_instance);
    pool_free(main_config.p);

    jab_ssl_stop();

   #ifdef MEM_DEBUG
    mem_debug_save_leaks();
    #endif
    return 0;
}
