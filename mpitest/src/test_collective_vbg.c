/*
 *	0x01:	Barrier
 *	0x02:	Bcast
 *	0x04:	Reduce
 *	0x08:	AllReduce
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <mpi.h>
#include "testlib.h"

int		sbuf[128], rbuf[128];

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


#define MPICALL_CHECK(lbl, rc, func)			\
{							\
    rc = func;						\
    if (rc != MPI_SUCCESS) goto lbl;			\
}

int
testBarrier(MPI_Comm comm)
{
    int rc = 0;
    int	i;
    if (myrank == 0) VERBOSE("Start MPI_Barrier using utf_barrier\n");
    for (i = 0; i < iteration; i++) {
	MPICALL_CHECK(err0, rc,  MPI_Barrier(comm));
    }
    myprintf(myrank, "Success: MPI_Barrier\n");
ext:
    return rc;
err0:
    myprintf(0, "ERROR: rank(%d) MPI_Barrier rc=%d\n", myrank, rc);
    goto ext;
}

int
testBcast(MPI_Comm comm, int comm_rank, int count)
{
    int rc = 0, errs, rslt;
    int	i;
    if (comm_rank == 0) VERBOSE("Start MPI_Bcastr using utf_barrier\n");
    for (i = 0; i < 12; i++) {
	sbuf[i] = comm_rank + i;
    }
    for (i = 0; i < iteration; i++) {
	MPICALL_CHECK(err0, rc, MPI_Bcast(sbuf, count, MPI_INT, 0, comm));
    }
    if (comm_rank == 0) VERBOSE("Verifying\n");
    errs = 0;
    if (comm_rank != 0) {
	for (i = 0; i < 12; i++) {
	    if (sbuf[i] != i) {
		myprintf(0, "rank(%d): sbuf[%d] Expect %d, but %d\n", comm_rank, i, i, sbuf[i]);
		errs++;
	    }
	}
    }
    PMPI_Reduce(&errs, &rslt, 1, MPI_INT, MPI_SUM, 0, comm);
    if (comm_rank == 0) {
	if (rslt) {
	    myprintf(comm_rank, "Fail: MPI_Bcast\n");
	    rc = -1;
	} else {
	    myprintf(comm_rank, "Success: MPI_Bcast\n");
	}
    }
ext:
    return rc;
err0:
    myprintf(0, "ERROR: rank(%d) MPI_Bcast rc=%d\n", comm_rank, rc);
    goto ext;
}

int
testReduce(MPI_Comm comm, int comm_rank, int comm_nprocs, int count)
{
    int rc = 0, errs;
    int	i;
    for (i = 0; i < count; i++) {
	sbuf[i] = comm_rank + i;
    }
    if (comm_rank == 0) VERBOSE("Start MPI_Reduce using utf_reduce\n");
    for (i = 0; i < iteration; i++) {
	MPICALL_CHECK(err0, rc,  MPI_Reduce(sbuf, rbuf, 6, MPI_INT, MPI_SUM, 0, comm));
    }
    if (comm_rank == 0) VERBOSE("Verifying\n");
    errs = 0;
    if (comm_rank == 0) {
	int	expval = 0;
	for(i = 0; i < comm_nprocs; i++) {
	    expval += i;
	}
	for (i = 0; i < count; i++) {
	    if (rbuf[i] != expval) {
		myprintf(comm_rank, "rbuf[%d] Expect %d, but %d\n", i, rbuf[i], expval + i);
		errs++;
	    }
	    expval += comm_nprocs;
	}
	if (errs) {
	    myprintf(comm_rank, "Fail: MPI_Reduce\n");
	    rc = -1;
	} else {
	    myprintf(comm_rank, "Success: MPI_Reduce\n");
	}
    }
ext:
    return rc;
err0:
    myprintf(0, "ERROR: rank(%d) MPI_Reduce rc=%d\n", comm_rank, rc);
    goto ext;
}

int
testAllreduce(MPI_Comm comm, int comm_rank, int comm_nprocs, int count)
{
    int rc = 0, errs, rslt;
    int	i;
    for (i = 0; i < count; i++) {
	sbuf[i] = comm_rank + i;
    }
    if (comm_rank == 0) VERBOSE("Start MPI_Allreduce using utf_reduce\n");
    for (i = 0; i < iteration; i++) {
	MPICALL_CHECK(err0, rc,  MPI_Allreduce(sbuf, rbuf, 6, MPI_INT, MPI_SUM, comm));
    }
    if (comm_rank == 0) VERBOSE("Verifying\n");
    errs = 0;
    {
	int	expval = 0;
	for(i = 0; i < comm_nprocs; i++) {
	    expval += i;
	}
	for (i = 0; i < count; i++) {
	    if (rbuf[i] != expval) {
		myprintf(comm_rank, "rbuf[%d] Expect %d, but %d\n", i, rbuf[i], expval + i);
		errs++;
	    }
	    expval += comm_nprocs;
	}
    }
    PMPI_Reduce(&errs, &rslt, 1, MPI_INT, MPI_SUM, 0, comm);
    if (comm_rank == 0) {
	if (rslt) {
	    myprintf(comm_rank, "Fail: MPI_Allreduce\n");
	    rc = -1;
	} else {
	    myprintf(comm_rank, "Success: MPI_Allreduce\n");
	}
    }
ext:
    return rc;
err0:
    myprintf(0, "ERROR: rank(%d) MPI_Allreduce rc=%d\n", comm_rank, rc);
    goto ext;
}

int
testAll(MPI_Comm comm, int comm_rank, int comm_nprocs,
	int cnt_bcast, int cnt_reduce, int cnt_allreduce)
{
    int	rc;
    if (sflag & 0x01) {	/* MPI_Barrier */
	MPICALL_CHECK(ext, rc, testBarrier(comm));
    }
    if (sflag & 0x02) {	/* MPI_Bcast (only 48 byte) count = 12, MPI_INT*/
	MPICALL_CHECK(ext, rc, testBcast(comm, comm_rank, cnt_bcast));
    }
    if (sflag & 0x04) {	/* MPI_Reduce 6 entries */
	MPICALL_CHECK(ext, rc, testReduce(comm, comm_rank, comm_nprocs, cnt_reduce));
    }
    if (sflag & 0x08) {	/* MPI_Allreduce 6 entries */
	MPICALL_CHECK(ext, rc, testAllreduce(comm, comm_rank, comm_nprocs, cnt_allreduce));
    }
ext:
    return rc;
}

int 
main(int argc, char *argv[])
{
    int		rc = 0;

    sflag = 0xff;
    iteration = 10; /* default iteration */
    test_init(argc, argv);
    myprintf(myrank, "nprocs = %d\n", nprocs);

    /* Testing COMM_WORLD */
    myprintf(myrank, "*** MPI_COMM_WORLD ***\n");
    MPICALL_CHECK(ext, rc, testAll(MPI_COMM_WORLD, myrank, nprocs, 12, 6, 6));
    /* Testing User-defined communicator */
    {
	MPI_Comm	newcomm;
	int	color  = myrank % 2;
	int	key    = myrank / 2;
	int	new_nprocs, new_myrank;
	MPICALL_CHECK(ext, rc, MPI_Comm_split(MPI_COMM_WORLD, color, key, &newcomm));
	MPICALL_CHECK(ext, rc, MPI_Comm_size(newcomm, &new_nprocs));
	MPICALL_CHECK(ext, rc, MPI_Comm_rank(newcomm, &new_myrank));
	// myprintf(0, "[%d] color(%d) key(%d) new nprocs(%d) new myrank(%d)\n", myrank, color, key, new_nprocs, new_myrank);
	myprintf(myrank, "\n*** User-defined Communicator ***\n");
	if (color == 0) {
	    myprintf(new_myrank, "--- Color=0 Start (grank=%d) --\n", myrank);
	    MPICALL_CHECK(ext, rc, testAll(newcomm, new_myrank, new_nprocs, 12, 6, 6));
	    myprintf(new_myrank, "--- Color=0 Done (grank=%d) --\n", myrank);
	}
	MPICALL_CHECK(ext, rc, MPI_Barrier(MPI_COMM_WORLD));
	if (color == 1) {
	    myprintf(new_myrank, "--- Color=1 Start (grank=%d) --\n", myrank);
	    MPICALL_CHECK(ext, rc, testAll(newcomm, new_myrank, new_nprocs, 12, 6, 6));
	    myprintf(new_myrank, "--- Color=1 Done (grank=%d) --\n", myrank);
	}
	myprintf(myrank, "\t--- All Barrier Start --\n");
	MPICALL_CHECK(ext, rc, MPI_Barrier(MPI_COMM_WORLD));
	myprintf(myrank, "\t--- All Barrier Done --\n");
	myprintf(myrank, "\t--- Calling MPI_Comm_free --\n");
	MPICALL_CHECK(ext, rc, MPI_Comm_free(&newcomm));
    }
    myprintf(myrank, "DONE\n");
ext:
    MPI_Finalize();
    return rc;
}
