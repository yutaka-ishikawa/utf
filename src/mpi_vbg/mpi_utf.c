#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <mpi.h>
#include <utf.h>
#include <utofu.h>
#include <jtofu.h>
#include <utf_bg.h>

#define COMM_MPICHLEVEL_HANDLE

extern int utf_progress();
extern int utf_MPI_hook(MPI_Comm, uint32_t *addr_tab, uint32_t *count);

#define UTF_BG_MIN_BARRIER_SIZE	4
static int	mpi_bg_enabled = 0;
static int	mpi_bg_disable = 0;
static int	mpi_bg_dbg = 0;
static int	mpi_bg_barrier = 0;
static useconds_t	mpi_bg_iniwait = 0;
static int	mpi_bg_confirm = 0;
static int	mpi_bg_warning = 0;
static utf_coll_group_t mpi_world_grp;
static utf_bg_info_t	*mpi_bg_bginfo;
static int	mpi_bg_nprocs, mpi_bg_myrank;
static int	mpi_init_nfirst = 0;
static int	errprintf(int myrank, const char *fmt, ...);

#ifdef COMM_MPICHLEVEL_HANDLE
#include "commaddr.c"
#else
#include "commgroup.c"
#endif /* COMM_MPICHLEVEL_HANDLE */

#define MALLOC_CHECK(lbl, rc, func)			\
{							\
    rc = func;						\
    if (rc == 0) {					\
	errprintf(mpi_bg_myrank, "%s: Cannot allocate memory.", __func__); \
	goto lbl;					\
    }							\
}

#define UTFCALL_CHECK(lbl, rc, func)			\
{							\
    rc = func;						\
    if (rc != UTF_SUCCESS) goto lbl;			\
}

#define MPICALL_CHECK_RETURN(rc, func)			\
{							\
    rc = func;						\
    if (rc != MPI_SUCCESS) return rc;			\
}

#define COMINFO_GET_CHECK_ABORT(ent, comm)		\
{							\
    ent = cominfo_get(comm);				\
    if (ent == NULL) {					\
	errprintf(mpi_bg_myrank, "%s: Internal error, cannot get info about Communicator (%lx)\n", __func__, comm);\
	abort();					\
    }							\
}

#define COMINFO_GET_CHECK_DO(lbl, ent, comm)		\
{							\
    ent = cominfo_get(comm);				\
    if (ent != NULL) {					\
	goto lbl;					\
    }							\
}

#define MPICALL_CHECK(lbl, rc, func)			\
{							\
    rc = func;						\
    if (rc != MPI_SUCCESS) goto lbl;			\
}

#define COMMGROUP_ERRCHECK(ent)						\
    if (ent == NULL) {							\
	fprintf(stderr, "%d:%s internal error\n", __LINE__, __func__);	\
    }

#define NOTYET_SUPPORT fprintf(stderr, "%s is not supported for keeping ranks.\n", __func__)

#define IF_BG_DISABLE(func)			\
{						\
    if (mpi_bg_enabled == 0) {			\
	int rc = func;				\
	return rc;				\
    }						\
}

static int
errprintf(int myrank, const char *fmt, ...)
{
    va_list	ap;
    int		rc;
    printf("[%d] ", myrank); fprintf(stderr, "[%d] ", myrank);
    va_start(ap, fmt);
    rc = vprintf(fmt, ap);
    rc = vfprintf(stderr, fmt, ap);
    va_end(ap);
    fflush(stdout); fflush(stderr);
    return rc;
}

static int
myprintf(int myrank, const char *fmt, ...)
{
    va_list	ap;
    int		rc;

    if (myrank != 0) return 0;
    va_start(ap, fmt);
    rc = vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);
    return rc;
}

static void
option_get()
{
    char	*cp = getenv("UTF_BG_DBG");
    if (cp && atoi(cp) != 0) {
	mpi_bg_dbg = 1;
    }
    cp = getenv("UTF_BG_DISABLE");
    if (cp && atoi(cp) != 0) {
	mpi_bg_disable = 1;
    }
    cp = getenv("UTF_BG_INITWAIT");
    if (cp && atoi(cp) != 0) {
	mpi_bg_iniwait = atol(cp);
    }
    cp = getenv("UTF_BG_BARRIER");
    if (cp && atoi(cp) != 0) {
	mpi_bg_barrier = 1;
    }
    cp = getenv("UTF_BG_CONFIRM");
    if (cp && atoi(cp) != 0) {
	mpi_bg_confirm = 1;
    }
    cp = getenv("UTF_BG_WARNING");
    if (cp && atoi(cp) != 0) {
	mpi_bg_warning = 1;
    }
}

static inline void
mpi_bg_poll_barrier(utf_coll_group_t bg_grp)
{
    while (utf_poll_barrier(bg_grp) == UTF_ERR_NOT_COMPLETED) {
	utf_progress();
    }
}

static inline void
mpi_bg_poll_reduce(utf_coll_group_t bg_grp, void **data)
{
    while (utf_poll_reduce(bg_grp, data) == UTF_ERR_NOT_COMPLETED) {
	utf_progress();
    }
}

static inline int
mpi_bg_init(MPI_Comm comm, int nprocs, int myrank, uint32_t *rankset)
{
    int rc = 0;
    utf_bg_info_t	*lcl_bginfo, *rmt_bginfo;

    lcl_bginfo = malloc(sizeof(utf_bg_info_t) * nprocs);
    rmt_bginfo = malloc(sizeof(utf_bg_info_t) * nprocs);
    if (lcl_bginfo == NULL || rmt_bginfo == NULL) {
	errprintf(myrank, "Cannot allocate working memory for VBG procs(%d)\n", nprocs);
	goto ext;
    }
    UTFCALL_CHECK(err1, rc, utf_bg_alloc(rankset, nprocs, myrank,
					 jtofu_query_max_proc_per_node(),
					 UTF_BG_TOFU, lcl_bginfo));
    MPICALL_CHECK(err2, rc, PMPI_Alltoall(lcl_bginfo, sizeof(utf_bg_info_t), MPI_BYTE,        
					  rmt_bginfo, sizeof(utf_bg_info_t), MPI_BYTE, comm));
    /* Communicator related initialization */
#ifdef COMM_MPICHLEVEL_HANDLE
    {
	static utf_coll_group_t collgrp;
	/* setting up VBG */
	rc = utf_bg_init(rankset, nprocs, myrank, rmt_bginfo, &collgrp);
	if (rc == UTF_SUCCESS) {
	    cominfo_reg(comm, (void*) collgrp, rankset, 0);
	    if (comm == MPI_COMM_WORLD) {
		mpi_bg_bginfo = rmt_bginfo; mpi_world_grp = collgrp;
		mpi_bg_nprocs = nprocs; mpi_bg_myrank = myrank;
	    } else {
		free(rmt_bginfo);
	    }
	} else {
	    if (mpi_bg_warning) {
		errprintf(mpi_bg_myrank, "VBG-WARNING: No more VBG resources\n");
	    }
	    goto ext;
	}
    }
#else
    {
	utfslist_entry_t *slst;
	struct grpinfo_ent	*grpent;

	comgrpinfo_init(GROUPINFO_SIZE);
	/* setting up for MPI_COMM_WORLD */
	rc = utf_bg_init(grpidx, nprocs, myrank, mpi_bg_bginfo, &mpi_world_grp);
	if (rc == UTF_SUCCESS) {
	    slst = utfslist_remove(&grpinfo_freelst);
	    grpent = container_of(slst, struct grpinfo_ent, slst);
	    grpent->grp = 0;
	    grpent->size = nprocs;
	    grpent->ranks = grpidx;
	    cominfo_reg(MPI_COMM_WORLD, grpent, (void*) mpi_world_grp, 0);
	    if (comm == MPI_COMM_WORLD) {
		mpi_bg_bginfo = rmt_bginfo; mpi_world_grp = collgrp;
		mpi_bg_nprocs = nprocs; mpi_bg_myrank = myrank;
	    } else {
		free(rmt_bginfo);
	    }
	} else {
	    if (mpi_bg_warning) {
		errprintf(mpi_bg_myrank, "VBG-WARNING: No more VBG resources\n");
	    }
	    goto ext;
	}
    }
#endif /* COMM_MPICHLEVEL_HANDLE */
    mpi_bg_enabled = 1;
    free(lcl_bginfo);
    return MPI_SUCCESS;

    /* error handling */
err1:
    errprintf(myrank, "error on utf_bg_alloc(), rc = %d\n", rc); goto ext;
err2:
    errprintf(myrank, "error on PMI_Alltoall(), rc = %d\n", rc);
    free(rmt_bginfo);
ext:
    free(lcl_bginfo);
    return rc;
}

int
MPI_Init(int *argc, char ***argv)
{
    int	rc, i;
    int	nprocs, myrank;
    uint32_t *rankset;

    if (mpi_init_nfirst > 0)  {
	/* Already called, and thus no need to initialize VBG */
	return PMPI_Init(argc, argv);
    }
    mpi_init_nfirst++;
    option_get();
    MPICALL_CHECK(err0, rc, PMPI_Init(argc, argv));
    if (mpi_bg_disable == 1) {
	if (mpi_bg_confirm) {
	    myprintf(0, "[%d] *** UTF VBG is disabled **\n", mpi_bg_myrank);
	}
	return 0;
    }
    cominfo_init(COMINFO_SIZE);
    PMPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    PMPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    MALLOC_CHECK(err0, rankset, malloc(sizeof(uint32_t)*nprocs));
    /* Rank info */
    for (i = 0; i < nprocs; i++) {
	rankset[i] = i;
    }
    MPICALL_CHECK(err1, rc, mpi_bg_init(MPI_COMM_WORLD, nprocs, myrank, rankset));
    if (mpi_bg_iniwait > 0) {
	usleep(mpi_bg_iniwait);
	if (mpi_bg_confirm) {
	    myprintf(0, "[%d] *** UTF VBG (%d usec waited)**\n", mpi_bg_myrank, mpi_bg_iniwait);
	}
    }
    MPICALL_CHECK(err1, rc, PMPI_Barrier(MPI_COMM_WORLD));
    if (mpi_bg_barrier) {
	if (mpi_bg_confirm) {
	    myprintf(0, "[%d] PASS\n", mpi_bg_myrank);
	}
	MPICALL_CHECK(err1, rc, PMPI_Barrier(MPI_COMM_WORLD));
    }
    if (mpi_bg_confirm) {
	myprintf(0, "[%d] *** UTF VBG is enabled **\n", mpi_bg_myrank);
    }
    return rc;
err0:
    return -1;
err1:
    free(rankset);
    return rc;
}

int
MPI_Init_thread(int *argc, char ***argv, int required, int *provided)
{
    int	rc, i;
    int	nprocs, myrank;
    uint32_t *rankset;

    if (mpi_init_nfirst > 0)  {
	/* Already called, and thus no need to initialize VBG */
	return PMPI_Init_thread(argc, argv, required, provided);
    }
    mpi_init_nfirst++;
    option_get();
    MPICALL_CHECK(err0, rc, PMPI_Init_thread(argc, argv, required, provided));
    if (mpi_bg_disable == 1) {
	if (mpi_bg_confirm) {
	    myprintf(0, "[%d] *** UTF VBG is disabled **\n", mpi_bg_myrank);
	}
	return 0;
    }
    PMPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    PMPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    MALLOC_CHECK(err0, rankset, malloc(sizeof(uint32_t)*nprocs));
    /* Rank info */
    for (i = 0; i < nprocs; i++) {
	rankset[i] = i;
    }
    MPICALL_CHECK(err1, rc, mpi_bg_init(MPI_COMM_WORLD, nprocs, myrank, rankset));
    if (mpi_bg_iniwait > 0) {
	usleep(mpi_bg_iniwait);
	if (mpi_bg_confirm) {
	    myprintf(0, "[%d] *** UTF VBG (%d usec waited)**\n", mpi_bg_myrank, mpi_bg_iniwait);
	}
    }
    MPICALL_CHECK(err1, rc, PMPI_Barrier(MPI_COMM_WORLD));
    if (mpi_bg_barrier) {
	if (mpi_bg_confirm) {
	    myprintf(0, "[%d] PASS\n", mpi_bg_myrank);
	}
	MPICALL_CHECK(err1, rc, PMPI_Barrier(MPI_COMM_WORLD));
    }
    if (mpi_bg_confirm) {
	myprintf(0, "[%d] *** UTF VBG is enabled **\n", mpi_bg_myrank);
    }
err0:
    return rc;
err1:
    free(rankset);
    return rc;
}

int
MPI_Finalize()
{
    int	rc;

    if (mpi_bg_enabled) {
	UTFCALL_CHECK(err1, rc, utf_bg_free(mpi_world_grp));
	mpi_bg_enabled = 0;
    }
ext:
    rc = PMPI_Finalize();
    return rc;
err1:
    errprintf(mpi_bg_myrank, "error on utf_bg_free %d\n", rc);
    goto ext;
}

int
MPI_Barrier(MPI_Comm comm)
{
    int	rc = 0;
    struct cominfo_ent *ent;

    IF_BG_DISABLE(PMPI_Barrier(comm));
    COMINFO_GET_CHECK_DO(cont, ent, comm);
    /* VBG resource has not been allocated */
    rc = PMPI_Barrier(comm);
    goto ext;
cont:
#ifdef COMM_MPICHLEVEL_HANDLE
    if (ent->nprocs < UTF_BG_MIN_BARRIER_SIZE) {
	MPICALL_CHECK_RETURN(rc, PMPI_Barrier(comm));
    } else {
	UTFCALL_CHECK(ext, rc,  utf_barrier(ent->bgrp));
	mpi_bg_poll_barrier(ent->bgrp);
    }
#else
    assert(ent->grp != NULL);
    if (ent->grp->size < UTF_BG_MIN_BARRIER_SIZE) {
	MPICALL_CHECK_RETURN(rc, PMPI_Barrier(comm));
    } else {
	UTFCALL_CHECK(ext, rc,  utf_barrier(ent->bgrp));
	mpi_bg_poll_barrier(ent->bgrp);
    }
#endif
ext:
    return rc;
}

static enum utf_datatype
mpitype_to_utf(MPI_Datatype datatype)
{
    switch(datatype) {
    case MPI_DOUBLE:
	return UTF_DATATYPE_DOUBLE;
    case MPI_LONG: case MPI_LONG_LONG: /* MPI_LONG_LONG_INT */
	return UTF_DATATYPE_INT64_T;
    case MPI_UNSIGNED_CHAR: case MPI_BYTE:
	return UTF_DATATYPE_UINT8_T;
    case MPI_WCHAR: case MPI_UNSIGNED_SHORT:
	return UTF_DATATYPE_UINT16_T;
    case MPI_UNSIGNED:
	return UTF_DATATYPE_UINT32_T;
    case MPI_UNSIGNED_LONG: case MPI_UNSIGNED_LONG_LONG:
	return UTF_DATATYPE_UINT64_T;
    case MPI_CHAR: case MPI_SIGNED_CHAR:
	return UTF_DATATYPE_INT8_T;
    case MPI_SHORT:
	return UTF_DATATYPE_INT16_T;
    case MPI_INT:
	return UTF_DATATYPE_INT32_T;
    case MPI_FLOAT:
	return UTF_DATATYPE_FLOAT;
    case MPI_LONG_DOUBLE:
	return UTF_DATATYPE_DOUBLE;
    default:
	return 0;
#if 0	/* MUST CHECK those types */
	return UTF_DATATYPE_FLOAT16;
	return UTF_DATATYPE_FLOAT16_COMPLEX;
	return UTF_DATATYPE_FLOAT_COMPLEX;
	return UTF_DATATYPE_DOUBLE_COMPLEX;
	return UTF_DATATYPE_SHORT_INT;
	return UTF_DATATYPE_2INT;
#endif
    }
}

static enum utf_datatype
mpiop_to_utf(MPI_Op op)
{
    switch(op) {
    case MPI_SUM:
	return UTF_REDUCE_OP_SUM;
    case MPI_MAX:
	return UTF_REDUCE_OP_MAX;
    case MPI_MAXLOC:
	return UTF_REDUCE_OP_MAXLOC;
    case MPI_BAND:
	return UTF_REDUCE_OP_BAND;
    case MPI_BOR:
	return UTF_REDUCE_OP_BOR;
    case MPI_BXOR:
	return UTF_REDUCE_OP_BXOR;
    case MPI_MIN:
	return UTF_REDUCE_OP_MIN;
    case MPI_LAND:
	return UTF_REDUCE_OP_LAND;
    case MPI_LOR:
	return UTF_REDUCE_OP_LOR;
    case MPI_LXOR:
	return UTF_REDUCE_OP_LXOR;
    case MPI_MINLOC:
	return UTF_REDUCE_OP_MINLOC;
    case MPI_PROD:
    case MPI_REPLACE:
    case MPI_NO_OP:
    default:
	return 0;
    }
}

int
MPI_Bcast(void *buf, int count, MPI_Datatype datatype, int root, MPI_Comm comm)
{
    int	rc;
    struct cominfo_ent *ent;
    int	siz;
    size_t	tsize;

    IF_BG_DISABLE(PMPI_Bcast(buf, count, datatype, root, comm));
    COMINFO_GET_CHECK_DO(cont, ent, comm);
    /* VBG resource has not been allocated */
    rc = PMPI_Bcast(buf, count, datatype, root, comm);
    goto ext;
cont:
    rc = MPI_Type_size(datatype, &siz);
    tsize = siz * count;
    /* UTF_BG_REDUCE_ULMT_ELMS_48 48 */
    rc = utf_broadcast(ent->bgrp, buf, tsize, NULL, root);
    if (rc == UTF_SUCCESS) {
	double	data;
	mpi_bg_poll_reduce(ent->bgrp, (void**)&data);
    } else if (rc == UTF_ERR_NOT_AVAILABLE) {
	//myprintf(0, "[%d] %s: calling PMPI_Bcast\n", mpi_bg_myrank, __func__);
	rc = PMPI_Bcast(buf, count, datatype, root, comm);
    }
ext:
    return rc;
}

int
MPI_Reduce(const void *sbuf, void *rbuf, int count, MPI_Datatype datatype,
	   MPI_Op op, int root, MPI_Comm comm)
{
    int	rc;
    struct cominfo_ent *ent;
    enum utf_datatype  utf_type;
    enum utf_reduce_op utf_op;

    // myprintf(root, "[%d] %s: datatype=%x op=%x\n", mpi_bg_myrank, __func__, datatype, op);
    IF_BG_DISABLE(PMPI_Reduce(sbuf, rbuf, count, datatype, op, root, comm));
    COMINFO_GET_CHECK_DO(cont, ent, comm);
    /* VBG resource has not been allocated */
    rc = PMPI_Reduce(sbuf, rbuf, count, datatype, op, root, comm);
    goto ext;
cont:
    utf_type = mpitype_to_utf(datatype);
    utf_op = mpiop_to_utf(op);
    if (utf_type != 0 && utf_op != 0) {
	double	data;
	rc = utf_reduce(ent->bgrp, sbuf, count, NULL, rbuf, NULL, utf_type, utf_op, root);
	if (rc == UTF_ERR_NOT_AVAILABLE) {
	    goto pmpi_call;
	} else if (rc != UTF_SUCCESS) {
	    myprintf(0, "[%d] %s: rc = %d\n", mpi_bg_myrank, __func__, rc);
	    goto pmpi_call;
	}
	/* progress */
	mpi_bg_poll_reduce(ent->bgrp, (void**)&data);
	goto ext;
    } else {
    pmpi_call:
	//myprintf(0, "[%d] %s: calling PMPI_Reduce\n", mpi_bg_myrank, __func__);
	rc = PMPI_Reduce(sbuf, rbuf, count, datatype, op, root, comm);
    }
ext:
    return rc;
}

int
MPI_Allreduce(const void *sbuf, void *rbuf, int count, MPI_Datatype datatype,
	      MPI_Op op, MPI_Comm comm)
{
    int	rc;
    struct cominfo_ent *ent;
    enum utf_datatype  utf_type;
    enum utf_reduce_op utf_op;

    //myprintf(mpi_bg_myrank, "[%d] %s: datatype=%x op=%x\n", mpi_bg_myrank, __func__, datatype, op);
    IF_BG_DISABLE(PMPI_Allreduce(sbuf, rbuf, count, datatype, op, comm));
    COMINFO_GET_CHECK_DO(cont, ent, comm);
    rc = PMPI_Allreduce(sbuf, rbuf, count, datatype, op, comm);
    goto ext;
cont:
    utf_type = mpitype_to_utf(datatype);
    utf_op = mpiop_to_utf(op);
    if (utf_type != 0 && utf_op != 0) {
	double	data;
	rc = utf_allreduce(ent->bgrp, sbuf, count, NULL, rbuf, NULL, utf_type, utf_op);
	if (rc == UTF_ERR_NOT_AVAILABLE) {
	    goto pmpi_call;
	} else if (rc != UTF_SUCCESS) {
	    myprintf(0, "[%d] %s: rc = %d\n", mpi_bg_myrank, __func__, rc);
	    goto pmpi_call;
	}
	/* progress */
	mpi_bg_poll_reduce(ent->bgrp, (void**)&data);
	goto ext;
    } else {
    pmpi_call:
	//myprintf(0, "[%d] %s: calling PMPI_Allreduce\n", mpi_bg_myrank, __func__);
	rc = PMPI_Allreduce(sbuf, rbuf, count, datatype, op, comm);
    }
ext:
    return rc;
}

#ifdef COMM_MPICHLEVEL_HANDLE
/*
 * process rank numbers are obrained from MPICH hook function for Tofu
 */
static int
mpi_comm_bg_init(MPI_Comm comm)
{
    int	rc = 0;
    int	nprocs, myrank;
    uint32_t	*rankset, count;
    
    size_t	sz;
    if (comm == MPI_COMM_NULL) {
	/* nothing */ goto ext;
    }
    PMPI_Comm_size(comm, &nprocs);
    PMPI_Comm_rank(comm, &myrank);
    sz = sizeof(uint32_t)*nprocs;
    MALLOC_CHECK(err0, rankset, malloc(sz));
    /* Rank info */
    count = nprocs;
    MPICALL_CHECK(err0, rc, utf_MPI_hook(comm, rankset, &count));
    if (count != nprocs) {
	errprintf(mpi_bg_myrank, "%s: internal error count must be %d (nprocs), but %d\n", __func__, nprocs, count);
	goto err0;
    }
#if 0
    {	int i;
	for (i = 0; i < nprocs; i++) {
	    errprintf(mpi_bg_myrank, "%s: rankset[%d] = %d\n", __func__, i, rankset[i]);
	}
    }
#endif
    rc = mpi_bg_init(comm, nprocs, myrank, rankset);
    if (rc != 0) {
	/* Cannot allocate VBG resources */
	free(rankset);
    }
    /* Goging to barrier synchronization in any return cases */
    MPICALL_CHECK(ext, rc, PMPI_Barrier(comm));
ext:
    return rc;
err0:
    return -1;
}

int
MPI_Comm_create(MPI_Comm comm, MPI_Group group, MPI_Comm *newcomm)
{
    int rc;
    MPICALL_CHECK_RETURN(rc, PMPI_Comm_create(comm, group, newcomm));
    rc = mpi_comm_bg_init(*newcomm);
    return rc;
}

int
MPI_Comm_dup(MPI_Comm comm, MPI_Comm *newcomm)
{
    int rc;
    struct cominfo_ent *parent;
    MPICALL_CHECK_RETURN(rc, PMPI_Comm_dup(comm, newcomm));
    COMINFO_GET_CHECK_DO(cont, parent, comm);
    /* Cannot find the original communicator */
    goto ext;
cont:
    cominfo_reg(*newcomm, parent->bgrp, parent->rankset, parent);
ext:
    return rc;
}

int
MPI_Comm_dup_with_info(MPI_Comm comm, MPI_Info info, MPI_Comm *newcomm)
{
    int rc;
    struct cominfo_ent *parent;
    MPICALL_CHECK_RETURN(rc, PMPI_Comm_dup_with_info(comm, info, newcomm));
    COMINFO_GET_CHECK_DO(cont, parent, comm);
    /* Cannot find the original communicator */
    goto ext;
cont:
    cominfo_reg(*newcomm, parent->bgrp, parent->rankset, parent);
ext:
    return rc;
}

int
MPI_Comm_split(MPI_Comm comm, int color, int key, MPI_Comm *newcomm)
{
    int rc;
    MPICALL_CHECK_RETURN(rc, PMPI_Comm_split(comm, color, key, newcomm));
    rc = mpi_comm_bg_init(*newcomm);
    return rc;
}

int
MPI_Comm_free(MPI_Comm *comm)
{
    int rc;
    utf_coll_group_t	grp;
    MPI_Comm	svd_comm = *comm;

    MPICALL_CHECK_RETURN(rc, PMPI_Comm_free(comm));
    grp = (utf_coll_group_t) cominfo_unreg(svd_comm);
    if (grp != NULL) {
	if (mpi_bg_confirm) {
	    utf_printf("%s: FREE grp(%p)\n", __func__, grp);
	}
	utf_bg_free(grp);
    }
    return rc;
}

int
MPI_Cart_create(MPI_Comm comm_old, int ndims, const int dims[], const int periods[],
		int reorder, MPI_Comm *comm_cart)
{
    int rc;
    MPICALL_CHECK_RETURN(rc, PMPI_Cart_create(comm_old, ndims, dims, periods, reorder, comm_cart));
    rc = mpi_comm_bg_init(*comm_cart);
    return rc;
}

int
MPI_Graph_create(MPI_Comm comm_old, int nnodes, const int indx[], const int edges[],
		 int reorder, MPI_Comm *comm_graph)
{
    int rc;
    MPICALL_CHECK_RETURN(rc, PMPI_Graph_create(comm_old, nnodes, indx, edges, reorder, comm_graph));
    rc = mpi_comm_bg_init(*comm_graph);
    return rc;
}

int
MPI_Cart_sub(MPI_Comm comm, const int remain_dims[], MPI_Comm *newcomm)
{
    int rc;
    MPICALL_CHECK_RETURN(rc, PMPI_Cart_sub(comm, remain_dims, newcomm));
    rc = mpi_comm_bg_init(*newcomm);
    return rc;
}

int
MPI_Dist_graph_create_adjacent(MPI_Comm comm_old, int indegree, const int sources[],
			       const int sourceweights[], int outdegree,
			       const int destinations[], const int destweights[],
			       MPI_Info info, int reorder, MPI_Comm *comm_dist_graph)
{
    int rc;
    MPICALL_CHECK_RETURN(rc, PMPI_Dist_graph_create_adjacent(comm_old, indegree, sources,
			sourceweights, outdegree, destinations, destweights,
			info, reorder, comm_dist_graph));
    rc = mpi_comm_bg_init(*comm_dist_graph);
    return rc;
}

int
MPI_Dist_graph_create(MPI_Comm comm_old, int n, const int sources[], const int degrees[],
		      const int destinations[], const int weights[], MPI_Info info,
		      int reorder, MPI_Comm *comm_dist_graph)
{
    int rc;
    MPICALL_CHECK_RETURN(rc, PMPI_Dist_graph_create(comm_old, n, sources, degrees,
			       destinations, weights, info,
					     reorder, comm_dist_graph));
    rc = mpi_comm_bg_init(*comm_dist_graph);
    return rc;
}

#if 0
MPI_Comm_idup
#endif
#else
/*
 * User-level handling process rank numbers in communicator
 */
int
MPI_Comm_group(MPI_Comm comm, MPI_Group *group)
{
    int rc;

    MPICALL_CHECK_RETURN(rc, PMPI_Comm_group(comm, group));
    {
	struct cominfo_ent	*coment = cominfo_get(comm);
	struct grpinfo_ent	*grpent;
	COMMGROUP_ERRCHECK(coment);
	grpent = grpinfo_dup(coment->grp, *group);
	utfslist_append(&grpinfo_lst, &grpent->slst);
    }
    return rc;
}

/*
 * union: All elements of the rst group (group1), followed by all elements
 *	  of second group (group2) not in the rst group.
 */
int
MPI_Group_union(MPI_Group group1, MPI_Group group2, MPI_Group *newgroup)
{
    int rc;
    struct grpinfo_ent	*grpent1 = grpinfo_get(group1);
    struct grpinfo_ent	*grpent2 = grpinfo_get(group2);

    MPICALL_CHECK_RETURN(rc, PMPI_Group_union(group1, group2, newgroup));
    COMMGROUP_ERRCHECK(grpent1);
    COMMGROUP_ERRCHECK(grpent2);
    {
	int	maxsize = grpent1->size + grpent2->size;
	int	newsize = grpent1->size;
	uint32_t	*newranks = malloc(sizeof(uint32_t)*maxsize);
	uint32_t	*np;
	int	i, j;
	memcpy(newranks, grpent1->ranks, sizeof(int)*grpent1->size);
	np = newranks + grpent1->size;
	for (i = 0; i < grpent2->size; i++) {
	    for (j = 0; j < grpent1->size; j++) {
		if (grpent1->ranks[j] == grpent2->ranks[i]) goto cont;
	    }
	    *np++ = grpent2->ranks[i]; newsize++;
	cont: ;
	}
	grpinfo_reg(*newgroup, newsize, newranks);
    }
    return rc;
}

/*
 * intersection: all elements of the first group that are also in the second group,
 *		 ordered as in the first group
 */
int
MPI_Group_intersection(MPI_Group group1, MPI_Group group2, MPI_Group *newgroup)
{
    int rc;
    struct grpinfo_ent	*grpent1 = grpinfo_get(group1);
    struct grpinfo_ent	*grpent2 = grpinfo_get(group2);

    MPICALL_CHECK_RETURN(rc, PMPI_Group_intersection(group1, group2, newgroup));
    COMMGROUP_ERRCHECK(grpent1);
    COMMGROUP_ERRCHECK(grpent2);
    {
	int	maxsize = grpent1->size + grpent2->size;
	int	newsize = 0;
	uint32_t	*newranks = malloc(sizeof(uint32_t)*maxsize);
	uint32_t	*np;
	int	i, j;
	np = newranks;
	for (i = 0; i < grpent1->size; i++) {
	    for (j = 0; j < grpent2->size; j++) {
		if (grpent1->ranks[i] == grpent2->ranks[j]) {
		    *np++ = grpent1->ranks[i]; newsize++;
		    break;
		}
	    }
	}
	grpinfo_reg(*newgroup, newsize, newranks);
    }
    return rc;
}

/*
 * difference: all elements of the first group that are not in the second group,
 *	       ordered as in the first group.
 */
int
MPI_Group_difference(MPI_Group group1, MPI_Group group2, MPI_Group *newgroup)
{
    int rc;
    struct grpinfo_ent	*grpent1 = grpinfo_get(group1);
    struct grpinfo_ent	*grpent2 = grpinfo_get(group2);

    MPICALL_CHECK_RETURN(rc, PMPI_Group_difference(group1, group2, newgroup));
    COMMGROUP_ERRCHECK(grpent1);
    COMMGROUP_ERRCHECK(grpent2);
    {
	int	maxsize = grpent1->size + grpent2->size;
	int	newsize = 0;
	uint32_t	*newranks = malloc(sizeof(uint32_t)*maxsize);
	uint32_t	*np;
	int	i, j;
	np = newranks;
	for (i = 0; i < grpent1->size; i++) {
	    for (j = 0; j < grpent2->size; j++) {
		if (grpent1->ranks[i] == grpent2->ranks[j]) {
		    goto cont;
		}
	    }
	    *np++ = grpent1->ranks[i]; newsize++;
	cont: ;
	}
	grpinfo_reg(*newgroup, newsize, newranks);
    }
    return rc;
}

/*
 * This function creates a group newgroup that consists of the
 * n processes in group with ranks ranks[0],,, ranks[n-1]; the process with rank i in newgroup
 * is the process with rank ranks[i] in group. Each of the n elements of ranks must be a valid
 * rank in group and all elements must be distinct, or else the program is erroneous. If n = 0,
 * then newgroup is MPI_GROUP_EMPTY. This function can, for instance, be used to reorder
 * the elements of a group.
 */
int
MPI_Group_incl(MPI_Group group, int n, const int ranks[], MPI_Group *newgroup)
{
    int rc;

    MPICALL_CHECK_RETURN(rc, PMPI_Group_incl(group, n, ranks, newgroup));
    {
	struct grpinfo_ent	*grpent = grpinfo_get(group);
	uint32_t	*newranks = malloc(sizeof(uint32_t)*n);
	uint32_t	*np;
	int newsize = 0;
	int	i;
	for (i = 0, np = newranks; i < n; i++) {
	    assert(ranks[i] <= grpent->size);
	    *np++ = grpent->ranks[ranks[i]];
	    newsize++;
	}
	grpinfo_reg(*newgroup, newsize, newranks);
    }
    return rc;
}

/*
 * This function creates a group of processes newgroup that is obtained
 * by deleting from group those processes with ranks ranks[0], .., ranks[n-1]. The ordering of
 * processes in newgroup is identical to the ordering in group. Each of the n elements of ranks
 * must be a valid rank in group and all elements must be distinct; otherwise, the program is
 * erroneous. If n = 0, then newgroup is identical to group a valid rank in group
 */
int
MPI_Group_excl(MPI_Group group, int n, const int ranks[], MPI_Group *newgroup)
{
    int rc;

    MPICALL_CHECK_RETURN(rc, PMPI_Group_excl(group, n, ranks, newgroup));
    {
	struct grpinfo_ent	*ent = grpinfo_get(group);
	uint32_t	*newranks = malloc(sizeof(uint32_t)*ent->size);
	memcpy(newranks, ent->ranks, sizeof(int)*ent->size);
	if (n == 0) {
	    grpinfo_reg(*newgroup, ent->size, newranks);
	    // utfslist_append(&grpinfo_lst, &newent->slst);
	} else {
	    uint32_t	*cp;
	    int	i, newsize = 0;
	    for (i = 0; i < n; i++) {
		assert(ranks[i] <= ent->size);
		newranks[ranks[i]] = -1;
	    }
	    for (i = 0, cp = newranks; i < ent->size; i++) {
		if (newranks[i] != -1) {
		    *cp++ = newranks[i]; newsize++;
		}
	    }
	    grpinfo_reg(*newgroup, newsize, newranks);
	}
    }
    return rc;
}

int
MPI_Group_range_incl(MPI_Group group, int n, int ranges[][3], MPI_Group *newgroup)
{
    int rc;
    MPICALL_CHECK_RETURN(rc, PMPI_Group_range_incl(group, n, ranges, newgroup));
    NOTYET_SUPPORT;
    return rc;
}

int
MPI_Group_range_excl(MPI_Group group, int n, int ranges[][3], MPI_Group *newgroup)
{
    int rc;
    MPICALL_CHECK_RETURN(rc, PMPI_Group_range_excl(group, n, ranges, newgroup));
    NOTYET_SUPPORT;
    return rc;
}

int
MPI_Group_free(MPI_Group *group)
{
    int rc;
    rc = grpinfo_unreg(*group);
    assert(rc == 0);
    MPICALL_CHECK_RETURN(rc, PMPI_Group_free(group));
    return rc;
}

int
MPI_Comm_create(MPI_Comm comm, MPI_Group group, MPI_Comm *newcomm)
{
    int rc;

    MPICALL_CHECK_RETURN(rc, PMPI_Comm_create(comm, group, newcomm));
    {
	struct cominfo_ent	*coment;
	struct grpinfo_ent	*grpent, *newgrpent;

	if (*newcomm == MPI_COMM_NULL) {
	    /* nothing */ goto ext;
	}
	coment = cominfo_get(comm);
	grpent = grpinfo_get(group);
	assert(coment != NULL);	assert(grpent != NULL);
	newgrpent = grpinfo_dup(coment->grp, grpent->grp);
	/* We must check how we copy or share ? */
	cominfo_reg(*newcomm, newgrpent, (void*) coment->bgrp, 0);
	/* no need barrier synchronization in this case ? */
    }
ext:
    return rc;
}

int
MPI_Comm_dup(MPI_Comm comm, MPI_Comm *newcomm)
{
    int rc;

    MPICALL_CHECK_RETURN(rc, PMPI_Comm_dup(comm, newcomm));
    cominfo_dup(comm, *newcomm);
    return rc;
}

int
MPI_Comm_dup_with_info(MPI_Comm comm, MPI_Info info, MPI_Comm *newcomm)
{
    int rc;
    rc = PMPI_Comm_dup_with_info(comm, info, newcomm);
    cominfo_dup(comm, *newcomm);
    return rc;
}

int
MPI_Comm_split(MPI_Comm comm, int color, int key, MPI_Comm *newcomm)
{
    int rc;
    MPICALL_CHECK_RETURN(rc, PMPI_Comm_split(comm, color, key, newcomm));
    {
	struct inf1 {
	    int color; int key; int rank;
	} *inf1;
	struct inf2 {
	    int key; int rank;
	} *inf2, *ifp;
	int	i, myrank, sz, newsz;
	uint32_t	*grpidx;
	utfslist_entry_t *slst;
	struct grpinfo_ent	*grpent;
	utf_coll_group_t bg_grp;

	PMPI_Comm_size(comm, &sz);
	PMPI_Comm_rank(comm, &myrank);
	inf1 = malloc(sizeof(struct inf1)*sz);
	inf2 = malloc(sizeof(struct inf2)*sz);
	assert(inf1 != NULL); assert(inf2 != NULL);
	inf1[myrank].color = color;
	inf1[myrank].key = key;
	for (i = 0; i < sz; i++) {
	    inf1[i].rank = sz;
	}
	MPI_Allgather(&inf1[myrank], 3, MPI_INT, inf1, 3, MPI_INT, comm);
	/* selection */
	ifp = inf2; newsz = 0;
	for (i = 0; i < sz; i++) {
	    if (inf1[i].color == color) {
		ifp->key = inf1[i].key;
		ifp->rank = inf1[i].rank;
		ifp++; newsz++;
	    }
	}
	/* sort */
	qsort(inf2, newsz, sizeof(struct inf2), mycmp);
	/* */
	grpidx = malloc(sizeof(uint32_t)*newsz);
	for (i = 0; i < nprocs; i++) {
	    grpidx[i] = inf2[i].rank;
	}
	slst = utfslist_remove(&grpinfo_freelst);
	grpent = container_of(slst, struct grpinfo_ent, slst);
	grpent->grp = 0;
	grpent->size = nprocs;
	grpent->ranks = grpidx;
	UTFCALL_CHECK(err1, rc, utf_bg_init(grpidx, nprocs, myrank, mpi_bg_bginfo, &bg_grp));
	cominfo_reg(*newcomm, grpent, (void*) bg_grp, 0);
	free(inf1); free(inf2);
	MPICALL_CHECK(err2, rc, PMPI_Barrier(MPI_COMM_WORLD));
    }
    return rc;
err1:
    errprintf(myrank, "error on utf_bg_init %d\n", rc); goto ext;
err2:
    errprintf(myrank, "error on PMPI_Barrier %d\n", rc);
ext:
    return -1;
}

int
MPI_Comm_free(MPI_Comm *comm)
{
    int rc;
    utf_coll_group_t	grp;
    MPI_Comm	svd_comm = *comm;

    MPICALL_CHECK_RETURN(rc, PMPI_Comm_free(comm));
    grp = (utf_coll_group_t) cominfo_unreg(svd_comm);
    if (grp != NULL) {
	if (mpi_bg_confirm) {
	    utf_printf("%s: FREE grp(%p)\n", __func__, grp);
	}
	utf_bg_free(grp);
    }
    return rc;
}

int
MPI_Cart_create(MPI_Comm comm_old, int ndims, const int dims[], const int periods[],
		int reorder, MPI_Comm *comm_cart)
{
    int rc;
    MPICALL_CHECK_RETURN(rc, PMPI_Cart_create(comm_old, ndims, dims, periods, reorder, comm_cart));
    NOTYET_SUPPORT;
    return rc;
}

int
MPI_Graph_create(MPI_Comm comm_old, int nnodes, const int indx[], const int edges[],
		 int reorder, MPI_Comm *comm_graph)
{
    int rc;
    MPICALL_CHECK_RETURN(rc, PMPI_Graph_create(comm_old, nnodes, indx, edges, reorder, comm_graph));
    NOTYET_SUPPORT;
    return rc;
}

int
MPI_Cart_sub(MPI_Comm comm, const int remain_dims[], MPI_Comm *newcomm)
{
    int rc;
    MPICALL_CHECK_RETURN(rc, PMPI_Cart_sub(comm, remain_dims, newcomm));
    NOTYET_SUPPORT;
    return rc;
}

int
MPI_Dist_graph_create_adjacent(MPI_Comm comm_old, int indegree, const int sources[],
			       const int sourceweights[], int outdegree,
			       const int destinations[], const int destweights[],
			       MPI_Info info, int reorder, MPI_Comm *comm_dist_graph)
{
    int rc;
    MPICALL_CHECK_RETURN(rc, PMPI_Dist_graph_create_adjacent(comm_old, indegree, sources,
			sourceweights, outdegree, destinations, destweights,
			info, reorder, comm_dist_graph));
    NOTYET_SUPPORT;
    return rc;
}

int
MPI_Dist_graph_create(MPI_Comm comm_old, int n, const int sources[], const int degrees[],
		      const int destinations[], const int weights[], MPI_Info info,
		      int reorder, MPI_Comm *comm_dist_graph)
{
    int rc;
    MPICALL_CHECK_RETURN(rc, PMPI_Dist_graph_create(comm_old, n, sources, degrees,
			       destinations, weights, info,
					     reorder, comm_dist_graph));
    NOTYET_SUPPORT;
    return rc;
}

int
MPI_Comm_idup(MPI_Comm comm, MPI_Comm *newcomm, MPI_Request *request)
{
    int rc;
    MPICALL_CHECK_RETURN(rc, PMPI_Comm_idup(comm, newcomm, request));
    NOTYET_SUPPORT;
    return rc;
}
#endif /* !COMM_MPICHLEVEL_HANDLE */

#ifdef COMMGROUP_TEST
int
main(int argc, char **argv)
{
    MPI_Comm	newcomm;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_bg_nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_bg_myrank);
    if (myrank == 0) {
	printf("nprocs(%d)\n", mpi_bg_nprocs);
	dbg_noshow = 0;
    }
    MPI_Comm_dup(MPI_COMM_WORLD, &newcomm);
    if (myrank == 0) {
	printf("newcom=%x\n", newcomm);
    }
    MPI_Finalize();
    return 0;
}
#endif
