#include <stdio.h>
#include "utf_tsc.h"

#define TIMER_MAX(a, b)	((a) > (b) ? (a) : (b))
#define TIMER_MIN(a, b)	((a) < (b) ? (a) : (b))
#define TMR_MINVAL	((uint64_t)-1LL - 1)

#define TMR_UTF_SEND_POST	0
#define TMR_UTF_RECV_POST	1
#define TMR_UTF_SENDENGINE	2
#define TMR_UTF_SNDPROGRESS	3
#define TMR_UTF_RCVPROGRESS	4
#define TMR_UTF_TCQ_CHECK	5
#define TMR_UTF_WAIT		6
#define TMR_TOFU_PUT		7
#define TMR_TOFU_GET		8
#define TMR_UTF_MEMREG		9
#define TMR_UTF_MEMDEREG	10
#define TMR_UTF_MALLOC		11
#define TMR_UTF_MFREE		12
#define TMR_UTF_SEND_POST2	13
#define TMR_UTF_RECV_LATENCY	14
#define TMR_UTF_RECV_LATENCY2	15
#define TMR_UTF_SEND_LATENCY	16
#define TMR_UTF_EXT1		17
#define TMR_UTF_EXT2		18
#define TMR_FI_TSEND		19
#define TMR_FI_SEND		20
#define TMR_FI_TRECV		21
#define TMR_FI_RECV		22
#define TMR_FI_READ		23
#define TMR_FI_WRITE		24
#define TMR_FI_CQREAD		25
#define TMR_EVT_MAX		26
#define TMR_COUNT_MAX		1000
#define TMR_SYM_LEN		20

extern uint64_t	tmr_max[TMR_EVT_MAX];
extern uint64_t	tmr_min[TMR_EVT_MAX];
extern uint64_t	tmr_begin[TMR_EVT_MAX];
extern uint32_t	tmr_cntmax[TMR_EVT_MAX];
extern uint32_t	tmr_count[TMR_EVT_MAX];
extern uint64_t	tmr_tm[TMR_EVT_MAX][TMR_COUNT_MAX];
extern uint64_t	tmr_hz;
extern int	tmr_sflag;

static inline void
utf_tmr_begin(int evt)
{
    if (!tmr_sflag) return;
    tmr_begin[evt] = tick_time();
}

static inline void
utf_tmr_end(int evt)
{
    uint64_t	tm;

    if (!tmr_sflag) return;
    tm = tick_time() - tmr_begin[evt];
    if (tmr_count[evt] < TMR_COUNT_MAX) {
	tmr_tm[evt][tmr_count[evt]] = tm;
    }
    tmr_max[evt] = TIMER_MAX(tmr_max[evt], tm);
    tmr_min[evt] = TIMER_MIN(tmr_min[evt], tm);
    if (tmr_max[evt] == tm) {
	tmr_cntmax[evt] = tmr_count[evt];
    }
    tmr_count[evt]++;
}

extern void	utf_tmr_init();
extern void	utf_tmr_show(FILE *);
extern void	utf_tmr_start();
extern void	utf_tmr_suspend();
extern void	utf_tmr_resume();

struct utf_timer {
    char	sym[TMR_SYM_LEN];
    int		ncnt;
    double	tm[TMR_COUNT_MAX];
};

extern int	utf_tmr_stop(int nent, struct utf_timer *ut_tim, double *tmax, double *tmin, uint64_t *counter);
