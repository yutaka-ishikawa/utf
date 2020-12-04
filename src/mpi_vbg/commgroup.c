/*
 * TODO:
 *	MPI_Group_incl: handling MPI_GROUP_EMPTY
 */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "utf_list.h"

#ifdef DEBUG
#define DBG if (mpi_bg_dbg)
#else
#define DBG if (0)
#endif

// typedef int MPI_Comm;
// typedef int MPI_Group;

#define GROUPINFO_SIZE	1024
struct grpinfo_ent {
    utfslist_entry_t	slst;
    MPI_Group		grp;
    int			size;
    uint32_t		*ranks;
};

struct cominfo_ent {
    utfslist_entry_t	slst;
    MPI_Comm		comm;
    struct grpinfo_ent	*grp;	/* this grpinfo entry should not be in the grpinfo_lst
				 * In other words, this grpinfo entry must be also reclaimed
				 * when this cominfo entry is reclaimed. */
    void		*bg_grp;
};

static struct utfslist		grpinfo_lst;
static struct utfslist		grpinfo_freelst;
static struct grpinfo_ent	*grpinfo_entarray;
static struct utfslist		cominfo_lst;
static struct utfslist		cominfo_freelst;
static struct grpinfo_ent	*cominfo_entarray;
static int			nprocs, myrank;
static int			dbg_noshow = 1;

static void
comgrpinfo_init(int entsize)
{
    int	i;

    /* group info */
    grpinfo_entarray = malloc(sizeof(struct grpinfo_ent)*entsize);
    memset(grpinfo_entarray, 0, sizeof(struct grpinfo_ent)*entsize);
    utfslist_init(&grpinfo_freelst, 0);
    for (i = 0; i < entsize; i++) {
	utfslist_append(&grpinfo_freelst, &grpinfo_entarray[i].slst);
    }
    /* comm info */
    cominfo_entarray = malloc(sizeof(struct cominfo_ent)*entsize);
    memset(cominfo_entarray, 0, sizeof(struct cominfo_ent)*entsize);
    utfslist_init(&cominfo_freelst, 0);
    for (i = 0; i < entsize; i++) {
	utfslist_append(&cominfo_freelst, &cominfo_entarray[i].slst);
    }
}

static struct grpinfo_ent *
grpinfo_reg(MPI_Group grp, int size, uint32_t *ranks)
{
    utfslist_entry_t *slst;
    struct grpinfo_ent	*ent;

    slst = utfslist_remove(&grpinfo_freelst);
    if (slst == NULL) {
	fprintf(stderr, "Too many MPI_Group created\n");
	abort();
    }
    ent = container_of(slst, struct grpinfo_ent, slst);
    ent->grp = grp;
    ent->size = size;
    ent->ranks = ranks;
    utfslist_append(&grpinfo_lst, &ent->slst);
    return ent;
}

static int
grpinfo_unreg(MPI_Group grp)
{
    utfslist_entry_t	*cur, *prev;
    struct grpinfo_ent *ent;
    utfslist_foreach2(&grpinfo_lst, cur, prev) {
	ent = container_of(cur, struct grpinfo_ent, slst);
	if (ent->grp == grp) goto find;
    }
    /* not found */
    return -1;
find:
    free(ent->ranks);
    ent->grp = 0;
    utfslist_remove2(&grpinfo_lst, cur, prev);
    utfslist_insert(&grpinfo_freelst, cur);
    return 0;
}

static struct grpinfo_ent *
grpinfo_get(MPI_Group grp)
{
    struct utfslist_entry	*cur;
    utfslist_foreach(&grpinfo_lst, cur) {
	struct grpinfo_ent	*ent = container_of(cur, struct grpinfo_ent, slst);
	if (ent->grp == grp) {
	    return ent;
	}
    }
    return NULL;
}

/*
 * copy except group ID
 */
static struct grpinfo_ent *
grpinfo_dup(struct grpinfo_ent *srcent, MPI_Group newgroup)
{
    utfslist_entry_t *slst;
    struct grpinfo_ent	*dupent;
    uint32_t	*grpidx;
    int	size = srcent->size;

    slst = utfslist_remove(&grpinfo_freelst);
    dupent = container_of(slst, struct grpinfo_ent, slst);
    grpidx = malloc(sizeof(int)*size);
    memcpy(grpidx, srcent->ranks, sizeof(int)*size);
    dupent->grp = newgroup;
    dupent->size = size;
    dupent->ranks = grpidx;
    return dupent;
}

static struct cominfo_ent *
cominfo_reg(MPI_Comm comm, struct grpinfo_ent *grpent, void *bg_grp)
{
    utfslist_entry_t *slst;
    struct cominfo_ent	*coment;

    slst = utfslist_remove(&cominfo_freelst);
    if (slst == NULL) {
	fprintf(stderr, "Too many MPI_Comm created\n");
	abort();
    }
    coment = container_of(slst, struct cominfo_ent, slst);
    coment->comm = comm;
    coment->grp = grpent;
    coment->bg_grp = bg_grp;
    utfslist_append(&cominfo_lst, &coment->slst);
    return coment;
}

static int
cominfo_unreg(MPI_Comm comm)
{
    utfslist_entry_t	*cur, *prev;
    struct cominfo_ent *ent;
    utfslist_foreach2(&cominfo_lst, cur, prev) {
	ent = container_of(cur, struct cominfo_ent, slst);
	if (ent->comm == comm) goto find;
    }
    /* not found */
    return -1;
find:
    free(ent->grp);
    ent->grp = 0;
    utfslist_remove2(&grpinfo_lst, cur, prev);
    utfslist_insert(&cominfo_freelst, cur);
    return 0;
}

static struct cominfo_ent *
cominfo_get(MPI_Comm comm)
{
    struct utfslist_entry	*cur;
    utfslist_foreach(&cominfo_lst, cur) {
	struct cominfo_ent	*ent = container_of(cur, struct cominfo_ent, slst);
	if (ent->comm == comm) {
	    return ent;
	}
    }
    return NULL;
}

static void
comm_show(FILE *fp, const char *fname, MPI_Comm comm)
{
    struct cominfo_ent	*ent = cominfo_get(comm);

    if (dbg_noshow) return;
    if (ent == NULL) {
	fprintf(stderr, "%s: Communicator(%x) is not found\n", fname, comm);
    } else {
	int	i = 0;
	uint32_t	*ranks = ent->grp->ranks;
	int	size = ent->grp->size;
	fprintf(stderr, "%s: Communicator(%x)\n", fname, comm);
	while (i < size) {
	    int j;
	    for (j = 0; j < 20 && i < size; j++, i++) {
		fprintf(stderr, "%05d ", *ranks++);
	    }
	}
	fprintf(stderr, "\n");
    }
}

static void
cominfo_dup(MPI_Comm comm, MPI_Comm newcomm)
{
    struct cominfo_ent	*coment = cominfo_get(comm);
    struct grpinfo_ent	*grpent;
    if (coment == NULL) {
	fprintf(stderr, "%s: Communicator(%x) is not found\n", __func__, comm);
	goto ext;
    }
    grpent = grpinfo_dup(coment->grp, 0);
    cominfo_reg(newcomm, grpent, 0);
    DBG {
	comm_show(stderr, __func__, comm);
	comm_show(stderr, __func__, newcomm);
    }
ext:
    return;
}


static int
mycmp(const void *arg1, const void *arg2)
{
    struct inf2 { int key; int rank; };
    int rc;

    rc = ((struct inf2*)arg1)->key < ((struct inf2*)arg2)->key;
    return rc;
}
