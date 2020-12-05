#include <string.h>
#include <utf.h>
#include <utf_timer.h>
#include <utf_debug.h>
#include "testlib.h"

#define PROG_PINGPING	"pingping"
#define PROG_PINGPONG	"pingpong"
#define BSIZE		(16*1024*1024)	// max
#define MSGSIZE		0	// default
#define ITER		100	// default

static char	__attribute__ ((aligned(256))) rbuf[BSIZE];
static char	__attribute__ ((aligned(256))) sbuf[BSIZE];

void
pingpong(int len, int iter, uint64_t *st, uint64_t *et)
{
    UTF_reqid	s_reqid, r_reqid;
    int	i, rc;

    *st = tick_time();
    if (myrank == 0) {
	for(i = 0; i < iter; i++) {
	    utf_tmr_begin(TMR_UTF_EXT1);
	    UTF_CALL(err1, rc, utf_recv, rbuf, len, 1, 0, &r_reqid);
	    UTF_CALL(err2, rc, utf_wait, r_reqid);
	    utf_tmr_end(TMR_UTF_EXT1);

	    utf_tmr_begin(TMR_UTF_EXT2);
	    UTF_CALL(err3, rc, utf_send, sbuf, len, 1, 0, &s_reqid);
	    UTF_CALL(err2, rc, utf_wait, s_reqid);
	    utf_tmr_end(TMR_UTF_EXT2);
	}
    } else {
	for(i = 0; i < iter; i++) {
	    utf_tmr_begin(TMR_UTF_EXT2);
	    UTF_CALL(err3, rc, utf_send, sbuf, len, 0, 0, &s_reqid);
	    UTF_CALL(err2, rc, utf_wait, s_reqid);
	    utf_tmr_end(TMR_UTF_EXT2);

	    utf_tmr_begin(TMR_UTF_EXT1);
	    UTF_CALL(err1, rc, utf_recv, rbuf, len, 0, 0, &r_reqid);
	    UTF_CALL(err2, rc, utf_wait, r_reqid);
	    utf_tmr_end(TMR_UTF_EXT1);
	}
    }
    *et = tick_time();
    return;
err1:
    myprintf("utf_recv error: %d (i=%d)\n", rc, i); exit(-1);
err2:
    myprintf("utf_wait error: %d (i=%d)\n", rc, i); exit(-1);
err3:
    myprintf("utf_send error: %d (i=%d)\n", rc, i); exit(-1);
}

void
pingping(int len, int iter, uint64_t *st, uint64_t *et)
{
    UTF_reqid	s_reqid, r_reqid;
    int	i, rc;

    *st = tick_time();
    if (myrank == 0) {
	for(i = 0; i < iter; i++) {
	    utf_tmr_begin(TMR_UTF_EXT1);
	    UTF_CALL(err1, rc, utf_recv, rbuf, len, 1, 0, &r_reqid);
	    UTF_CALL(err2, rc, utf_wait, r_reqid);
	    utf_tmr_end(TMR_UTF_EXT1);
	}
    } else {
	if (wflag) {
	    for(i = 0; i < iter; i++) {
		utf_tmr_begin(TMR_UTF_EXT2);
		UTF_CALL(err3, rc, utf_send, sbuf, len, 0, 0, &s_reqid);
		UTF_CALL(err2, rc, utf_waitcmpl, s_reqid);
		utf_tmr_end(TMR_UTF_EXT2);
	    }
	} else {
	    for(i = 0; i < iter; i++) {
		utf_tmr_begin(TMR_UTF_EXT2);
		UTF_CALL(err3, rc, utf_send, sbuf, len, 0, 0, &s_reqid);
		UTF_CALL(err2, rc, utf_wait, s_reqid);
		utf_tmr_end(TMR_UTF_EXT2);
	    }
	}
    }
    *et = tick_time();
    return;
err1:
    myprintf("utf_recv error: %d (i=%d)\n", rc, i); exit(-1);
err2:
    myprintf("utf_wait error: %d (i=%d)\n", rc, i); exit(-1);
err3:
    myprintf("utf_send error: %d (i=%d)\n", rc, i); exit(-1);
}


void
myinit()
{
    uint64_t	st, et;

    memset(rbuf, 0, sizeof(rbuf));
    memset(sbuf, 0xac, sizeof(sbuf));
    /* dry run */
    pingpong(0, 2, &st, &et);
    /* timer init */
    {
	char	*cp = getenv("UTF_TIMER");
	if (cp && atoi(cp) == 1) {
	    mytmrinit();
	}
    }
}

#define CLK2USEC(tm)	((double)(tm) / ((double)hz/(double)1000000))
void
show(int len, uint64_t tm0)
{
    UTF_reqid	reqid;
    int	rc;

    if (myrank == 0) {
	uint64_t	tm1, hz;
	UTF_CALL(err1, rc, utf_recv, &tm1, sizeof(uint64_t), 1, 0, &reqid);
	UTF_CALL(err3, rc, utf_wait, reqid);
	hz = tick_helz(0);
	printf("%d,%8.3f,%8.3f,%8.3f,%8.3f\n", len, CLK2USEC(tm0), CLK2USEC(tm1),
		 (float)len/CLK2USEC(tm0), (float)len/CLK2USEC(tm0));
    } else {
	UTF_CALL(err2, rc, utf_send, &tm0, sizeof(uint64_t), 0, 0, &reqid);
	UTF_CALL(err3, rc, utf_wait, reqid);
    }
    return;
err1:
    myprintf("%s: utf_recv error: %d\n", __func__, rc); exit(-1);
err2:
    myprintf("%s: utf_send error: %d\n", __func__, rc); exit(-1);
err3:
    myprintf("%s: utf_wait error: %d\n", __func__, rc); exit(-1);
}

int
main(int argc, char **argv)
{
    uint64_t	st, et, tm;
    char	*progname, *optstring;
    void	(*prog)(int, int, uint64_t*, uint64_t*);

    iteration = ITER;	// default
    length = MSGSIZE;	// default
    if (!strcasecmp(argv[1], PROG_PINGPING)) {
	progname = PROG_PINGPING;
	prog = pingping;
    } else if (!strcasecmp(argv[1], PROG_PINGPONG)) {
	progname = PROG_PINGPONG;
	prog = pingpong;
    } else {
	fprintf(stderr, "must specify pingping or pingpong\n\targument is %s\n", argv[1]);
	exit(-1);
    }
    test_init(argc, argv);
    if (length > BSIZE || mlength > BSIZE) {
	fprintf(stderr, "length must be no more than %d Byte\n", BSIZE);
	exit(-1);
    }
    if (wflag) {
	optstring = ", waitcmpl is used";
    } else {
	optstring = "";
    }
    if (!sflag && myrank == 0) {
	printf("%s: length(%d) mlength(%d)\n", progname, length, mlength);
	printf("%s: iteration=%d %s\n#length, usec, usec, MB/sec\n", progname, iteration, optstring);
    }
    myinit();
    /**/
    if (mlength > 0) {
	int	l;
	prog(0, iteration, &st, &et);
	tm = (et - st)/iteration;
	show(0, tm);
	for (l = 1; l <= mlength; l <<= 1) {
	    prog(l, iteration, &st, &et);
	    tm = (et - st)/iteration;
	    show(l, tm);
	}
    } else {
	prog(length, iteration, &st, &et);
	tm = (et - st)/iteration;
	show(length, tm);
    }
    /* timer report */
    mytmrfinalize(progname);
    utf_finalize(1);
    if (myrank == 0) printf("Finish\n");
    return 0;
}
