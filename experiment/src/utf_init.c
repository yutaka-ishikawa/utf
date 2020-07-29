#include <getopt.h>
#include <pmix.h>
#include <pmix_fjext.h>
#include <utofu.h>
#include <jtofu.h>
#include "utf_conf.h"
#include "utf_externs.h"
#include "utf_errmacros.h"
#include "utf_tofu.h"

extern void utf_barrier();
extern void utf_PMIx_Finalize();

char	*recvbuf;	/* receiver buffer */
char	*sendbuf;	/* sender buffer */
utofu_stadd_t	recvstadd;	/* stadd of eager receiver buffer */
utofu_stadd_t	sendstadd;	/* stadd of eager receiver buffer */
int	dflag, vflag, mflag;
int	mypid, nprocs, myrank;
utofu_vcq_hdl_t	vcqh;
size_t	max_mtu, pig_size;

int	myinitialized = 0;
static utofu_tni_id_t	tniid;
static utofu_vcq_id_t	vcqid;

/* for profile */
#include "utf_timer.h"
uint64_t	tmr_max[TMR_EVT_MAX];
uint64_t	tmr_min[TMR_EVT_MAX];
uint64_t	tmr_begin[TMR_EVT_MAX];
uint32_t	tmr_cntmax[TMR_EVT_MAX];
uint32_t	tmr_count[TMR_EVT_MAX];
uint64_t	tmr_tm[TMR_EVT_MAX][TMR_COUNT_MAX];
uint64_t	tmr_hz;
int		tmr_sflag;

static char *
sym_tmr[] = { "UTF_SEND_POST", "UTF_RECV_POST", "UTF_SENDENGINE",
	      "UTF_SNDPROGRESS", "UTF_RCVPROGRESS", "UTF_TCQ_CHECK",
	      "TOFU_PIGSEND", "TOFU_PUT", "TOFU_GET",
	      "UTF_MEMREG", "UTF_MEMDEREG", "UTF_MALLOC", "UTF_MFREE",
	      "UTF_SEND_POST2", "UTF_RECV_LATENCY", "UTF_RECV_LATENCY2",
	      "UTF_SEND_LATENCY", "UTF_EXT1",  "UTF_EXT2", 0};

#define CLK2USEC(a)	((double)(a) / ((double)tmr_hz/(double)1000000))

int
utf_printf(const char *fmt, ...)
{
    va_list	ap;
    int		rc;

    fprintf(stderr, "[%d] ", utf_info.myrank);
    va_start(ap, fmt);
    rc = vfprintf(stderr, fmt, ap);
    va_end(ap);
    fflush(stderr);
    return rc;
}

int
myprintf(const char *fmt, ...)
{
    va_list	ap;
    int		rc;
    printf("[%d] ", utf_info.myrank); fprintf(stderr, "[%d] ", utf_info.myrank);
    va_start(ap, fmt);
    rc = vprintf(fmt, ap);

    va_start(ap, fmt);
    rc = vfprintf(stderr, fmt, ap);
    va_end(ap);

    fflush(stdout); fflush(stderr);
    return rc;
}

void *
utf_malloc(size_t sz)
{
    void	*ptr;
    ptr = malloc(sz);
    if (ptr == NULL) {
	utf_printf("%s: cannot allocate memory size(%ld)\n", __func__, sz);
	abort();
    }
    return ptr;
}

void
utf_free(void *ptr)
{
    free(ptr);
}

int
utf_rank(int *rank)
{
    *rank = utf_info.myrank;
    return 0;
}

int
utf_nprocs(int *psize)
{
    *psize = utf_info.nprocs;
    return 0;
}

int
utf_vcqh(utofu_vcq_hdl_t *vp)
{
    *vp = vcqh;
    return 0;
}

void
utf_tmr_init()
{
    int i;
    for (i = 0; i < TMR_EVT_MAX; i++) {
	tmr_max[i] = 0L; tmr_min[i] = TMR_MINVAL;
    }
    memset(tmr_count, 0, sizeof(tmr_count));
    memset(tmr_tm, 0, sizeof(tmr_tm));
    tmr_hz = tick_helz(0);
}

void
utf_tmr_start()
{
    tmr_sflag = 1;
    memset(tmr_count, 0, sizeof(tmr_count));
    memset(tmr_tm, 0, sizeof(tmr_tm));
}

int
utf_tmr_stop(int nent, struct utf_timer *ut_tim, double *tmax, double *tmin, uint64_t *counter)
{
    int	evt, cnt;
    nent = nent < TMR_EVT_MAX ? nent : TMR_EVT_MAX;
    for (evt = 0; evt < nent; evt++) {
	int	maxcnt = tmr_count[evt] < TMR_COUNT_MAX ? tmr_count[evt] : TMR_COUNT_MAX;
	ut_tim[evt].ncnt = maxcnt;
	strncpy(ut_tim[evt].sym, sym_tmr[evt], TMR_SYM_LEN);
	for (cnt = 0; cnt < maxcnt; cnt++) {
	    ut_tim[evt].tm[cnt] = CLK2USEC(tmr_tm[evt][cnt]);
	}
	tmax[evt] = CLK2USEC(tmr_max[evt]);
	tmin[evt] = tmr_min[evt] == TMR_MINVAL ? (float) 0.0 : (float) CLK2USEC(tmr_min[evt]);
	counter[evt] = tmr_count[evt];
    }
    tmr_sflag = 0;
    return nent;
}

void
fp_print(FILE *fp, char *str, char *buf, ssize_t sz, char *opt)
{
    if (utf_info.myrank == 0 && fp) fprintf(fp, "%s", str);
    if (buf) {
	size_t	bsz = strlen(buf);
	size_t	csz = strlen(opt);
	if (bsz + csz < sz) {
	    strcat(buf, opt);
	}
    }
}

void
show_compile_options(FILE *fp, char *buf, ssize_t sz)
{
    fp_print(fp, "Compile Options:\n", 0, 0, 0);
#ifdef PEEK_MEMORY
    fp_print(fp, "\tPEEK_MEMORY\n", buf, sz, "-peek");
#endif /* PEEK_MEMORY */
#ifdef REMOTE_PUT
    fp_print(fp, "\tREMOTE_PUT\n", buf, sz, "-put");
#endif /* REMOTE_PUT */
#ifdef VCQIDTAB
    fp_print(fp, "\tVCQIDTAB\n", buf, sz, "-vcqtab");
#endif /* VCQIDTAB */
#ifdef CACHE_INJECT
    fp_print(fp, "\tCACHE_INJECT\n", buf, sz, "-cache");
#endif /* VCQIDTAB */
    if (fp) fflush(fp);
}

void
utf_combuf_init()
{
    int  rc;
    long algn = sysconf(_SC_PAGESIZE);

    rc = posix_memalign((void*) &recvbuf, algn, TST_RECV_BUF_SIZE);
    SYSERRCHECK_EXIT(rc, !=, 0, "Not enough memory");
    memset(recvbuf, 0,  TST_RECV_BUF_SIZE);
    UTOFU_CALL(1, utofu_reg_mem_with_stag,
	       vcqh, (void *)recvbuf,  TST_RECV_BUF_SIZE,
	       TST_RECV_TAG, 0, &recvstadd);

    rc = posix_memalign((void*) &sendbuf, algn, TST_SEND_BUF_SIZE);
    SYSERRCHECK_EXIT(rc, !=, 0, "Not enough memory");
    memset(sendbuf, 0,  TST_SEND_BUF_SIZE);
    UTOFU_CALL(1, utofu_reg_mem_with_stag,
	       vcqh, (void *)sendbuf,  TST_SEND_BUF_SIZE,
	       TST_SEND_TAG, 0, &sendstadd);

    utf_printf("%s: recvbuf(%p) sendbuf(%p)\n", __func__, recvbuf, sendbuf);
}

void
utf_init(int argc, char **argv)
{
    int		opt;
    int	np, ppn, rank;

    if (myinitialized) {
	return;
    }
    myinitialized = 1;
    mypid = getpid();
    while ((opt = getopt(argc, argv, "d:vs:r:i:l:m")) != -1) {
	switch (opt) {
	case 'd': /* debug */
	    dflag = atoi(optarg);
	    utf_printf("dflag(0x%x)\n", dflag);
	    break;
	case 'v': /* verbose */
	    vflag = 1;
	    break;
	case 'm':
	    mflag = 1;
	    break;
	}
    }
    optind = 0; /* reset */
    DEBUG(DLEVEL_PMIX) {
	utf_printf("Calling PMIx_Init\n");
    }

    /* vcq handle are created  */
    utf_get_peers(NULL, &np, &ppn, &rank);
    utf_tni_select(utf_info.myppn, utf_info.mynrnk, &tniid, 0);
    UTOFU_CALL(1, utofu_create_vcq_with_cmp_id, tniid, 0x7, 0, &vcqh);
    UTOFU_CALL(1, utofu_query_vcq_id, vcqh, &vcqid);
    utf_printf("vcqh(%lx) vcqid(%lx)\n", vcqh, vcqid);

    /* receive buffer is allocated */
    utf_combuf_init();

    if (!getenv("UTF_NO_SHOW_OPTION")) {
	show_compile_options(stdout, NULL, 0);
	show_compile_options(stderr, NULL, 0);
    }
    utf_tmr_init();
    utf_printf("%s: utf_info.nprocs(%d) nprocs(%d)\n", __func__, utf_info.nprocs, nprocs);
    utf_barrier();
}


void
utf_stadd_free()
{
    UTOFU_CALL(1, utofu_dereg_mem, vcqh, recvstadd, 0);
    UTOFU_CALL(1, utofu_dereg_mem, vcqh, sendstadd, 0);
}

void
utf_finalize()
{
    utf_barrier();
    utf_stadd_free();
    utf_free(utf_info.vname);
    UTOFU_CALL(0, utofu_free_vcq, vcqh);
    jtofu_finalize();
    utf_PMIx_Finalize();
    fflush(NULL);
}
