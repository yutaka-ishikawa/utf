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


#define MPICALL_CHECK(lbl, rc, func)			\
{							\
    rc = func;						\
    if (rc != MPI_SUCCESS) goto lbl;			\
}


int 
main(int argc, char *argv[])
{
    int		sbuf[128], rbuf[128];
    int		i, errs, rslt, rc = 0;

    sflag = 0xff;
    test_init(argc, argv);
    myprintf(myrank, "nprocs = %d\n", nprocs);

    if (sflag & 0x01) {
	myprintf(myrank, "Start utf_barrier\n");
	/* MPI_Barrier */
	for (i = 0; i < 10; i++) {
	    MPICALL_CHECK(err0, rc,  MPI_Barrier(MPI_COMM_WORLD));
	}
	myprintf(myrank, "Success: End of utf_barrier\n");
    }

    if (sflag & 0x02) {
	/* MPI_Bcast (only 48 byte) */
	for (i = 0; i < 12; i++) {
	    sbuf[i] = myrank + i;
	}
	myprintf(myrank, "Start utf_broadcast\n");
	rc = MPI_Bcast(sbuf, 12, MPI_INT, 0, MPI_COMM_WORLD);
	if (rc != MPI_SUCCESS) {
	    myprintf(0, "rank(%d) rc = %d\n", myrank, rc);
	}
	myprintf(myrank, "Verifying\n");
	errs = 0;
	if (myrank != 0) {
	    for (i = 0; i < 12; i++) {
		if (sbuf[i] != i) {
		    myprintf(0, "rank(%d): sbuf[%d] Expect %d, but %d\n", myrank, i, i, sbuf[i]);
		    errs++;
		}
	    }
	}
	PMPI_Reduce(&errs, &rslt, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
	if (myrank == 0) {
	    if (rslt) {
		myprintf(myrank, "Fail: End of utf_broadcast\n", myrank);
	    } else {
		myprintf(myrank, "Success: End of utf_broadcast\n", myrank);
	    }
	}
    }

    if (sflag & 0x04) {
	/* MPI_Reduce 6 entries */
	for (i = 0; i < 6; i++) {
	    sbuf[i] = myrank + i;
	}
	myprintf(myrank, "Start utf_reduce\n");
	for (i = 0; i < 2; i++) {
	    MPICALL_CHECK(err1, rc,  MPI_Reduce(sbuf, rbuf, 6, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD));
	}
	myprintf(myrank, "Verifying\n");
	errs = 0;
	if (myrank == 0) {
	    int	expval = 0;
	    for(i = 0; i < nprocs; i++) {
		expval += i;
	    }
	    for (i = 0; i < 6; i++) {
		if (rbuf[i] != expval) {
		    myprintf(myrank, "sbuf[%d] Expect %d, but %d\n", i, rbuf[i], expval + i);
		    errs++;
		}
		expval += nprocs;
	    }
	    if (errs) {
		myprintf(myrank, "Fail: End of utf_reduce\n", myrank);
	    } else {
		myprintf(myrank, "Success: End of utf_reduce\n", myrank);
	    }
	}
    }

    if (sflag & 0x08) {
	/* MPI_Allreduce 6 entries */
	for (i = 0; i < 6; i++) {
	    sbuf[i] = myrank + i;
	}
	myprintf(myrank, "Start utf_allreduce\n");
	for (i = 0; i < 2; i++) {
	    MPICALL_CHECK(err1, rc,  MPI_Allreduce(sbuf, rbuf, 6, MPI_INT, MPI_SUM, MPI_COMM_WORLD));
	}
	myprintf(myrank, "Verifying\n");
	errs = 0;
	{
	    int	expval = 0;
	    for(i = 0; i < nprocs; i++) {
		expval += i;
	    }
	    for (i = 0; i < 6; i++) {
		if (rbuf[i] != expval) {
		    myprintf(myrank, "sbuf[%d] Expect %d, but %d\n", i, rbuf[i], expval + i);
		    errs++;
		}
		expval += nprocs;
	    }
	}
	PMPI_Reduce(&errs, &rslt, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
	if (myrank == 0) {
	    if (rslt) {
		myprintf(myrank, "Fail: End of utf_allreduce\n", myrank);
	    } else {
		myprintf(myrank, "Success: End of utf_allreduce\n", myrank);
	    }
	}
    }
    

ext:
    MPI_Finalize();
    return rc;

err0:
    errprintf(myrank, "MPI_Barrier error i = %d rc = %d\n", i, rc);
    goto ext;
err1:
    errprintf(myrank, "MPI_Reduce error i = %d rc = %d\n", i, rc);
    goto ext;
}
