#include "jab_simul-main.h"
//#include "wp_defines.h"

extern int packets_created;
extern int packets_sent;
extern int packets_canceled;
extern int ev_connect_count;
extern int ev_send_message_count;
extern int forwarded_messages_count;
extern int received_messages_count;
extern int received_messages_offline_count;
extern int received_messages_admin_count;
extern int received_presences_count;
extern int ev_change_status_count;
extern int presences_after_roster_count;
extern int ev_add_roster_count;
extern int ev_del_roster_count;
extern int glob_roster_count;
extern int jpolld_conn_create_count;
extern int jpolld_conn_kill_count;
extern int jpolld_conn_unexpected_kill_count;
static int last_diff=0;

#ifdef WORK_AFTER_LOGGING
int do_events = 0;
#else
int do_events = 1;
#endif

int events_start_after = 1;

typedef int (*stat_printf_fun)(int line, const char* fmt, ... );
struct timeval start_time;


#if USE_NCURSES
int screen_printf(int line,const char* fmt, ... ) {
  va_list ap;
  va_start(ap, fmt);
  setsyx(line,0);
  return vwprintw(screen,(char*)fmt,ap);
}
#endif

int stat_printf(stat_printf_fun sprf, struct timeval* curtime) {
  long long usecs=ut(*curtime)-ut(start_time);
  long secs,mins,hours/*,days*/;
  secs=usecs/1000000;
  mins=secs/60;secs=secs%60;
  hours=mins/60;mins=mins%60;

  /* if established == */
  if (jpolld_conn_create_count-jpolld_conn_kill_count > events_start_after) {
	do_events = 1;
	events_start_after = -1;
  }

  //days=hours/24;hours=hours%24;
  sprf(0,"%02i:%02i.%02i\n",hours,mins,secs);
  sprf(1,"Conn stat:  conns:  total: %i\t estabilished: %i  \n",
       jpolld_conn_create_count,jpolld_conn_create_count-jpolld_conn_kill_count);
  sprf(2,"            kills:  total: %i\t unexpected:   %i\n",
       jpolld_conn_kill_count,jpolld_conn_unexpected_kill_count);
  sprf(3,"Messages:   tot.sent:     %i\t tot.rcvd:   %i\n",
       ev_send_message_count+forwarded_messages_count,received_messages_count);
  sprf(4,"            rcvd.offline: %i\t rcvd.admin: %i\n",
       received_messages_offline_count,received_messages_admin_count);
  sprf(5,"            rcvd.normal:  %i\t fwd: %i\t avg.time:   %i [ms]  \n",
       received_messages_count-received_messages_offline_count-received_messages_admin_count,
       forwarded_messages_count,(int)(get_div_mesg_stat()/(1000*2)));
  sprf(6,"            diff check:   %i\t stability:  %i\n",last_diff,(int)get_diff_stat());
  sprf(7,"Roster:     tot.adds:     %i\t avg.time:   %i [ms]  \n",
       ev_add_roster_count,(int)(get_div_stat(RADD_STAT)/1000));
  sprf(8,"            tot.dels:     %i\t avg.time:   %i [ms]\t glob_rost: %i \n",
       ev_del_roster_count,(int)(get_div_stat(RDEL_STAT)/1000),glob_roster_count);
  sprf(9,"Presences:  tot.sent:     %i\t tot.rcvd:   %i\n",
       ev_change_status_count+presences_after_roster_count,received_presences_count);
  sprf(10,"Packets:    created:      %i\t     sent:   %i\n",
       packets_created,packets_sent);
  sprf(11,"           canceled:     %i\tin queues:   %i  \n",
       packets_canceled,packets_created-(packets_sent+packets_canceled));
  sprf(12,"   --------------\n");
  return 0;
}

jpolld_thread thread_init(int id, int rport, jpolld_instance i) {
  int j;
  pool p;
  jpolld_thread t;

  p =  pool_new();
  t = pmalloco(p, sizeof(_jpolld_thread));
  /* XXX this doesn't work in a threaded world */
  t->id = id;
  t->i = i;
  t->p = p;
  t->mpfd = 1;
  add_conns(t);
  for(j = 0; j<MAX_PFDS;j++) {
    t->pfds[j].fd = -1;
    t->pfds[j].events = 0;
    t->conns[j] = NULL;
  }
  t->sid = NULL;
  return t;
}

void handle_POLLERR(jpolld_thread t, int j, struct timeval* curtime) {
  // Jesli socket sie popsul ...
  int error;
  int err_size=sizeof(error);
  if (getsockopt( t->pfds[j].fd, SOL_SOCKET, SO_ERROR, &error, &err_size) < 0) error=errno;
  debug(ERR_DL,"POLLERR: %s\n",strerror(error));

  if (error == EWOULDBLOCK || error == EAGAIN || error==EINPROGRESS )
    //Falszywy alarm, po prostu jest zamielony albo co
    return;

  if (t->conns[j]->sstate==sock_CONN_IN_PROGRESS) {
    //accept_connected(t->pfds[j].fd,&t->conns[j]->sstate);//Sprawdzi nam errory
    if (t->conns[j]->user && (t->conns[j]->user->flags&UPF_EXIT_AFTER_LOGIN))
      exit_app(0);
  }
  jpolld_conn_kill(t->conns[j]);
}

void handle_POLLOUT(jpolld_thread t, int j, struct timeval* curtime) {
  // Jesli mozemy cos wyslac do socketa ...

  if (t->conns[j]->sstate==sock_CONN_IN_PROGRESS) {
    //Jesli przyszly jakies dane, to znaczy, ze nie blokujacy connect powiodl sie
    t->conns[j]->sstate=sock_CONNECTED;
    debug(INFO_DL,"Asynchronous connection OK\n");

    if (t->conns[j]->ssl)
      jab_ssl_connect(t->conns[j]->ssl);

    return;
    //Powinien byc return, bo na read moze wyskoczyc blad "Resource temporarily unavailable"
  }

  SpUs_logwriteuser(t->conns[j],"*** event POLLOUT(W)\n");     /* sielim PATCH */
  debug(DET_DL,"[%li:%li] some event (OUT) on fd=%i\n",
	curtime->tv_sec,curtime->tv_usec,
	t->pfds[j].fd);
  _jpolld_conn_write(t->conns[j]); // Funkcja moze killowac polaczenie !
}

void handle_POLLIN(jpolld_thread t, int j, struct timeval* curtime) {
  // Jesli cos przyszlo do socketa ...
  char line[1024];
  int n;
  int blocked = 0;
  int maxlen=1023;
  if (t->conns[j]->sstate==sock_CONN_IN_PROGRESS) {
    //Jesli przyszly jakies dane, to znaczy, ze nie blokujacy connect powiodl sie
    t->conns[j]->sstate=sock_CONNECTED;
    debug(INFO_DL,"Asynchronous connection OK\n");

    if (t->conns[j]->ssl)
      jab_ssl_connect(t->conns[j]->ssl);

    return;
    //Powinien byc return, bo na read moze wyskoczyc blad "Resource temporarily unavailable"
  }
  SpUs_logreaduser(t->conns[j],"*** event POLLIN(R)\n");     /* sielim PATCH */

  debug(DET_DL,"[%li:%li] some event (IN) on fd=%i\n",    /* sielim PATCH */
	curtime->tv_sec,curtime->tv_usec,
	t->pfds[j].fd);
  memset(line, '\0', maxlen+1);                           /* dpsm PATCH */

  if (t->conns[j]->ssl)
    n = jab_ssl_read(t->conns[j]->ssl, line, maxlen, &blocked);
  else
    n = read(t->pfds[j].fd, line, maxlen);                  /* dpsm PATCH */

  if( (n <= 0) && (!blocked) ) {
    user_finish_conn(t->conns[j]->user,n<0 ? strerror(errno):"0 bytes read");
    //t->conns[j]->state=state_CLOSING;
  } else {
    SpUs_logreaduser(t->conns[j],"*** %i bytes read:\n%s\n",n,line);
    xstream_eat(t->conns[j]->xs, line, n);
    if (t->conns[j]->user && t->conns[j]->user->id.user &&
	t->conns[j]->user->id.user[0] && (t->conns[j]->user->flags&UPF_SNIFF) ) {
      char buf[400];
      FILE* logfile;
      sprintf(buf,"%s/%s_"FR_SERV"server.log",t->conns[j]->user->sniff_dir,t->conns[j]->user->id.user);
      logfile=fopen(buf,"a+");
      if (logfile) {
	fprintf(logfile,"%s\n************************\n",line);
	fclose(logfile);
      }
    }
  }
}

void *thread_main(void *arg) {
  long long info_timestamp=0;
  long long loginfo_timestamp=0;
  long long users_check_timestamp=0;
  jpolld_thread t = (jpolld_thread)arg;
  int j;
  int nready;
  next_pfd cur_npfd;
  /*sielim PATCH begin*/
  struct timeval curtime;
  debug(INFO_DL,"\n\n*********************\n\n        Begin loop\n\n*********************\n\n");
  /*sielim PATCH end*/
  gettimeofday(&start_time,NULL);

  /* XXX Move this to the master thread */
  while(1) {
    //pool_stat(1);
    //nready=my_poll(t);
    nready = poll((struct pollfd *)&(t->pfds), t->mpfd + 1, LOOP_TIMO);
    gettimeofday(&curtime,NULL);
    {
      // ************************************************************
      // Zadanie pierwsze - niskopoziomowe - sprawdzic co sie dzieje
      // na socketach:
      //  a)odebrac dane z socketow
      //  b)wyslac dane, ktore siedza w buforach, jesli mozna
      // ************************************************************
      if(nready < 0) {
	/* sielim PATCH */
	if (errno==EINTR) {
	  fprintf(stderr,"[%d] Ignoring interrupt while poll (%s)\n",t->id,strerror(errno));
	  continue;
	}
	if (errno==ENOMEM) {
	  fprintf(stderr,"[%d] Ignoring error while poll (%s)\n",t->id,strerror(errno));
	  usleep(500000);
	  continue;
	}
	fprintf(stderr,"[%d] Poll failed: %s\n",t->id,strerror(errno));
	break;
      }
      else if (nready>0) for(j = 2; j<t->mpfd+1; j++) {
	if(t->pfds[j].fd == -1)
	  continue;
	/*
	printf("IN: %i->%i OUT: %i->%i\n",
	       t->pfds[j].events&POLLIN,t->pfds[j].revents&POLLIN,
	       t->pfds[j].events&POLLOUT,t->pfds[j].revents&POLLOUT);*/
	//UWAGA : W ponizszej sekwencji nie nalezy zdejmowac 'else'. W jednym przebiegu petli na
	// jednym polaczniu nie powinno byc obslugiwanych kilka operacji jednoczesnie, bo jedna
	// moze drugiej usunac polaczenie. Poza tym write'y do socketa powinny miec wyzszy priorytet
	// niz read'y (zeby sie nie zapychal)
	if(t->pfds[j].revents & (POLLNVAL | POLLHUP | POLLERR)) {
	  handle_POLLERR(t,j,&curtime);
	  if(--nready <= 0) break;
	}
	else if(t->pfds[j].revents & POLLOUT) {
	  handle_POLLOUT(t,j,&curtime);
	  if(--nready <= 0) break;
	}
	else if(t->pfds[j].revents & POLLIN) {
	  handle_POLLIN(t,j,&curtime);
	  if(--nready <= 0) break;
	}
      }
    } // Koniec zadania pierwszego
    if (ut(curtime)-users_check_timestamp > USERS_CHECK_EVENTS_INTERVAL*1000) {
      // ************************************************************
      // Zadanie drugie - wysokopoziomowe - wykonac zadania uzytkownikow
      //  a) logowanie/wylogowywanie
      //  b) wysylanie wiadomosci, presence, zadania o rooster
      //  c) symulacja wszelkich innych zadan
      // ************************************************************

      user_properities user;
      for (user=alive_users_list;user;user=user->next) {
	user_do_your_job(user,&curtime);
      }
      users_check_timestamp=ut(curtime);
    }
    if (ut(curtime)-info_timestamp > INFO_STAT_INTERVAL*1000) {
      int diff=ev_send_message_count+forwarded_messages_count-(received_messages_count-received_messages_admin_count);
      add_diff_stat(diff-last_diff);
      last_diff=diff;
#if USE_NCURSES
      mvwprintw(screen,0,0,"");
      stat_printf(screen_printf,&curtime);
      refresh();
      wrefresh(msgwin);
#else
      {
	char buf[2000];
	char *spcs=
	  "                                                                                             "
	  "                                                                                             ";
	snprintf(buf,sizeof(buf)," conns:%i kills:%i msgs:s%i-r%i=%i-o%i a%i(%i [ms]) rost_add:%i rost_del:%i",
		 jpolld_conn_create_count,jpolld_conn_kill_count,
		 ev_send_message_count,
		 received_messages_count,
		 ev_send_message_count-(received_messages_count-received_messages_offline_count),
		 received_messages_offline_count,
		 received_messages_admin_count,
		 (int)(get_div_mesg_stat()/(1000*2)),
		 ev_add_roster_count,ev_del_roster_count);
	debug(STATUS_DL,"%s\r",buf);
	fflush(stdout);
	debug(STATUS_DL,"%s\r",spcs+strlen(spcs)-strlen(buf));
      }
#endif
      info_timestamp=ut(curtime);
    }
    if (ut(curtime)-loginfo_timestamp > LOGINFO_STAT_INTERVAL*1000) {
      time_t tim=time(NULL);
      //log_debug(0,"%s",ctime(&tim));
      //stat_printf(log_debug,&curtime);
      loginfo_timestamp=ut(curtime);
    }
    usleep(5000);
  }
  pool_free(t->conns[1]->p);
  for(j = 2; j<t->mpfd+1; j++) {
    if(t->conns[j])
      pool_free(t->conns[j]->p);
  }
  while(t->npfd) {
    cur_npfd = t->npfd;
    t->npfd = cur_npfd->next;
    free(cur_npfd);
  }
  fprintf(stderr, "[%d] Closing down thread\n", t->id);
  pool_free(t->p);

  return 0;
}
