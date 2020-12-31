#include <string.h>
#include <utf.h>
#include <utf_timer.h>
#include <utf_debug.h>
#include "testlib.h"

#define PROG_PINGPING	"pingping"
#define PROG_PINGPONG	"pingpong"
#define BSIZE		(1024*1024*1024)	// max 1 GiB
#define MSGSIZE		0	// default
#define ITER		100	// default

static char	__attribute__ ((aligned(256))) rbuf[BSIZE];
static char	__attribute__ ((aligned(256))) sbuf[BSIZE];

void
pingpong(int len, int iter, uint64_t tag, uint64_t *st, uint64_t *et)
{
    UTF_reqid	s_reqid, r_reqid;
    int	i, rc;

    *st = tick_time();
    if (myrank == 0) {
	for(i = 0; i < iter; i++) {
	    utf_tmr_begin(TMR_UTF_EXT1);
	    UTF_CALL(err1, rc, utf_recv, rbuf, len, 1, tag, &r_reqid);
	    UTF_CALL(err2, rc, utf_wait, r_reqid);
	    utf_tmr_end(TMR_UTF_EXT1);

	    utf_tmr_begin(TMR_UTF_EXT2);
	    UTF_CALL(err3, rc, utf_send, sbuf, len, 1, tag, &s_reqid);
	    UTF_CALL(err2, rc, utf_wait, s_reqid);
	    utf_tmr_end(TMR_UTF_EXT2);
	}
    } else {
	for(i = 0; i < iter; i++) {
	    utf_tmr_begin(TMR_UTF_EXT2);
	    UTF_CALL(err3, rc, utf_send, sbuf, len, 0, tag, &s_reqid);
	    UTF_CALL(err2, rc, utf_wait, s_reqid);
	    utf_tmr_end(TMR_UTF_EXT2);

	    utf_tmr_begin(TMR_UTF_EXT1);
	    UTF_CALL(err1, rc, utf_recv, rbuf, len, 0, tag, &r_reqid);
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
pingping(int len, int iter, uint64_t tag, uint64_t *st, uint64_t *et)
{
    UTF_reqid	s_reqid, r_reqid;
    int	i, rc;

    *st = tick_time();
    if (myrank == 0) {
	for(i = 0; i < iter; i++) {
	    utf_tmr_begin(TMR_UTF_EXT1);
	    UTF_CALL(err1, rc, utf_recv, rbuf, len, 1, tag, &r_reqid);
	    UTF_CALL(err2, rc, utf_wait, r_reqid);
	    utf_tmr_end(TMR_UTF_EXT1);
	}
    } else {
	if (wflag) {
	    for(i = 0; i < iter; i++) {
		utf_tmr_begin(TMR_UTF_EXT2);
		UTF_CALL(err3, rc, utf_send, sbuf, len, 0, tag, &s_reqid);
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
myinit(size_t len)
{
    size_t	ll;
    uint64_t	st, et;

    memset(rbuf, len, sizeof(rbuf));
    for (ll = 0; ll < len; ll++) {
	sbuf[ll] = (char) ll;
    }
    /* dry run */
    pingpong(0, 2, 0x10, &st, &et);
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
show(char *bname, int len, uint64_t tm0)
{
    UTF_reqid	reqid;
    int	rc;

    if (myrank == 0) {
	int	i, errs = 0;
	uint64_t	tm1, tm_max, hz;

	if (vflag) {
	    for (i = 0; i < len; i++) {
		if (rbuf[i] != (char)i) {
		    errs++;
		    if (errs < 10) {
			fprintf(stderr, "rbuf[%d] is %d, but expect %d\n", i, rbuf[i], (char)i);
		    }
		}
	    }
	}
	UTF_CALL(err1, rc, utf_recv, &tm1, sizeof(uint64_t), 1, 0x20, &reqid);
	UTF_CALL(err3, rc, utf_wait, reqid);
	hz = tick_helz(0);
	tm_max = tm0 > tm1 ? tm0: tm1;
	if (vflag) {
	    printf("@%s-utf, %d,%8.3f,%8.3f, %d\n", 
		   bname, len, CLK2USEC(tm_max), (float)len/CLK2USEC(tm_max), errs); fflush(stdout);
	} else {
	    printf("@%s-utf, %d,%8.3f,%8.3f\n", 
		   bname, len, CLK2USEC(tm_max), (float)len/CLK2USEC(tm_max)); fflush(stdout);
	}
    } else {
	UTF_CALL(err2, rc, utf_send, &tm0, sizeof(uint64_t), 0, 0x20, &reqid);
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
    void	(*prog)(int, int, uint64_t, uint64_t*, uint64_t*);

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
	if (vflag) {
	    printf("%s: iteration=%d %s\n#name, length, usec, MB/sec, Erros\n", progname, iteration, optstring);
	} else {
	    printf("%s: iteration=%d %s\n#name, length, usec, MB/sec\n", progname, iteration, optstring);
	}
    }
    myinit((mlength > length) ? mlength : length);
    /**/
    if (mlength > 0) {
	size_t	l;
#if 0 /* zero byte message is excluded 20201225 */
	prog(0, iteration, 0x100, &st, &et);
	tm = (et - st)/iteration;
	show(progname, 0, tm);
#endif
	for (l = length; l <= mlength; l <<= 1) {
	    prog(l, iteration, 0x100, &st, &et);
	    tm = (et - st)/iteration;
	    show(progname, l, tm);
	}
    } else {
	prog(length, iteration, 0x100, &st, &et);
	tm = (et - st)/iteration;
	show(progname, length, tm);
    }
    /* timer report */
    mytmrfinalize(progname);
    utf_finalize(1);
    if (myrank == 0) printf("Finish\n");
    return 0;
}
