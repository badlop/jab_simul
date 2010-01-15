#ifndef __KARMA_H__
#define __KARMA_H__

#define ut(t) ((long long)((t).tv_sec)*1000000 + (t).tv_usec)

// default to disable karma 
#define KARMA_READ_MAX(k) (k*100) /* how much you are allowed to read off the sock */
#define KARMA_INIT -100   /* internal "init" value, should not be able to get here */
#define KARMA_HEARTBEAT 2000 /* miliseconds to register for heartbeat */
#define KARMA_MAX 10     /* total max karma you can have */
#define KARMA_INC 1      /* how much to increment every KARMA_HEARTBEAT seconds */
#define KARMA_DEC 0      /* how much to penalize for reading KARMA_READ_MAX in
                            KARMA_HEARTBEAT seconds */
#define KARMA_PENALTY -5 /* where you go when you hit 0 karma */
#define KARMA_RESTORE 5  /* where you go when you payed your penelty or INIT */

struct s_karma
{
    int val; /* current karma value */
    long bytes; /* total bytes read (in that time period) */
    int max;  /* max karma you can have */
    int inc,dec; /* how much to increment/decrement */
    int penalty,restore; /* what penalty (<0) or restore (>0) */
    struct timeval last_update; /* time this was last incremented */
};

void karma_inc(struct s_karma *k, struct timeval* t);     /* inteligently increments karma */
void karma_dec(struct s_karma *k, struct timeval* t);     /* inteligently decrements karma */
int karma_chk(struct s_karma *k,long bytes_read, struct timeval* t); /* checks to see if we have good karma */

#endif
