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

    DEBUG(DLEVEL_INIFIN) {
	utf_printf("%s: utf_initialized(%d)\n", __func__, utf_initialized);
    }
    if (utf_initialized) {
	utf_initialized++;
	goto ext;
    }
    utf_initialized = 1;
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
    utf_dflag = utf_getenvint("UTF_DEBUG");
    /* info show flag */
    utf_iflag = utf_getenvint("UTF_INFO");
#if 0
    if (utf_dflag > 0) {
	utf_printf("%s: utf_dflag=%d (0x%x)\n", __func__, utf_dflag, utf_dflag);
    }
#endif
    /* utf_info is setup  */
    /* stderr redirect will be on inside the utf_get_peers */
    utf_get_peers(NULL, &np, &tppn, &rnk);
    utf_mem_init();
    i = utf_getenvint("UTF_MSGMODE");
    utf_setmsgmode(i);
    utf_tmr_init();
    i = utf_getenvint("UTF_NOKEEP");
    utf_nokeep = i;
    i = utf_getenvint("UTF_DBGTIMER_INTERVAL");
    utf_dbg_timer = i;
    i = utf_getenvint("UTF_DBGTIMER_ACTION");
    utf_dbg_timact = i;
    utf_dbg_init();

    INFO(ILEVEL_MSG) {
	utf_infoshow(ILEVEL_MSG);
    }
    DEBUG(DLEVEL_INIFIN) {
	utf_printf("%s: utf_info.nprocs(%d) np(%d) utf_info.myrank(%d) rnk(%d) NO_KEEP(%d)\n",
		   __func__, utf_info.nprocs, np, utf_info.myrank, rnk, utf_nokeep);
	utf_mem_show(stderr);
    }
    /**/
    utf_fence();
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
    DEBUG(DLEVEL_INIFIN) {
	utf_egrbuf_show(stderr);
    } else if (utf_dflag & DLEVEL_STATISTICS) {
	utf_statistics(stderr);
    }
    utf_mem_finalize();
    utf_procmap_finalize();
    jtofu_finalize();
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
