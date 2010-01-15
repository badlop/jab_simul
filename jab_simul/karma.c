#include <sys/time.h>
#include "karma.h"


void karma_inc(struct s_karma *k,struct timeval* cur_time)
{
    int punishment_over = 0;
    
    /* only increment every KARMA_HEARTBEAT seconds */
    if( ( ut(k->last_update) + (long long)KARMA_HEARTBEAT*1000
	  > ut(*cur_time) ) && k->last_update.tv_sec != 0)
        return;

    /* if incrementing will raise over 0 */
    if( ( k->val < 0 ) && ( k->val + k->inc > 0 ) )
        punishment_over = 1;

    /* increment the karma value */
    k->val += k->inc;
    if( k->val > k->max ) k->val = k->max; /* can only be so good */

    /* lower our byte count, if we have good karma */
    if( k->val > 0 ) k->bytes -= ( KARMA_READ_MAX(k->val) );
    if( k->bytes <0 ) k->bytes = 0;

    /* our karma has *raised* to 0 */
    if( punishment_over )
    {
        k->val = k->restore;
        /* XXX call back for no more punishment */
    }

    /* reset out counter */
    k->last_update = *cur_time;
}

void karma_dec(struct s_karma *k,struct timeval* curtime)
{
    /* lower our karma */
    k->val -= k->dec;

    /* if below zero, set to penalty */
    if( k->val <= 0 ) 
        k->val = k->penalty;
}

/* returns 0 on okay check, 1 on bad check */
int karma_chk(struct s_karma *k,long bytes_read,struct timeval* curtime)
{
    /* first, check for need to update */
    if( ( ut(k->last_update) + (long long)KARMA_HEARTBEAT*1000 
	  < ut(*curtime) ) || k->last_update.tv_sec == 0)
        karma_inc( k, curtime );

    /* next, add up the total bytes */
    k->bytes += bytes_read;
    if( k->bytes > KARMA_READ_MAX(k->val) )
        karma_dec( k, curtime );

    /* check if it's okay */
    if( k->val <= 0 )
        return 1;

    /* everything is okay */
    return 0;
}
