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

#define COMINFO_SIZE	1024
struct cominfo_ent {
    utfslist_entry_t	slst;
    MPI_Comm		comm;
    int			nprocs;
    int			myrank;
    int			refcnt;
    uint32_t		*rankset;
    void		*bgrp;
    struct cominfo_ent	*parent;
};
static struct utfslist		cominfo_lst;
static struct utfslist		cominfo_freelst;
static struct cominfo_ent	*cominfo_entarray;

static void
cominfo_init(int entsize)
{
    static int	nfirst = 0;
    int	i;
    if (nfirst) {
	return;
    }
    nfirst = 1;
    /* comm info */
    cominfo_entarray = malloc(sizeof(struct cominfo_ent)*entsize);
    memset(cominfo_entarray, 0, sizeof(struct cominfo_ent)*entsize);
    utfslist_init(&cominfo_lst, 0);
    utfslist_init(&cominfo_freelst, 0);
    for (i = 0; i < entsize; i++) {
	utfslist_append(&cominfo_freelst, &cominfo_entarray[i].slst);
    }
}

static struct cominfo_ent *
cominfo_reg(MPI_Comm comm, void *bg_grp, uint32_t *rankset, struct cominfo_ent *parent)
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
    MPI_Comm_size(comm, &coment->nprocs);
    MPI_Comm_rank(comm, &coment->myrank);
    coment->rankset = rankset;
    coment->bgrp = bg_grp;
    coment->parent = parent;
    coment->refcnt = 1;
    if (parent) {
	parent->refcnt++;
    }
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
    --ent->refcnt;
    if (ent->refcnt > 0) {
	goto ext;
    }
    if (ent->parent) {
	cominfo_unreg(ent->parent->comm);
    }
    free(ent->rankset);
    free(ent->bgrp);
    ent->comm = 0;
    ent->rankset = 0;
    ent->bgrp = 0;
    utfslist_remove2(&cominfo_lst, cur, prev);
    utfslist_insert(&cominfo_freelst, cur);
ext:
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
