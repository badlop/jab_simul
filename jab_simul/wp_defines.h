#ifndef __WP_DEFINES_H__
#define __WP_DEFINES_H__

#include "config.h"

#define WORK_AFTER_LOGGING

#define PINGS_ENABLED 1
#define FORK_AS_DAEMON 0
#define SOCKS_STAT 1
#define KARMA_ENABLED 1
#define KRECIOL 0
#define WRITE_SOCKS_TO_LOG 1
#define ENABLE_IP_SENDING 1

/*        **********************************************************       */

//#define DEBUG_LEVEL DET_DL
//#define DEBUG_LEVEL INFO_DL
//#define DEBUG_LEVEL CONN_DL
//#define DEBUG_LEVEL STATUS_DL
#define DEBUG_LEVEL ERR_DL
#define ERR_DL 0
#define MONIT_DL 1
#define STATUS_DL 2
#define WARN_DL 3
#define CONN_DL 4
#define ROST_DL 3
#define PROST_DL 3
#define INFO_DL 5
#define DET_DL 10
extern int deb4357xtyf;
#if USE_NCURSES
# define debug(lev, fmt, x...) (lev>DEBUG_LEVEL?deb4357xtyf=lev:(wprintw(msgwin,fmt, ## x),log_debug(0, fmt, ## x)))
#else
# define debug(lev, fmt, x...) (lev>DEBUG_LEVEL?deb4357xtyf=lev:fprintf(lev==0?stderr:stdout, fmt, ## x))
#endif

//#define debug(lev, fmt, x...)
#define user_debug(user, lev, x... ) debug(((user)->flags&UPF_SNIFF?WARN_DL:lev),## x)

/*        **********************************************************       */

extern int log_debug(int line,const char* msgfmt, ...);

#define LOOP_TIMO 5                   //[ms]
#define TIMEOUTS_TIMER_NO 0
#define TIMEOUTS_CHECK_INTERVAL 303   //[ms]

#define USERS_CHECK_EVENTS_INTERVAL (20) //[ms] - 20 ms
#define INFO_STAT_INTERVAL (300)         //[ms] - 0,3 s
#define LOGINFO_STAT_INTERVAL (60000)    //[ms] - 1 min

//#define MAX_SCANNED_SOCKS 20000
//#define MAX_LOGGED_SOCKS 30
//#define SOCKS_STAT_WRITE_LOG_TIMER_NO 1
#define SOCKS_STAT_WRITE_LOG_INTERVAL 15003         //[ms] - 15 s
//#define SOCKS_STAT_REFRESH_TIMER_NO 2
#define SOCKS_STAT_REFRESH_INTERVAL (60000*15+13) //[ms] - 15 min

#define KARMA_TIMER_NO 3
#define KARMA_CHECK_INTERVAL    211          //[ms] - 211 ms

#if PINGS_ENABLED
# if WRITE_SOCKS_TO_LOG
#  define LOG_PATH "/tmp/"
# endif
# define PING_ACTIVATION_TIMO 85000   //[ms]
# define CLOSE_INACTIVE_TIMO 120000   //[ms]  musi byc > PING_ACTIVATION_TIMO
#endif


#endif
