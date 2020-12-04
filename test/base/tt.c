#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include "../include/utf.h"
int	nnp, nprocs, myrank;
pid_t	mypid;
int	vflag, dflag, mflag, sflag, Mflag, pflag;
int	iteration;
size_t	length;
#define TMR_UTF_SEND_POST	0
#define TMR_UTF_RECV_POST	1
#define TMR_UTF_SENDENGINE	2
#define TMR_UTF_SNDPROGRESS	3
#define TMR_UTF_RCVPROGRESS	4
#define TMR_UTF_TCQ_CHECK	5
#define TMR_TOFU_PIGSEND	6
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
#define TMR_EVT_MAX		19
#define TMR_COUNT_MAX		1000
#define TMR_SYM_LEN		20

uint64_t	tmr_max[TMR_EVT_MAX];
uint64_t	tmr_min[TMR_EVT_MAX];
uint64_t	tmr_begin[TMR_EVT_MAX];
uint32_t	tmr_cntmax[TMR_EVT_MAX];
uint32_t	tmr_count[TMR_EVT_MAX];
uint64_t	tmr_tm[TMR_EVT_MAX][TMR_COUNT_MAX];
uint64_t	tmr_hz;
int		tmr_sflag;

int
myprintf(const char *fmt, ...)
{
    va_list	ap;
    int		rc;
    printf("[%d] ", myrank); fprintf(stderr, "[%d] ", myrank);
    va_start(ap, fmt);
    rc = vprintf(fmt, ap);

    va_start(ap, fmt);
    rc = vfprintf(stderr, fmt, ap);
    va_end(ap);

    fflush(stdout); fflush(stderr);
    return rc;
}

int
utf_init(int argc, char **argv, int *rank, int *nprocs, int *ppn)
{
    printf("%s\n", __func__);
}
int
utf_send(int dst, size_t size, uint64_t tag, void *buf,  UTF_reqid *reqid)
{
    printf("%s\n", __func__);
}

int
utf_recv(int src, size_t size, int tag, void *buf, UTF_reqid *reqid)
{
    printf("%s\n", __func__);
}
int
utf_wait(UTF_reqid id)
{
    printf("%s\n", __func__);
}
int
utf_test(UTF_reqid id)
{
    printf("%s\n", __func__);
}

void
utf_finalize()
{
}

void
test_init(int argc, char **argv)
{
    printf("%s\n", __func__);
}

void
mytmrinit()
{
    printf("%s\n", __func__);
}

void
mytmrfinalize(char *p)
{
    printf("%s\n", __func__);
}
