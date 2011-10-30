#ifndef __JPOLLD_H__
#define __JPOLLD_H__

#include "config.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <syslog.h> /* sielim PATCH */
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/un.h>

#include <sys/select.h>
#include <sys/poll.h>
#include <strings.h>
#include <sys/ioctl.h>


#include <lib/lib.h>                   /* sielim PATCH */
#include "libjabber_ext.h"             /* sielim PATCH */

#include <pthread.h>

#define TO_SERV ""
#define FR_SERV ""
//#define TO_SERV "to"
//#define FR_SERV "fr"

#define ut(t) ((long long)((t).tv_sec)*1000000 + (long long)(t).tv_usec)

#include "wp_defines.h"                /* sielim PATCH */
#if USE_NCURSES
# include <ncurses.h>
#endif
#include "jab_simul-admin.h"

typedef char* CString;
typedef unsigned short BOOL;

typedef struct slist_struct {
  void* arg;
  struct slist_struct* next;
} _slist,*slist;

#if USE_NCURSES
extern WINDOW* screen;
extern WINDOW* msgwin;
#endif

#define MAX_PFDS 8000
#define MAX_USERS 50000
#define MAX_THREADS 10

#ifdef MEM_DEBUG
#ifndef SAFE

#define malloc(x) mem_debug_malloc(ZONE, x)
#define pool_new() mem_debug_pool_new(ZONE)
#define pool_heap(x) mem_debug_pool_heap(ZONE, x)
#define _pool_heap(x, y)  mem_debug_pool_heap(ZONE, x, y)

#define free(x) mem_debug_free(x)
#define pool_free(x) mem_debug_pool_free(x)

#endif /* SAFE */
#endif /* MEM_DEBUG */

void exit_app(int);

extern struct status_str {
  char* Status;
  char* Show;
} statusy[];

typedef enum { state_UNKNOWN, state_AUTHD, state_CLOSING } conn_state;
typedef enum { sock_ERROR, sock_NOT_CONNECTED, sock_CONN_IN_PROGRESS, sock_CONNECTED } sock_state;

typedef struct jpolld_conn_struct *jpolld_conn, _jpolld_conn;
typedef struct jpolld_thread_struct *jpolld_thread, _jpolld_thread;

/* struktura do przechowywania informacji o wyslanych xml'ach, ktore wymagaja odeslania */
typedef struct xml_need_response_struct *xml_need_response, _xml_need_response;
/* struktura do przechowywania danych usera (wysokopoziomowych) */
typedef struct user_properities_struct *user_properities, _user_properities;
/* typ funkcji - handlera do obslugi przychodzacych od serwera odpowiedzi */
typedef void (*t_xml_response_handler)(xmlnode,user_properities,struct timeval*,struct timeval*);

/* struktura do przechowywania danych repeatera */
typedef struct struct_event_repeater *event_repeater,_event_repeater;
/* typ funkcji - handlera do obslugi eventow czasowych wywolywanych przez repeatery*/
typedef void (*t_event_handler)(event_repeater,user_properities,struct timeval*);

struct struct_event_repeater {
  long long activation_tm;       // in us  -  kiedy sie aktywuje w najblizszym czasie
  long long frequency;           // in us  -  jak czesto sie aktywuje, jak <0 to sam sie nie aktywuje
  t_event_handler event_handler; // procedura obslugi
  int counter;                   // ile max razy sie odpali (-1 = b.o )
#define UP_EVER -1
  int user_state;                // stan, w jakim musi sie znajdowac user, zeby sie aktywowal, UP_EVER - dowolny
  struct struct_event_repeater* next; // nastepny w liscie
  void* arg;
};
event_repeater create_repeater_from_xml(xmlnode xml, struct timeval*);

typedef struct jpolld_write_buf_struct
{
    pool p;              /* pool for the write buffer */
    xmlnode xbuffer;     /* current xmlnode */
    char *wbuffer;       /* current write buffer */
    char *cbuffer;       /* position in write buffer */
    struct jpolld_write_buf_struct *next;
} *jpolld_write_buf, _jpolld_write_buf;

typedef struct jpolld_read_buf_struct
{
    xmlnode x;
    struct jpolld_read_buf_struct *next;
} *jpolld_read_buf, _jpolld_read_buf;

typedef struct next_pfd_struct
{
    int pos;
    struct next_pfd_struct *next;
} *next_pfd, _next_pfd;

typedef
struct user_name_struct {
  char user[50];
  char server[50];
  int enabled:1;
  int can_receive_message:1;
  int can_be_added_to_roster:1;
} _user_name;

typedef struct jpolld_instance_struct
{
  //int listenid;
  //int listenfd;
  //int lport;
  //char *lip;
  //char *mhost;
  //char *hostname;
  //char *secret;
    _user_name user_names_space[MAX_USERS];
    user_properities users[MAX_USERS];
    pthread_mutex_t mutex;
} *jpolld_instance, _jpolld_instance;

struct jpolld_conn_struct
{
    int loc;
    pool p;
    xstream xs;
    conn_state state;
    sock_state sstate;  /* sielim PATCH */

    jpolld_write_buf writer;
    jpolld_read_buf reader;
    jpolld_thread t;
    /*struct jpolld_conn_struct *next;*/
  struct timeval creation_time;            /* sielim PATCH */
  unsigned int admin_flags;                /* sielim PATCH */
  user_properities user;                   /* sielim PATCH */
  xml_need_response xml_need_responses_queue;  /* sielim PATCH */
  void *ssl;
};

/* sielim PATCH begin */
/* struktura do przechowywania informacji o wyslanych xml'ach, ktore wymagaja odeslania */
struct xml_need_response_struct {
  pool p;
  char* id;                                // Identyfikator wyslanej wiadomosci
  struct timeval send_time;                // Kiedy zostala wyslana
  t_xml_response_handler callback;         // Handler - co robic, jak przyjdzie odpowiedz
  struct xml_need_response_struct* next;   // nastepny w liscie
};

/* struktura do przechowywania danych usera (wysokopoziomowych) */
struct user_properities_struct {
  struct jid_struct id;
  char* passwd;
  char* host;
  char* sid;
  int ssl;
  int connection_timeout;                    // Maksymalny czas oczekiwania na polaczenie [ms]
  int login_timeout;                         // Maksymalny czas oczekiwania na potwierdzenie autoryzacji [ms]
#define DEFAULT_PORT 5222
  int port;
  jpolld_conn conn;

#define UP_DISABLED 0
#define UP_CAN_LOGIN 1
#define UP_IS_LOGGING 2
#define UP_LOGGED_IN 3
  int state;
  event_repeater events_queue;               // Kolejka eventow czasowych
  long long sooner_activation_tm;            // Czas pojawienia sie najblizszego przyszlego eventu czasowego

  //Miejsce na rozne parametry:
  // - flagi
  // - xml'e konfiguracyjne ?
  // - ... inne
  char* sniff_dir;                           //Sciezka do plikow z logami socketow
#define UPF_EXIT_AFTER_LOGIN 0x01            //Specjalny user do testowania czasu logowania
#define UPF_SNIFF 0x02                       //Dokladny podglad co sie dzieje u usera (logi)
#define UPF_EXPECT_KILL 0x04                 //Oczekujemy, ze polaczenie zostanie za chwile zamkniete
  int flags;
  unsigned short digest_auth:1;
  unsigned short stream_opened:1;            // Czy po polaczeniu juz otworzyl strumien i moze wysylac autoryzacje.
  float msg_resp_propab;                     // Prawdopodobienstwo odpowiedzi na wiadomosc
  //statystyka wiadomosci
  long long received_msgs_time_sum;
  int received_msgs_count;
  //statystyka presence'ow
  int received_presences_count;
  int sent_presences_count;

  int last_roster_count;
  int roster_count;
  xht roster_hash;
  struct {
    struct priv_roster_item_struct* priv_rost_list;
    int numextcontacts;
    pool grpp;
    struct group_item_struct* Groups; //Lista wszystkich grup
    struct group_item_struct* gall; //Grupa glowna
    unsigned short loaded;
  } priv_rost;

  int tmpid;
  char* Status;
  char* Show;
  struct user_properities_struct* next;
};

extern user_properities alive_users_list;

// admin_flags:
#define FL_SPUS 0x0001              // special user flag
/* sielim PATCH end */

struct jpolld_thread_struct
{
    int id;
    jpolld_instance i;
    pool p;
    pthread_mutex_t mutex;
    pthread_cond_t done;

    /* This is the masterfd deal */
    int masterfd;
    xstream masterxs;
    char *host;
    char *sid;
    conn_state mstate;

    /* This is our real conns */
    int mpfd;
    next_pfd npfd;
    struct pollfd pfds[MAX_PFDS+1];
    jpolld_conn conns[MAX_PFDS+1];

    /* This is added by sielim*/                         /*sielim PATCH*/
#if SOCKS_STAT                                           /*sielim PATCH*/
  /* statystyki socketow */                              /*sielim PATCH*/
  long events_no[MAX_PFDS+1];                            /*sielim PATCH*/
  long wr_bytes[MAX_PFDS+1];                             /*sielim PATCH*/
  long rd_bytes[MAX_PFDS+1];                             /*sielim PATCH*/
  struct timeval open_times[MAX_PFDS+1];                 /*sielim PATCH*/
#endif                                                   /*sielim PATCH*/
};

extern jpolld_thread main_thread;

/* statystyka wiadomosci stuff */
void init_stat(int no,int size);
void add_stat(int no,long long time);
long long get_div_stat(int no);
long long get_stat(int no);

#define MESG_STAT 0
#define DIFF_STAT 1
#define RADD_STAT 2
#define RDEL_STAT 3

#define init_mesg_stat(size)   init_stat(MESG_STAT,size)
#define add_mesg_stat(time)    add_stat(MESG_STAT,time)
#define get_div_mesg_stat()    get_div_stat(MESG_STAT)
#define init_diff_stat(size)   init_stat(DIFF_STAT,size)
#define add_diff_stat(time)    add_stat(DIFF_STAT,time)
#define get_diff_stat()        get_stat(DIFF_STAT)

/* conn socket *stuff */
int make_clientfd(char *host, int port, sock_state* conn_state);
int accept_connected(int fd,sock_state *psstate);

/* conn stuff */
void jpolld_conn_node(int type, xmlnode x, void *arg);
void add_conns(jpolld_thread t);
jpolld_conn jpolld_conn_create(jpolld_thread,int,user_properities,sock_state,struct timeval*);  /* sielim PATCH */
void jpolld_conn_kill(jpolld_conn c,char*,int);
#define jpolld_conn_kill(user) jpolld_conn_kill(user,__FILE__,__LINE__)
int jpolld_conn_write_str(jpolld_conn c, const char *buf, int force);
int jpolld_conn_write(jpolld_conn c, xmlnode x, int force);
void _jpolld_conn_write(jpolld_conn c);


/* obsluga listy oczekiwan na odpowiedzi */
// Tworzymy i dodajemy do kolejki
xml_need_response xml_need_response_node_create
(char*,t_xml_response_handler,struct timeval*,xml_need_response);
// Szukamy i usuwamy z kolejki
xml_need_response xml_need_response_node_get(char*,xml_need_response*);
// Zwalniamy
#define xml_need_response_node_delete(x) pool_free(x->p);

/* obsluga listy userow */
// Tworzymy strukture
user_properities user_create(_user_name* uns,
			     char* passwd,char* resource,char* fullname,
			     char* host,int port,
			     event_repeater ev_list, int ssl);
//Usuwamy strukture
void user_delete(user_properities);
//Glowna procedura aktywnosci
void user_do_your_job(user_properities user, struct timeval* curtime);
int user_connect(user_properities);

void user_finish_conn(user_properities user,char* why);
int user_conn_kill(user_properities user,char*,int);
#define user_conn_kill(user) user_conn_kill(user,__FILE__,__LINE__)
int user_do_login_plain(user_properities, struct timeval*);
int user_do_login_digest(user_properities, struct timeval*);
int user_do_fetch_properities(user_properities, struct timeval*);
int user_do_fetch_extcontacts(user_properities, struct timeval*);
int user_do_presence(user_properities user, struct timeval* curtime, char* type, char* jid_to, char* status, char* show);
int user_do_adddel_roster_item(user_properities, struct timeval*, char*, char*, int);
int user_do_send_message(user_properities, struct timeval*,char*, char*);
int user_do_send_raw_bytes(user_properities, struct timeval*,char*);

#define xml_need_response_node_delete(x) pool_free(x->p);

void copy_repeaters(pool p, event_repeater* plist_to, event_repeater listfrom);
/* Glowna procedura obslugi eventow czasowych */
void user_handle_repeater_queue(user_properities,struct timeval*);
/* Glowna procedura obslugi xmli przychodzacych od serwera do usera */
void handle_user_node(xmlnode x, user_properities user, struct timeval*);
/* handlery do obslugi odpowiedzi na rozne zadania */
void handle_login_response(xmlnode x, user_properities user,struct timeval*,struct timeval*);
void handle_roster_response(xmlnode x, user_properities user,struct timeval*,struct timeval*);
void handle_presence_response(xmlnode x, user_properities user,struct timeval*,struct timeval*);

/* dpms patch begin */
typedef struct config_struct *config, _config;

struct config_struct
{
    pool p;
    int maxusers;
    int masterfd;
    char *host;
    char *sid;
    int listenfd;
    int lport;
    char *lip;
  //char *mhost;
  //int mport;
    char *hostname;
  //char *secret;
    int rate_time;
    int max_points;
  /* sielim PATCH begin */
    char fullxmlscannedloguser[50];
    char fullxmlmasterlogpath[100];
    char fullxmluserlogpath[100];
    int log_route_errors;
    int log_full_master_socket;
    struct {
      int count;                /* 0 - wylaczona, >0 - liczba skanowanych logowanych socketow */
      int write_log_interval;   /* ms - co ile milisekund ma wpisywac statystyke do logow */
      int refresh_interval;     /* ms - co ile milisekund ma czyscic statystyke - nie czysci mastersocketa */
    } log_stat_socket;
  /* sielim PATCH end */
};
extern struct config_struct main_config;

/* main */
int configure(jpolld_instance,config cfg, char *cfgfile);


void jab_ssl_stop();
int jab_ssl_init();
ssize_t jab_ssl_read(void *ssl, void *buf, size_t count, int *blocked);
ssize_t jab_ssl_write(void *ssl, const void *buf, size_t count, int *blocked);
void *jab_ssl_conn(int fd);
void jab_free_conn(void *ssl);
int jab_ssl_connect(void *ssl);

#endif
