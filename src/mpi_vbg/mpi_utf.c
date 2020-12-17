#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <mpi.h>
#include <utf.h>
#include <utofu.h>
#include <jtofu.h>
#include <utf_bg.h>

#define UTF_BG_MIN_BARRIER_SIZE	4
static int	mpi_bg_enabled = 0;
static int	mpi_bg_dbg = 0;
static int	mpi_bg_confirm = 0;
static utf_coll_group_t mpi_world_grp;
static utf_bg_info_t	*mpi_bg_bginfo;
static int	mpi_bg_nprocs, mpi_bg_myrank;
static int	mpi_init_nfirst = 0;

#include "commgroup.c"

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
    cp = getenv("UTF_BG_CONFIRM");
    if (cp && atoi(cp) != 0) {
	mpi_bg_confirm = 1;
    }
}

static inline int
mpi_bg_init()
{
    int rc, i;
    int		nprocs, myrank;
    uint32_t	*grpidx;
    utf_bg_info_t	*lcl_bginfo, *rmt_bginfo;

    PMPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    PMPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    grpidx = malloc(sizeof(uint32_t)*nprocs);
    lcl_bginfo = malloc(sizeof(utf_bg_info_t) * nprocs);
    rmt_bginfo = malloc(sizeof(utf_bg_info_t) * nprocs);
    if (grpidx == NULL || lcl_bginfo == NULL || rmt_bginfo == NULL) {
	errprintf(myrank, "Cannot allocate working memory for VBG procs(%d)\n", nprocs);
	goto ext;
    }
    /* Group info */
    for (i = 0; i < nprocs; i++) {
	grpidx[i] = i;
    }
    UTFCALL_CHECK(err1, rc, utf_bg_alloc(grpidx, nprocs, myrank,
					 jtofu_query_max_proc_per_node(),
					 UTF_BG_TOFU, lcl_bginfo));
    MPICALL_CHECK(err2, rc, PMPI_Alltoall(lcl_bginfo, sizeof(utf_bg_info_t), MPI_BYTE,        
					  rmt_bginfo, sizeof(utf_bg_info_t), MPI_BYTE, MPI_COMM_WORLD));
    free(lcl_bginfo);
    mpi_bg_bginfo = rmt_bginfo; mpi_bg_nprocs = nprocs; mpi_bg_myrank = myrank;
    /* Communicator related initialization */
    comgrpinfo_init(GROUPINFO_SIZE);
    {
	utfslist_entry_t *slst;
	struct grpinfo_ent	*grpent;

	/* setting up for MPI_COMM_WORLD */
	rc = utf_bg_init(grpidx, nprocs, myrank, mpi_bg_bginfo, &mpi_world_grp);
	slst = utfslist_remove(&grpinfo_freelst);
	grpent = container_of(slst, struct grpinfo_ent, slst);
	grpent->grp = 0;
	grpent->size = nprocs;
	grpent->ranks = grpidx;
	cominfo_reg(MPI_COMM_WORLD, grpent, (void*) mpi_world_grp);
    }
    mpi_bg_enabled = 1;
    return MPI_SUCCESS;
    /* error handling */
err1:
    errprintf(myrank, "error on utf_bg_aloc(), rc = %d\n", rc); goto ext;
err2:
    errprintf(myrank, "error on PMI_Alltoall(), rc = %d\n", rc);
ext:
    free(grpidx); free(lcl_bginfo); free(rmt_bginfo);
    return -1;
}

int
MPI_Init(int *argc, char ***argv)
{
    int	rc;

    if (mpi_init_nfirst > 0)  {
	/* Already called, and thus no need to initialize VBG */
	return PMPI_Init(argc, argv);
    }
    mpi_init_nfirst++;
    option_get();
    MPICALL_CHECK(err, rc, PMPI_Init(argc, argv));
    DBG {
	myprintf(0, "%s: 1\n", __func__);
    }
    MPICALL_CHECK(err, rc, mpi_bg_init());
    DBG {
	myprintf(0, "[%d] %s: 2\n", mpi_bg_myrank, __func__);
    }
    MPICALL_CHECK(err, rc, PMPI_Barrier(MPI_COMM_WORLD));
    DBG {
	myprintf(0, "[%d] %s: 3 return\n", mpi_bg_myrank, __func__);
    }
    if (mpi_bg_confirm) {
	myprintf(0, "[%d] *** UTF VBG is enabled **\n", mpi_bg_myrank);
    }
err:
    return rc;
}

int
MPI_Init_thread(int *argc, char ***argv, int required, int *provided)
{
    int	rc;

    if (mpi_init_nfirst > 0)  {
	/* Already called, and thus no need to initialize VBG */
	return PMPI_Init_thread(argc, argv, required, provided);
    }
    mpi_init_nfirst++;
    option_get();
    MPICALL_CHECK(err, rc, PMPI_Init_thread(argc, argv, required, provided));
    rc = mpi_bg_init();
err:
    return rc;
}

int
MPI_Barrier(MPI_Comm comm)
{
    int	rc = 0;
    struct cominfo_ent *ent;

    IF_BG_DISABLE(PMPI_Barrier(comm));
    COMINFO_GET_CHECK_ABORT(ent, comm);
    assert(ent->grp != NULL);
    if (ent->grp->size < UTF_BG_MIN_BARRIER_SIZE) {
	MPICALL_CHECK_RETURN(rc, PMPI_Barrier(comm));
    } else {
	UTFCALL_CHECK(err, rc,  utf_barrier(ent->bg_grp));
	while (utf_poll_barrier(ent->bg_grp) == UTF_ERR_NOT_COMPLETED) { }
    }
err:
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
    case MPI_PROD:
    case MPI_LAND:
    case MPI_LOR:
    case MPI_LXOR:
    case MPI_MINLOC:
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
    COMINFO_GET_CHECK_ABORT(ent, comm);

    rc = MPI_Type_size(datatype, &siz);
    tsize = siz * count;
    /* UTF_BG_REDUCE_ULMT_ELMS_48 48 */
    rc = utf_broadcast(ent->bg_grp, buf, tsize, NULL, root);
    if (rc == UTF_SUCCESS) {
	double	data;

	while (utf_poll_reduce(ent->bg_grp, (void **)&data) == UTF_ERR_NOT_COMPLETED) { }
    } else if (rc == UTF_ERR_NOT_AVAILABLE) {
	//myprintf(0, "[%d] %s: calling PMPI_Bcast\n", mpi_bg_myrank, __func__);
	rc = PMPI_Bcast(buf, count, datatype, root, comm);
    }
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
    utf_type = mpitype_to_utf(datatype);
    utf_op = mpiop_to_utf(op);
    if (utf_type != 0 && utf_op != 0) {
	double	data;
	COMINFO_GET_CHECK_ABORT(ent, comm);
	rc = utf_reduce(ent->bg_grp, sbuf, count, NULL, rbuf, NULL, utf_type, utf_op, root);
	if (rc == UTF_ERR_NOT_AVAILABLE) {
	    goto pmpi_call;
	} else if (rc != UTF_SUCCESS) {
	    myprintf(0, "[%d] %s: rc = %d\n", mpi_bg_myrank, __func__, rc);
	    goto pmpi_call;
	}
	/* progress */
	while (utf_poll_reduce(ent->bg_grp, (void **)&data) == UTF_ERR_NOT_COMPLETED) { }
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
    utf_type = mpitype_to_utf(datatype);
    utf_op = mpiop_to_utf(op);
    if (utf_type != 0 && utf_op != 0) {
	double	data;
	COMINFO_GET_CHECK_ABORT(ent, comm);
	rc = utf_allreduce(ent->bg_grp, sbuf, count, NULL, rbuf, NULL, utf_type, utf_op);
	if (rc == UTF_ERR_NOT_AVAILABLE) {
	    goto pmpi_call;
	} else if (rc != UTF_SUCCESS) {
	    myprintf(0, "[%d] %s: rc = %d\n", mpi_bg_myrank, __func__, rc);
	    goto pmpi_call;
	}
	/* progress */
	while (utf_poll_reduce(ent->bg_grp, (void **)&data) == UTF_ERR_NOT_COMPLETED) { }
	goto ext;
    } else {
    pmpi_call:
	//myprintf(0, "[%d] %s: calling PMPI_Allreduce\n", mpi_bg_myrank, __func__);
	rc = PMPI_Allreduce(sbuf, rbuf, count, datatype, op, comm);
    }
ext:
    return rc;
}

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
	cominfo_reg(*newcomm, newgrpent, (void*) coment->bg_grp);
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
	cominfo_reg(*newcomm, grpent, (void*) bg_grp);
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
    MPICALL_CHECK_RETURN(rc, PMPI_Comm_free(comm));
    cominfo_unreg(*comm);
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
