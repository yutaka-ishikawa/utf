#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include "utf_externs.h"
#include "testlib.h"
#include "utf_timer.h"
#include "utf_conf.h"
#include "utf_tofu.h"

static int mypid;
#ifdef UTF_DEBUG
#define DEBUG(mask) if (dflag&(mask))
#else
#define DEBUG(mask) if (0)
#endif
#include "utf_errmacros.h"

#define PROG_PINGPING	"pingping"
#define PROG_PINGPONG	"pingpong"

#define BSIZE		1024	// max
#define MSGSIZE		0	// default
#define ITER		100	// default


#ifdef VCQIDTAB
extern utofu_vcq_id_t	*tab_vcqid;
static inline utofu_vcq_id_t
rank2vcqid(int rank)
{
    return tab_vcqid[rank];
}
#else
static inline utofu_vcq_id_t
rank2vcqid(int rank)
{
    return utf_info.vname[rank].vcqid;
}
#endif

#ifdef REMOTE_PUT
#define PYLSIZE	(256 - sizeof(int)*2)
#else
#define PYLSIZE	(32 - sizeof(int)*2)
#endif
struct packet {
    int pktlen;
    int	datalen;
    uint8_t	data[PYLSIZE];
};
#define PKTSIZE	sizeof(struct packet)
struct packet	sndpacket;

static utofu_vcq_hdl_t	vcqh;	// static
extern char *recvbuf;		 /* receiver buffer */
extern char *sendbuf;		 /* receiver buffer */

static char	__attribute__ ((aligned(256))) rbuf[BSIZE];
static char	__attribute__ ((aligned(256))) sbuf[BSIZE];
static uint64_t	recvoff = 0;
static int myrecvoff = 0;
static int	pendcount;
#ifndef PEEK_MEMORY
static int	rcount;
#endif
#ifdef REMOTE_PUT
static int	rmtput_maxcount;
static int	rmtput_count;

static inline void
utf_tcqprogress()
{
    int	rc;
    void	*cbdata;

    rmtput_maxcount = rmtput_count > rmtput_maxcount ? rmtput_count : rmtput_maxcount;
    do {
	if ((rc = utofu_poll_tcq(vcqh, 0, &cbdata)) == UTOFU_SUCCESS) {
	    --rmtput_count;
	}
	if (rc != UTOFU_SUCCESS && rc != UTOFU_ERR_NOT_FOUND) goto err;
    } while (rmtput_count > pendcount);
    return;
err:
    {
	char msg[1024];
	utofu_get_last_error(msg);
	utf_printf("%s: error rc(%d) %s\n", __func__, rc, msg);
	abort();
    }
}
#endif

#ifdef PEEK_MEMORY
inline void pip_pause(void)
{
    asm volatile("sevl" ::: "memory");
    asm volatile("wfe" ::: "memory");
}

//static inline
void
myrecv(int peer, void *buf, size_t size)
{
    volatile struct packet *pkt = (struct packet*) (recvbuf + myrecvoff);
    while (pkt->pktlen == 0) {
#ifdef REMOTE_PUT
	if (rmtput_count) utf_tcqprogress();
#endif
    }
    myrecvoff += pkt->pktlen;
}
#else
//static inline
void
myrecv(int peer, void *buf, size_t size)
{
    int rc;
    struct utofu_mrq_notice mrq_notice;

    do {
	rc = utofu_poll_mrq(vcqh, 0, &mrq_notice);
    } while (rc != UTOFU_SUCCESS);
    switch(mrq_notice.notice_type) {
    case UTOFU_MRQ_TYPE_LCL_PUT: /* 0 */
	break;
    case UTOFU_MRQ_TYPE_RMT_PUT: /* 1 */
	if (Mflag & 0x01) {
	    myrecvoff += PKTSIZE;
	    rcount++;
	} else {
	    volatile struct packet *pkt = (struct packet*) (recvbuf + myrecvoff);
	    //utf_printf("%s: rmt_stadd(%lx)\n", __func__, mrq_notice.rmt_stadd);
	    //utf_printf("%s: pkt->pktlen = %d PKTSIZE(%ld) myrecvoff(%d)\n", __func__, pkt->pktlen, PKTSIZE, myrecvoff);
	    myrecvoff += pkt->pktlen;
	}
	break;
    case UTOFU_MRQ_TYPE_LCL_GET: /* 2 */
    case UTOFU_MRQ_TYPE_RMT_GET: /* 3 */
    case UTOFU_MRQ_TYPE_LCL_ARMW:/* 4 */
    case UTOFU_MRQ_TYPE_RMT_ARMW:/* 5 */
	break;
    default:
	utf_printf("%s: unknown mrq event(0x%x)\n", __func__, mrq_notice.notice_type);
    }
}
#endif /* PEEK_MEMORY */

//static inline
void
mysend(int dst, void *buf, size_t size)
{
    utofu_vcq_id_t rvcqid;
    utofu_stadd_t rstadd;
    uint64_t edata = 0;
    uint64_t	flgs;
#ifdef PEEK_MEMORY
    flgs = UTOFU_ONESIDED_FLAG_STRONG_ORDER;
#else
    flgs = UTOFU_ONESIDED_FLAG_REMOTE_MRQ_NOTICE
	 | UTOFU_ONESIDED_FLAG_STRONG_ORDER;
#endif
#ifdef REMOTE_PUT
    flgs |= UTOFU_ONESIDED_FLAG_TCQ_NOTICE;
#ifdef CACHE_INJECT
    flgs |= UTOFU_ONESIDED_FLAG_CACHE_INJECTION;
#endif
#endif
    rvcqid = rank2vcqid(dst);	  /* remote VCQID */
    rstadd  = recvstadd + recvoff; /* remote stadd */
#ifdef REMOTE_PUT
    if (rmtput_count) utf_tcqprogress();
    {
	struct packet	*pkt = (struct packet*) (sendbuf + recvoff);
	utofu_stadd_t	lstadd = sendstadd + recvoff;
	pkt->datalen = size;
	if (size <= PYLSIZE) {
	    memcpy(pkt->data, buf, size);
	}
	pkt->pktlen = PKTSIZE;
	UTOFU_CALL(1, utofu_put,
		   vcqh, rvcqid,  lstadd, rstadd, pkt->pktlen, edata, flgs, NULL);
	rmtput_count++;
    }
#else
    sndpacket.datalen = size;
    if (size <= PYLSIZE) {
	memcpy(sndpacket.data, buf, size);
    }
    sndpacket.pktlen = PKTSIZE;
    UTOFU_CALL(1, utofu_put_piggyback,
	       vcqh, rvcqid, &sndpacket, rstadd, sndpacket.pktlen, edata, flgs, NULL);
#endif
    recvoff += PKTSIZE;
}

void
myinit()
{
    int i;

    if (!sflag && myrank == 0) {
	if (Mflag & 01) {
	    myprintf("No access receive buffer area\n");
	} else {
	    myprintf("Access receive buffer area\n");
	}
    }
    if (pflag) {
	pendcount = 1;
    } else {
	pendcount = 10;
    }
    if (myrank == 0) {
	myprintf("pending # of TCQ: %d\n", pendcount);
    }
    recvoff = 0;
    for (i = 0; i < length; i++) {
	sbuf[i] = i + 10;
    }
    memset(rbuf, 0, BSIZE);
    // if (!sflag) myprintf("I'm rank %d\n", myrank);
    /* dry run */
    if (myrank == 0) {
	myrecv(1, rbuf, 0); mysend(1, sbuf, 0);
    } else {
	mysend(0, sbuf, 0); myrecv(0, rbuf, 0);
    }
    mytmrinit();
}

void
pingpong(uint64_t *st, uint64_t *et)
{
    int	i;
    if (!sflag && myrank == 0) {
	printf("ping pong: pktsize(%ld) length=%ld, iteration=%d\n", PKTSIZE, length, iteration);  fflush(stdout);
    }
    if (myrank == 0) {
	*st = tick_time();
	for(i = 0; i < iteration; i++) {
	    utf_tmr_begin(TMR_UTF_EXT1);
	    myrecv(1, rbuf, length);
	    utf_tmr_end(TMR_UTF_EXT1);

	    utf_tmr_begin(TMR_UTF_EXT2);
	    mysend(1, sbuf, length);
	    utf_tmr_end(TMR_UTF_EXT2);
	}
	*et = tick_time();
    } else {
	*st = tick_time();
	for(i = 0; i < iteration; i++) {
	    utf_tmr_begin(TMR_UTF_EXT2);
	    mysend(0, sbuf, length);
	    utf_tmr_end(TMR_UTF_EXT2);
	    utf_tmr_begin(TMR_UTF_EXT1);
	    myrecv(0, rbuf, length);
	    utf_tmr_end(TMR_UTF_EXT1);
	}
	*et = tick_time();
    }
}

void
pingping(uint64_t *st, uint64_t *et)
{
    int	i;
    if (!sflag && myrank == 0) {
	printf("ping ping: pktsize(%ld) length=%ld, iteration=%d\n", PKTSIZE, length, iteration);  fflush(stdout);
    }
    if (myrank == 0) {
	*st = tick_time();
	for(i = 0; i < iteration; i++) {
	    utf_tmr_begin(TMR_UTF_EXT1);
	    myrecv(1, rbuf, length);
	    utf_tmr_end(TMR_UTF_EXT1);
	}
	*et = tick_time();
    } else {
	*st = tick_time();
	for(i = 0; i < iteration; i++) {
	    utf_tmr_begin(TMR_UTF_EXT1);
	    mysend(0, sbuf, length);
	    utf_tmr_end(TMR_UTF_EXT1);
	}
	*et = tick_time();
    }
}


int
main(int argc, char **argv)
{
    uint64_t	st, et, hz, tm;
    char	*progname;
    void	(*prog)();

    mypid = getpid();
    iteration = ITER;	// default
    length = MSGSIZE;	// default
    if (argc < 2) {
	fprintf(stderr, "%s pingpong | pingping\n", argv[0]);
	exit(-1);
    }
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
    // setenv("UTF_NO_SHOW_OPTION", "1", 1);
    test_init(argc, argv);
    utf_vcqh(&vcqh);
    hz = tick_helz(0);
    /* initialize */
    myinit();
    /**/
    prog(&st, &et);
    tm = (et - st)/iteration;
    myprintf("%f\n", ((double)(tm) / ((double)hz/(double)1000000)));

#ifdef REMOTE_PUT
    /* clean up */
    while (rmtput_count > 0) {
	utf_tcqprogress();
    }
    if (myrank == 0) {
	myprintf("max pending TCQ : %x\n", rmtput_maxcount);
	myprintf("rmtput_count    : %d\n", rmtput_count);
    }
#endif
    {
	struct utofu_mrq_notice mrq_notice;
	int	rc;
	rc = utofu_poll_mrq(vcqh, 0, &mrq_notice);
	if (rc == UTOFU_ERR_NOT_FOUND) {
	    myprintf("MRQ is OK\n");
	} else {
	    myprintf("rc = %d\n", rc);
	}
    }
    mytmrfinalize(progname);
    utf_finalize();
    return 0;
}
