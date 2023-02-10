#include <getopt.h>
#include <pmix.h>
#include <pmix_fjext.h>
#include <utofu.h>
#include <jtofu.h>
#include "utf_conf.h"
#include "utf_externs.h"
#include "utf_errmacros.h"
#include "utf_tofu.h"
#include "utf_timer.h"
#include "utf_debug.h"

/* for profile */
uint64_t	tmr_max[TMR_EVT_MAX];
uint64_t	tmr_min[TMR_EVT_MAX];
uint64_t	tmr_begin[TMR_EVT_MAX];
uint32_t	tmr_cntmax[TMR_EVT_MAX];
uint32_t	tmr_count[TMR_EVT_MAX];
uint64_t	tmr_tm[TMR_EVT_MAX][TMR_COUNT_MAX];
uint64_t	tmr_hz;
int		tmr_sflag;
int		utf_nokeep;

#define TMR_INIT_STEP_0	0
#define TMR_INIT_STEP_1	1
#define TMR_INIT_STEP_2	2
#define TMR_INIT_STEP_3	3
#define TMR_INIT_STEP_4	4
#define TMR_INIT_STEP_5	5
#define TMR_INIT_STEP_MAX 6
uint64_t	tmr_init[TMR_INIT_STEP_MAX];

static char *
sym_tmr[] = { "UTF_SEND_POST", "UTF_RECV_POST", "UTF_SENDENGINE",
	      "UTF_SNDPROGRESS", "UTF_RCVPROGRESS", "UTF_TCQ_CHECK",
	      "UTF_WAIT", "TOFU_PUT", "TOFU_GET",
	      "UTF_MEMREG", "UTF_MEMDEREG", "UTF_MALLOC", "UTF_MFREE",
	      "UTF_SEND_POST2", "UTF_RECV_LATENCY", "UTF_RECV_LATENCY2",
	      "UTF_SEND_LATENCY", "UTF_EXT1",  "UTF_EXT2",
	      "FI_TSEND", "FI_SEND", "FI_TRECV", "FI_RECV", "FI_WRITE", "FI_READ", "FI_CQREAD", 0};

#define CLK2USEC(a)	((double)(a) / ((double)tmr_hz/(double)1000000))


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
utf_intra_rank(int *rank)
{
    *rank = utf_info.mynrnk;
    return 0;
}

int
utf_intra_nprocs(int *psize)
{
    *psize = utf_info.myppn;
    return 0;
}

int
utf_vcqh(utofu_vcq_hdl_t *vp)
{
    *vp = utf_info.vcqh;
    return 0;
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


/*
 * All proccesses must call utf_init()
 *	where utf_fence() is called.
 */
int
utf_init(int argc, char **argv, int *rank, int *nprocs, int *ppn)
{
    int	opt;
    int	np, tppn, rnk, i;

#if 0 /* it takes so much in large processes */
    DEBUG(DLEVEL_INIFIN) {
	utf_printf("%s: utf_initialized(%d)\n", __func__, utf_initialized);
    }
#endif
    if (utf_initialized) {
	utf_initialized++;
	goto ext;
    }
    utf_initialized = 1;
    utf_tmr_init();
    tmr_init[TMR_INIT_STEP_0] = tick_time();
    utf_info.mypid = getpid();
    opterr = 0;
    if (argc > 0 && argv != NULL) {
	while ((opt = getopt(argc, argv, "D")) != -1) {
	    switch (opt) {
	    case 'D':
		utf_rflag = 1;
		break;
	    }
	}
	optind = 0; /* reset */
    }
    /* debug flag */
    utf_dflag = utf_getenvint("UTF_DEBUG", 0);
    /* info show flag */
    utf_iflag = utf_getenvint("UTF_INFO", 0);
    /* Tofu Comm. buffer check flag */
    utf_tflag = utf_getenvint("UTF_TOFU_COMM_CHECK", 1);
    /* reload show flag */
    utf_sflag = utf_getenvint("UTF_TOFU_SHOW_RCOUNT", 0);
    utf_reload = 0;
#if 0
    if (utf_dflag > 0) {
	utf_printf("%s: utf_dflag=%d (0x%x)\n", __func__, utf_dflag, utf_dflag);
    }
#endif
    /* utf_info is setup  */
    /* stderr redirect will be on inside the utf_get_peers */
    tmr_init[TMR_INIT_STEP_1] = tick_time();
    {
	struct tofu_vname *vn = utf_get_peers(NULL, &np, &tppn, &rnk);
	if (vn == NULL) {
	    utf_printf("%s: initialization fail\n", __func__);
	    abort();
	}
	INFO(ILEVEL_STAG) {
	    utf_vname_show(stderr);
	}
    }
    tmr_init[TMR_INIT_STEP_2] = tick_time();
    DEBUG(DLEVEL_INI) {
	if (utf_info.myrank == 0) { utf_printf("%s: S-2\n", __func__); }
    }
    utf_mem_init(utf_info.nprocs);
    i = utf_getenvint("UTF_MSGMODE", 0);
    utf_setmsgmode(i);
    i = utf_getenvint("UTF_TRANSMODE", 0);
    utf_settransmode(i);
    i = utf_getenvint("UTF_INJECT_COUNT", 0);
    utf_setinjcnt(i);
    i = utf_getenvint("UTF_RECV_COUNT", 0);
    utf_setrcvcnt(i);
    i = utf_getenvint("UTF_ASEND_COUNT", 0);
    utf_setasendcnt(i);
    i = utf_getenvint("UTF_ARMA_COUNT", 0);
    utf_setarmacnt(i);

    i = utf_getenvint("UTF_NOKEEP", 0);
    utf_nokeep = i;

    INFO(ILEVEL_MSG) {
	utf_infoshow(ILEVEL_MSG);
    }
    DEBUG(DLEVEL_INIFIN) {
	utf_printf("%s: utf_info.nprocs(%d) np(%d) utf_info.myrank(%d) rnk(%d) NO_KEEP(%d)\n",
		   __func__, utf_info.nprocs, np, utf_info.myrank, rnk, utf_nokeep);
	utf_mem_show(stderr);
    }
    /**/
    DEBUG(DLEVEL_INI) {
	if (utf_info.myrank == 0) { utf_printf("%s:B-F\n", __func__); }
    }
    tmr_init[TMR_INIT_STEP_3] = tick_time();
    utf_fence();
    tmr_init[TMR_INIT_STEP_4] = tick_time();
    DEBUG(DLEVEL_INI) {
	if (utf_info.myrank == 0) { utf_printf("%s:A-F\n", __func__); }
    }
    DEBUG(DLEVEL_INI) {
	if (utf_info.myrank == 0) {
	    utf_printf("%s: step0-1=%fmsec, step1-2=%fmsec, step2-3=%fmsec,step3-4=%fsec\n",
		       __func__,
		       CLK2USEC(tmr_init[TMR_INIT_STEP_1] - tmr_init[TMR_INIT_STEP_0])/1000.,
		       CLK2USEC(tmr_init[TMR_INIT_STEP_2] - tmr_init[TMR_INIT_STEP_1])/1000.,
		       CLK2USEC(tmr_init[TMR_INIT_STEP_3] - tmr_init[TMR_INIT_STEP_2])/1000.,
		       CLK2USEC(tmr_init[TMR_INIT_STEP_4] - tmr_init[TMR_INIT_STEP_3])/1000000.);
	}
    }
ext:
    if (rank) *rank = utf_info.myrank;
    if (nprocs) *nprocs = utf_info.nprocs;
    if (ppn) *ppn = utf_info.myppn;
    return 0;
}


void
utf_finalize(int wipe)
{
    DEBUG(DLEVEL_INIFIN) {
	utf_printf("%s: wipe(%d) utf_initialized(%d)\n", __func__, wipe, utf_initialized);
    }
    --utf_initialized;
    if (utf_initialized > 0) {
	/* still use */
	return;
    }
    if (wipe) {
	utf_req_wipe();
	utf_fence();
    } else {
	utf_fence();
    }
    if (utf_sflag) {
	utf_printf("Reload Count: %d\n", utf_reload);
    }
    DEBUG(DLEVEL_INIFIN) {
	utf_egrbuf_show(stderr);
    } else if (utf_dflag & DLEVEL_STATISTICS) {
	utf_dbg_stat(stderr);
    }
    if (utf_getenvint("UTF_COMDEBUG", 0)) {
	extern void utf_recvcntr_show(FILE*);
	extern void utf_sendctr_show();
	utf_sendctr_show();
	utf_recvcntr_show(stderr);
    }
    utf_mem_finalize();
    utf_procmap_finalize();
    jtofu_finalize();
    INFO(ILEVEL_STAT) {
	utf_stat_show();
    }
    DEBUG(DLEVEL_LOG|DLEVEL_LOG2) {
	utf_log_show(stderr);
    }
    fflush(NULL);
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

void
utf_tmr_suspend()
{
    tmr_sflag = 0;
}

void
utf_tmr_resume()
{
    tmr_sflag = 1;
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
