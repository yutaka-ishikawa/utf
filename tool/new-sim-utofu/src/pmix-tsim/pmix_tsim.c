/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "pmix.h"
#include "pmix_fjext.h"
#include "jtofu.h"
#include "tsim_debug.h"
#include <stdio.h>
#include <string.h>

struct pmix_tsim {
    uint32_t rank;
    uint32_t size;
#ifdef	notdef_unused
    uint32_t j_id; /* comm_world id */
#endif
    void *mapinfo;
    size_t mapinfo_size;
};
static struct pmix_tsim *pmix_tsim = 0;

static pmix_status_t PMIx_tsim_init(void);
static pmix_status_t PMIx_tsim_fini(void);

/* added by YI 2019/09/04 and 2019/09/11 */
extern int
_mpich_interp_pmixget(const pmix_proc_t *proc, const char key[],
                      pmix_value_t **val);
extern void   _myenv_init();
int _pmix_myrank;
int _pmix_nprocs;
/* end of added */

static int pmix_countinit = 0;

pmix_status_t PMIx_Init(pmix_proc_t *proc, pmix_info_t info[], size_t ninfo)
{
    static pmix_status_t xc = PMIX_SUCCESS;

    if (pmix_countinit == 0) {
        _myenv_init();
        if (pmix_tsim == 0) {
            usleep(100*1000); /* 100 msec wait 2019/09/13 YI */
            xc = PMIx_tsim_init();
            if (xc != PMIX_SUCCESS) { goto bad; }
            if (pmix_tsim == 0) {
                xc = PMIX_ERR_INIT; goto bad;
            }
        }
    }
    pmix_countinit++;
    {
	pmix_nspace_t nspc;
	int nc;

	nc = snprintf(nspc, sizeof (nspc), "0x%016lx@%08x",
		(uintptr_t)pmix_tsim, 0);
	if ((nc <= 0) || (nc >= sizeof (nspc))) {
	    xc = PMIX_ERR_INIT; goto bad;
	}
#ifdef	PMIX_PROC_LOAD
	PMIX_PROC_LOAD(proc, nspc, pmix_tsim->rank);
        proc->rank = pmix_tsim->rank;
#else
	strncpy(proc->nspace, nspc, PMIX_MAX_NSLEN);
	proc->rank = pmix_tsim->rank;
#endif
    }
bad:
    _pmix_myrank = pmix_tsim->rank;
    _pmix_nprocs = pmix_tsim->size;
    DEBUG {
        fprintf(stderr, "YI*** proc->rank(%d) _pmix_myrank(%d) _pmix_nprocs(%d)\n", proc->rank, _pmix_myrank, _pmix_nprocs); fflush(stderr);
    }
#if 0 /**/
    {
        char    *cp;
	jtofu_job_id_t j_id = 0;

        printf("PMIX_Init: proc->nspace(%s), proc->rank(%d)\n", proc->nspace, proc->rank); fflush(stdout);

        cp = getenv("UTOFU_WORLD_ID"); /* XXX not JTOFU_WORLD_ID */
	if (cp != 0) {
            char *p = 0; unsigned long lv;
            lv = strtoul(cp, &p, 0);
            if ((p != 0) && (p > cp) && (p[0] == '\0')) {
                j_id = (jtofu_job_id_t) lv;
            }
	}
        printf("PMIX_Init: calling jtofu_initialize j_id(%x) proc->nspace(%s) mapinfo(%p) mapsize(%ld)\n", j_id, proc->nspace, pmix_tsim->mapinfo, pmix_tsim->mapinfo_size); fflush(stdout);
        jtofu_initialize(j_id,
                         proc->rank,
                         pmix_tsim->mapinfo);
        printf("PMIX_Init: return from jtofu_initialize\n"); fflush(stdout);
    }
#endif /* 0 */
    return xc;
}

pmix_status_t PMIx_Finalize(const pmix_info_t info[], size_t ninfo)
{
    pmix_status_t xc = PMIX_SUCCESS;

    --pmix_countinit;
    if (pmix_countinit != 0) {
        return xc;
    }
    if (pmix_tsim != 0) {
	xc = PMIx_tsim_fini();
	if (xc != PMIX_SUCCESS) { goto bad; }
    }
bad:
    return xc;
}
int PMIx_Initialized(void)
{
    int rc = 0;
    if (pmix_tsim != 0) { rc = 1; }
    return rc;
}
pmix_status_t PMIx_Abort(int status, const char msg[],
    pmix_proc_t procs[], size_t nprocs)
{
    pmix_status_t xc = PMIX_SUCCESS;
    if (pmix_tsim == 0) { xc = PMIX_ERR_INIT; goto bad; }
bad:
    return xc;
}

pmix_status_t PMIx_Get(const pmix_proc_t *proc, const char key[],
    const pmix_info_t info[], size_t ninfo, pmix_value_t **val)
{
    pmix_status_t xc = PMIX_SUCCESS;
#define IS_RANK_WILDCARD(_p)	((_p)->rank == PMIX_RANK_WILDCARD)

    DEBUG{
        fprintf(stderr, "YI****[%d] %s: proc->rank(%d) ninfo(%ld) key(%s)\n",
                pmix_tsim->rank, __func__, proc->rank, ninfo, key); fflush(stderr);
    }
    if (pmix_tsim == 0) {
	xc = PMIX_ERR_INIT; goto bad;
    }
    if (key == 0) {
	xc = PMIX_ERR_BAD_PARAM; goto bad;
    }
    else if ((strcmp(key, PMIX_JOB_SIZE) == 0) && IS_RANK_WILDCARD(proc)) {
	pmix_value_t *v = 0;
	PMIX_VALUE_CREATE(v, 1);
	if (v == 0) { xc = PMIX_ERROR; goto bad; }
	v->type = PMIX_UINT32;
	v->data.uint32 = pmix_tsim->size;
	val[0] = v;
    }
    else if ((strcmp(key, FJPMIX_RANKMAP) == 0) && IS_RANK_WILDCARD(proc)) {
	pmix_value_t *v = 0;
	PMIX_VALUE_CREATE(v, 1);
	if (v == 0) { xc = PMIX_ERROR; goto bad; }
	v->type = PMIX_BYTE_OBJECT;
	/* v->data.bo */
	{
	    pmix_byte_object_t bo;
	    bo.size = pmix_tsim->mapinfo_size;
	    bo.bytes = malloc(bo.size);
	    if (bo.bytes == 0) { xc = PMIX_ERROR; goto bad; }
	    memcpy(bo.bytes, pmix_tsim->mapinfo, bo.size);
	    PMIX_BYTE_OBJECT_LOAD(&v->data.bo, bo.bytes, bo.size);
	}
	val[0] = v;
    }
    else if ((strcmp(key, PMIX_LOCAL_RANK) == 0) && ! IS_RANK_WILDCARD(proc)) {
	/* YYY PMIX_LOCAL_RANK = 0 */
	xc = PMIX_ERR_NOT_FOUND; goto bad;
    }
    else if ((strcmp(key, PMIX_LOCAL_SIZE) == 0) && IS_RANK_WILDCARD(proc)) {
	/* YYY PMIX_LOCAL_SIZE = 1 */
	xc = PMIX_ERR_NOT_FOUND; goto bad;
    }
    else if (strcmp(key, PMIX_RANK) == 0) {
	pmix_value_t *v = 0;
	PMIX_VALUE_CREATE(v, 1);
	if (v == 0) { xc = PMIX_ERR_NOT_FOUND; goto bad; }
	v->type = PMIX_PROC_RANK;
        v->data.rank = pmix_tsim->rank;
        val[0] = v;
    } else {
        xc = _mpich_interp_pmixget(proc, key, val);
    }
bad:
    return xc;
}


static pmix_status_t PMIx_tsim_init(void)
{
    pmix_status_t xc = PMIX_SUCCESS; int el = 0;
    uint32_t tsim_rank = UINT32_MAX;
    uint32_t tsim_size = 0;
#ifdef	notdef_unused
    uint32_t tsim_j_id = 0; /* comm_world id */
#endif
    void *tsim_mivp = 0;
    size_t tsim_misz = 0;
    struct pmix_tsim *pmix_tsim_proc = 0;

    if (pmix_tsim != 0) {
	goto bad; /* XXX - is not an error */
    }
    /* rank */
    {
	const char *rank_nams[] = {
	    "UTOFU_WORLD_RANK",
	    "OMPI_COMM_WORLD_RANK",
	    "OMPI_MCA_orte_ess_vpid",
	    "PMI_RANK",
	    0
	};
	int ii;
	const char *namp;
	ii = 0;
	while ((namp = rank_nams[ii]) != 0) {
	    const char *cp = getenv(namp); char *p = 0; long lv;
	    if (cp != 0) {
		lv = strtol(cp, &p, 10);
		if ((p != 0) && (p > cp) && (lv >= 0)) {
		    tsim_rank = (uint32_t)lv;
		    break;
		}
	    }
	    ii++;
	}
	if (tsim_rank == UINT32_MAX) {
	    xc = PMIX_ERR_INIT; el = __LINE__; goto bad;
	}
    }
    /* size */
    {
	const char *size_nams[] = {
	    "UTOFU_WORLD_SIZE",
	    "OMPI_COMM_WORLD_SIZE",
	    "OMPI_MCA_orte_ess_num_procs",
	    "PMI_SIZE",
            "PJM_MPI_PROC",
	    0
	};
	int ii;
	const char *namp;
	ii = 0;
	while ((namp = size_nams[ii]) != 0) {
	    const char *cp = getenv(namp); char *p = 0; long lv;

            // fprintf(stderr, "YI**** getenv(%s) = %p (%s)\n", namp, cp, cp == 0 ? "": cp); fflush(stderr);
	    if (cp != 0) {
		lv = strtol(cp, &p, 10);
		if ((p != 0) && (p > cp) && (lv > 0)) {
		    tsim_size = (uint32_t)lv;
		    break;
		}
	    }
	    ii++;
	}
        DEBUG {
            fprintf(stderr, "YI**** tsim_size = %d\n", tsim_size); fflush(stderr);
        }
	if (tsim_size == 0) {
	    xc = PMIX_ERR_INIT; el = __LINE__; goto bad;
	}
    }
#ifdef	notdef_unused
    /* j_id */
    {
	const char *j_id_nams[] = {
	    "UTOFU_WORLD_ID",
	    "OMPI_MCA_orte_ess_jobid",
	    /* "PMI_FOO", */
	    0
	};
	int ii;
	const char *namp;
	ii = 0;
	while ((namp = j_id_nams[ii]) != 0) {
	    const char *cp = getenv(namp); char *p = 0; long lv;
	    if (cp != 0) {
		lv = strtol(cp, &p, 10);
		if ((p != 0) && (p > cp) && (lv > 0)) {
		    tsim_j_id = (uint32_t)lv;
		    break;
		}
	    }
	    ii++;
	}
	if (tsim_j_id == 0) {
	}
    }
    // printf("=== rank %d size %d cwid %d\n", tsim_rank, tsim_size, tsim_j_id);
#else
    // printf("=== rank %d size %d\n", tsim_rank, tsim_size);
#endif
    {
	extern int jtofu_create_mapinfo_by_envvars(void **vpp_mapinfo,
	    size_t *p_mapinfo_size, unsigned int *num_proc);
	int jc;
	unsigned int nprocs = 0;

	jc = jtofu_create_mapinfo_by_envvars(&tsim_mivp, &tsim_misz, &nprocs);
	if (jc != 0 /* JTOFU_SUCCESS */ ) {
	    xc = PMIX_ERR_INIT; el = __LINE__; goto bad;
	}
	if ((tsim_mivp == 0) || (tsim_misz == 0) || (nprocs == 0)) {
	    xc = PMIX_ERR_INIT; el = __LINE__; goto bad;
	}
	// printf("nprocs %d\n", nprocs);
    }

    pmix_tsim_proc = calloc(1, sizeof (pmix_tsim_proc[0]));
    if (pmix_tsim_proc == 0) {
	xc = PMIX_ERROR /* PMIX_ERR_NOMEM */; el = __LINE__; goto bad;
    }
    pmix_tsim_proc->rank = tsim_rank;
    pmix_tsim_proc->size = tsim_size;
#ifdef	notdef_unused
    pmix_tsim_proc->j_id = tsim_j_id;
#endif
    pmix_tsim_proc->mapinfo = tsim_mivp; tsim_mivp = 0; /* ZZZ */
    pmix_tsim_proc->mapinfo_size = tsim_misz;

    /* copy to pmix_tsim */
    pmix_tsim = pmix_tsim_proc;

bad:
    if (el != 0) {
	fprintf(stderr, "ERROR %d\t%s():%s:%d\n", xc, __func__, __FILE__, el);
	fflush(stderr);
    }
    if (tsim_mivp != 0) {
	free(tsim_mivp); tsim_mivp = 0;
    }
    return xc;
}

static pmix_status_t PMIx_tsim_fini(void)
{
    pmix_status_t xc = PMIX_SUCCESS; int el = 0;
    struct pmix_tsim *pmix_tsim_proc = 0;

    if (pmix_tsim == 0) {
	goto bad; /* XXX - is not an error */
    }
    pmix_tsim_proc = pmix_tsim;
    pmix_tsim = 0;

    if (pmix_tsim_proc->mapinfo != 0) {
	free(pmix_tsim_proc->mapinfo); pmix_tsim_proc->mapinfo = 0;
    }
    free(pmix_tsim_proc);

bad:
    if (el != 0) {
	fprintf(stderr, "ERROR %d\t%s():%s:%d\n", xc, __func__, __FILE__, el);
	fflush(stderr);
    }
    return xc;
}
