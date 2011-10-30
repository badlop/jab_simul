#include "jab_simul-main.h"

#define MaxPacksBuffered 10

const char* xstream_typename(int type) {
  switch(type) {
  case XSTREAM_NODE:
    return "XSTREAM_NODE";
  case XSTREAM_ROOT:
    return "XSTREAM_ROOT";
  case XSTREAM_CLOSE:
    return "XSTREAM_CLOSE";
  case XSTREAM_ERR:
    return "XSTREAM_ERR";
  default:
    return "UNKNOWN";
  }
}

int accept_connected(int fd,sock_state *psstate) {
  if (*psstate==sock_CONN_IN_PROGRESS) {
    int error;
    socklen_t errsize=sizeof(error);
    if (getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&errsize)) {
      debug(ERR_DL,"getsockopt error ! %s\n",strerror(errno));
      *psstate=sock_ERROR;
      return -1;
    } else if (error==0) {
      debug(ERR_DL,"Connected OK\n");
      *psstate=sock_CONNECTED;
      return fd;
    } else {
      debug(ERR_DL,"Connect error ! %s\n",strerror(error));
      *psstate=sock_ERROR;
      return -1;
    }
  } else if (*psstate==sock_CONNECTED) {
    debug(ERR_DL,"Strange, but seems to be OK...fd=%i\n",fd);
    return fd;
  } else {
    debug(ERR_DL,"Use make_conn_socket to connect first !\n");
    return -1;
  }
}

int make_conn_socket(u_short port, char *host, sock_state *psstate) {
  int s;
  int flags;
  int cres;
  struct sockaddr_in sa;
  struct in_addr *saddr;

  *psstate=sock_NOT_CONNECTED;
  bzero((void *)&sa,sizeof(struct sockaddr_in));

  if((s = socket(AF_INET,SOCK_STREAM,0)) < 0)
    return(-1);

  flags = fcntl(s, F_GETFL, 0);         // Ustawiamy na nieblokujacy jeszcze przed connectem !!!
  flags |= O_NONBLOCK;
  if(fcntl(s, F_SETFL, flags) <0)
    debug(ERR_DL,"Error setting flags\n");

  saddr = make_addr(host);
  if(saddr == NULL)
    return(-1);
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);

  sa.sin_addr.s_addr = saddr->s_addr;

  cres=connect(s,(struct sockaddr*)&sa,sizeof sa);
  debug(INFO_DL,"connect(%i)->%i%s\n",s,cres,strerror(errno));

  if (cres < 0) {
    if (errno==EINPROGRESS) {
      *psstate=sock_CONN_IN_PROGRESS;
    } else {
      close(s);
      *psstate=sock_ERROR;
      return(-1);
    }
  } else {
    *psstate=sock_CONNECTED;
    debug(ERR_DL,"Connected !\n");
  }
  return(s);
}

/* create a new socket to connect to svc_mux */
int make_clientfd(char *host, int port, sock_state* conn_state) {
  int sockfd = make_conn_socket(port, host, conn_state);
  if(sockfd <= 0) {
    debug(ERR_DL,"Socket creation error ! (%s)\n",strerror(errno));
    return -1;
  }
  return sockfd;
}

void jpolld_conn_node(int type, xmlnode x, void *arg)
{
    jpolld_conn c = (jpolld_conn)arg;
    jpolld_thread t;
    int loc;
    struct timeval curtime;

    t = (jpolld_thread)(c->t);
    loc = c->loc;
    SpUs_logreaduser(c,"*** read all xstream - now is ready to process:\n%s\n",
		     xmlnode2str(x));
    debug(DET_DL,"[%d] We read all xmlnode from %i. Type: %s\n%s\n",
	  t->id,loc,xstream_typename(type),xmlnode2str(x));
    if(loc == 1)
    {
        printf("\n**** CONN 1 GOT A CALLBACK ****\n\n");
    }
    switch(type) {
    case XSTREAM_NODE:
      gettimeofday(&curtime,NULL);
      handle_user_node(x,c->user,&curtime);
      break;
    case XSTREAM_ROOT:
      user_debug(c->user,INFO_DL,"User %s: Odpowiedz od serwera.\nXML: %s\n",
		 c->user->id.user,xmlnode2str(x));
      c->user->sid = pstrdup(c->user->id.p,xmlnode_get_attrib(x,"id"));
      c->user->stream_opened=1;
      break;
    case XSTREAM_CLOSE:
      user_finish_conn(c->user,"XSTREAM_CLOSE");
      user_debug(c->user,CONN_DL,"User %s: Serwer zakonczyl polaczenie.\nXML: %s\n",
		 c->user->id.user,xmlnode2str(x));
      break;
    case XSTREAM_ERR:
      if (c->state == state_CLOSING) {
	user_finish_conn(c->user,"XSTREAM_ERR");
	user_debug(c->user,CONN_DL,"User %s: Serwer zakonczyl polaczenie.\nXML: %s\n",
		   c->user->id.user,xmlnode2str(x));
      } else {
	//To jest jest chyba bardzo powazny blad, oznacza, ze najprawdopodobniej serwer mocno nawalil.
	printf("XSTREAM_ERR\n\r");
	user_debug(c->user,ERR_DL,
		   "ERROR !!!! User %s: Zdaje sie ze serwer przyslal nieprawidlowego xml'a !!!!!!\n",
		   c->user->id.user);
      }
      user_finish_conn(c->user,"XSTREAM_ERR");
      break;
    default:
      break;
    }
    xmlnode_free(x);
}

void add_conns(jpolld_thread t)
{
    int i;
    int more;
    next_pfd cur_npfd;

    more = MAX_PFDS-(t->mpfd) > 10 ? 10 : MAX_PFDS-(t->mpfd);

    for(i = (t->mpfd)+more; i > (t->mpfd); i--)
    {
        cur_npfd = malloc(sizeof(_next_pfd));
        cur_npfd->next = NULL;
        cur_npfd->pos = i;
        if(t->npfd)
        {
            cur_npfd->next = t->npfd;
        }
        t->npfd = cur_npfd;
    }

    return;
}

int jpolld_conn_create_count=0;
jpolld_conn jpolld_conn_create(jpolld_thread t, int loc, user_properities user,
			       sock_state sstate, struct timeval* cr_time) {
  pool p;
  jpolld_conn nc;

  jpolld_conn_create_count++;
  p = pool_new();
  nc = pmalloco(p, sizeof(_jpolld_conn));
  nc->loc = loc;
  nc->p = p;
  nc->xs = xstream_new(nc->p, jpolld_conn_node, nc);
  nc->state = state_UNKNOWN;

  nc->t = t;
  nc->admin_flags=0;                    /* sielim PATCH */
  nc->user = user;                      /* sielim PATCH */
  nc->sstate = sstate;                  /* sielim PATCH */
  nc->xml_need_responses_queue=NULL;    /* sielim PATCH */
  nc->creation_time=*cr_time;           /* sielim PATCH */

  if (user->ssl) {
    nc->ssl = jab_ssl_conn(t->pfds[loc].fd);

    if ((sstate ==sock_CONNECTED) && (nc->ssl)) {
      jab_ssl_connect(nc->ssl);
    }
  }

  return nc;
}

int jpolld_find_conn(jpolld_thread t, int fd)
{
    int j;

    for(j = 0; j<MAX_PFDS; j++)
    {
        if(t->pfds[j].fd == fd)
            return j;
    }

    return -1;
}

int packets_created=0;
int packets_sent=0;
int packets_canceled=0;

void _jpolld_conn_write(jpolld_conn c) {
  int len;
  int blocked = 0;

  if(!c)
    return;
  while (c->writer != NULL) {
    if (c->ssl)
      len = jab_ssl_write(c->ssl, c->writer->cbuffer,
			 strlen(c->writer->cbuffer),
			 &blocked);
    else
      len = write(c->t->pfds[c->loc].fd, c->writer->cbuffer,
		  strlen(c->writer->cbuffer));

    debug(DET_DL, "[%d] We wrote to %i: %s\n", c->t->id, c->loc, c->writer->cbuffer);
    if (c->user && c->user->id.user && c->user->id.user[0] && (c->user->flags&UPF_SNIFF)) {
      char buf[400];
      FILE* logfile;
      sprintf(buf,"%s/%s_"TO_SERV"server.log",c->user->sniff_dir,c->user->id.user);
      logfile=fopen(buf,"a+");
      if (logfile) {
	fprintf(logfile,"%s\n************************\n",c->writer->cbuffer);
	fclose(logfile);
      }
    }
    if(len<=0) {
      if (blocked)
	break;

      if (errno == EWOULDBLOCK)
	break;

      //Cos zlego sie stalo. Zamykamy wszystko od razu.
      jpolld_conn_kill(c);
      break;
    } else {
      if( len < strlen(c->writer->cbuffer)) {
	//Jeszcze nie wszystko weszlo do socketa. Poczekamy na nastepna okazje.
	c->writer->cbuffer += len;
	break;
      } else {
	jpolld_write_buf b;
	//Caly kawalek wszedl do socketa. Byc moze to juz wszystko ? A moze
	// sprobowac wrzucic do socketa nastepny kawalek (powrot petli) ?
	b = c->writer;
	SpUs_logwriteuser(c,"*** %p Buffer sent and deleted\n",b);
	packets_sent++;
	c->writer = b->next;
	pool_free(b->p);
	if (c->writer == NULL) {
	  // Tak, nie memy juz nic wiecej do wyslania.
	  // Mozna zdjac POLLOUT
	  int old=c->t->pfds[c->loc].events;
	  int new=old ^ POLLOUT;
	  SpUs_logwriteuser(c,"*** flags changed %c%c -> %c%c\n",
			    (old & POLLIN)?'R':'-',
			    (old & POLLOUT)?'W':'-',
			    (new & POLLIN)?'R':'-',
			    (new & POLLOUT)?'W':'-');
	  c->t->pfds[c->loc].events ^= POLLOUT;
	  if(c->state == state_CLOSING) {
	    // Teraz dopiero deallokujemy strukture (jak wszystko zostalo juz wyslane)
	    // jesli byla wola zamkniecia polaczenia.
	    if (c->user) c->user->flags|=UPF_EXPECT_KILL; //Ustawiamy flage
	    jpolld_conn_kill(c);
	  }
	}
      }
    }
  }
}

int jpolld_conn_write_str(jpolld_conn c, const char *buf, int force)
     /*! Zwraca -1 jak kolejka jest przepelniona lub inny blad, 0 jak OK */
{
    jpolld_write_buf b;
    jpolld_write_buf cur;
    pool p;

    if(!c)
      return -1;
    packets_created++;
    p = pool_new();
    b = pmalloco(p, sizeof(_jpolld_write_buf));
    b->p = p;
    b->xbuffer = NULL;
    b->cbuffer = b->wbuffer = pstrdup(p, buf);
    b->next = NULL;
    if(c->writer != NULL) { /* if there is alredy a packet being written */
      int count;
      for(cur = c->writer,count=0;cur->next != NULL; cur = cur->next,count++);
      if (count>=MaxPacksBuffered && !force) {
	pool_free(b->p);
	debug(ERR_DL,"Kolejka za dluga, pakiet anulowany !\n");
	packets_canceled++;
	return -1;
      }
      cur->next = b;
      /* add it to the queue */
    }
    else
    { /* otherwise, just make it our current packet */
        c->writer = b;
    }
    {
      int old=c->t->pfds[c->loc].events;
      int new=old | POLLOUT;
      SpUs_logwriteuser(c,"*** flags changed %c%c -> %c%c\n",
			(old & POLLIN)?'R':'-',
			(old & POLLOUT)?'W':'-',
			(new & POLLIN)?'R':'-',
			(new & POLLOUT)?'W':'-');
    }
    //Wlaczamy flage POLLOUT
    c->t->pfds[c->loc].events |= POLLOUT;
    return 0;
}

int jpolld_conn_write(jpolld_conn c, xmlnode x, int force)
     /*! Zwraca -1 jak kolejka jest przepelniona lub inny blad, 0 jak OK */
{
    jpolld_write_buf b;
    jpolld_write_buf cur;
    pool p;

    if(!c)
      return -1;
    packets_created++;
    p = xmlnode_pool(x);
    b = pmalloco(p, sizeof(_jpolld_write_buf));
    b->p = p;
    b->xbuffer = x;
    b->cbuffer = b->wbuffer = xmlnode2str(x);
    b->next = NULL;
    if(c->writer != NULL) { /* if there is alredy a packet being written */
      int count;
      for(cur = c->writer,count=0;cur->next != NULL; cur = cur->next,count++);
      if (count>=MaxPacksBuffered && !force) {
	pool_free(b->p);
	debug(ERR_DL,"Kolejka za dluga, pakiet anulowany !\n");
	packets_canceled++;
	return -1;
      }
      cur->next = b;
      /* add it to the queue */
    }
    else
    { /* otherwise, just make it our current packet */
        c->writer = b;
    }
    SpUs_logwriteuser(c,"*** %p - next write buffer added:\n%s\n",b,b->wbuffer);
    { /* sielim PATCH begin */
      int old=c->t->pfds[c->loc].events;
      int new=old | POLLOUT;
      SpUs_logwriteuser(c,"*** flags changed %c%c -> %c%c\n",
			(old & POLLIN)?'R':'-',
			(old & POLLOUT)?'W':'-',
			(new & POLLIN)?'R':'-',
			(new & POLLOUT)?'W':'-');
    } /* sielim PATCH end */
    c->t->pfds[c->loc].events |= POLLOUT;
    return 0;
}

#ifdef jpolld_conn_kill
# undef jpolld_conn_kill
#endif

int jpolld_conn_kill_count=0;

void jpolld_conn_kill(jpolld_conn c,char* s,int l) {
  if (c) {
    next_pfd cur_npfd;
    int loc = (int)c->loc;
    jpolld_thread t = (jpolld_thread)c->t;
    debug(WARN_DL,"Connection killed from %s:%i\n",s,l);

    close(t->pfds[loc].fd);

    t->mpfd == loc ? t->mpfd-- : t->mpfd;
    cur_npfd = malloc(sizeof(_next_pfd));
    cur_npfd->pos = loc;
    cur_npfd->next = t->npfd;
    t->npfd = cur_npfd;

    t->pfds[loc].fd = -1;
    t->pfds[loc].events = 0;
    t->conns[loc] = NULL;
    if(c->reader) {
      jpolld_read_buf rb;
      jpolld_read_buf rb_cur;

      rb = c->reader;
      while(rb != NULL) {
	rb_cur = rb->next;
	xmlnode_free(rb->x);
	rb = rb_cur;
      }
    }
    if(c->writer) {
      jpolld_write_buf wb;
      jpolld_write_buf wb_cur;

      wb = c->writer;
      while(wb != NULL) {
	wb_cur = wb->next;
	pool_free(wb->p);
	packets_canceled++;
	wb = wb_cur;
      }
    }
    /* sielim PATCH begin */
    if(c->xml_need_responses_queue) {
      xml_need_response nr;
      xml_need_response nr_cur;

      nr = c->xml_need_responses_queue;
      while(nr != NULL) {
	nr_cur = nr->next;
	pool_free(nr->p);
	nr = nr_cur;
      }
    }

    if (c->ssl) {
      jab_free_conn(c->ssl);
    }

    debug(CONN_DL,"[%d] user:%s \tConn gone, mpfd: %d\n", t->id, c->user?c->user->id.user:"?", t->mpfd);
    if (c->user) {
      c->user->conn=NULL;
#undef user_conn_kill
      user_conn_kill(c->user,s,l);
    }
    jpolld_conn_kill_count++;
    /* sielim PATCH end */
    pool_free(c->p);
#ifdef POOL_DEBUG
      fprintf(stderr,"*********  What is in memory: *********\n");
      pool_stat(1);
      fprintf(stderr,
	      "\n***************************************"
	      "\n***************************************\n\n");
#endif
  }
}
