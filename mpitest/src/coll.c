/*
 * This is a test for the dev1 branch
 */
#include <mpi.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "testlib.h"
#include <utf.h>

#define LEN_INIT	1
int	*sendbuf;
int	*recvbuf;

#define MYPRINT	if (myrank == 0)

int
verify_reduce(int *sendbuf, int *recvbuf, int length)
{
    int errs = 0;
    int	j, k;
    for (k = 0; k < length; k++) {
	int	val = 0;
	for (j = 0; j < nprocs; j++) {
	    val += (j + k + 1);
	}
	if (recvbuf[k] != val) {
	    printf("sendbuf[%d] = %d, expect = %d\n", k, recvbuf[k], val);
	    errs++;
	}
    }
    return errs;
}


int
main(int argc, char** argv)
{
    int	tsz;
    int	errs = 0;
    size_t	i, sz;

    length = LEN_INIT;
    iteration = 1;
    sflag = 0x10;	// Alltoall in default
    test_init(argc, argv);

    if (vflag) {
	utf_vname_show(stdout);
    }
    MPI_Type_size(MPI_INT, &tsz);
    sz = length*nprocs*tsz;
    sendbuf = malloc(sz);
    recvbuf = malloc(sz);
    MYPRINT {
	printf("sendbuf=%p recvbuf=%p\n"
	       "MPI_INT SIZE: %d\n"
	       "length(%ld) byte(%ld) nprocs(%d) iteration(%d) sflag(0x%x)\n",
	       sendbuf, recvbuf, tsz, length, sz, nprocs, iteration, sflag); fflush(stdout);
    }
    if (sendbuf == NULL || recvbuf == NULL) {
	MYPRINT {printf("Cannot allocate buffers: sz=%ldMiB * 2\n", (uint64_t)(((double)sz)/(1024.0*1024.0))); }
	exit(-1);
    }
    for (i = 0; i < length*nprocs; i++) {
	sendbuf[i] = myrank + i + 1;
	recvbuf[i] = -1;
    }
    if (sflag & 0x1) {
	for (i = 0; i < iteration; i++) {
	    MYPRINT { VERBOSE("Start MPI_Barier %ldth\n", i); }
	    MPI_Barrier(MPI_COMM_WORLD);
	    MYPRINT { VERBOSE("End of MPI_Barrier %ld\n", i); }
	}
    }
    if (sflag & 0x2) {
	for (i = 0; i < iteration; i++) {
	    int	j;
	    MYPRINT { VERBOSE("Start MPI_Reduce %ldth\n", i); }
	    MPI_Reduce(sendbuf, recvbuf, length, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
	    MYPRINT { VERBOSE("End of MPI_Reduce %ld\n", i); }
	    if (myrank == 0) {
		errs += verify_reduce(sendbuf, recvbuf, length);
	    }
	    /* reset value */
	    for (j = 0; j < length*nprocs; i++) {
		sendbuf[j] = myrank + j + 1;
		recvbuf[j] = -1;
	    }
	}
	if (errs) {
	    printf("[%d] MPI_Reduce: Errors = %d\n", myrank, errs);
	} else {
	    MYPRINT { printf("MPI_Reduce: No errors\n"); }
	}
    }
    if (sflag & 0x4) {
	for (i = 0; i < iteration; i++) {
	    int	j;
	    MYPRINT { VERBOSE("Start MPI_Allreduce %ldth\n", i); }
	    // MPI_Allreduce(MPI_IN_PLACE, sendbuf, length, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
	    MPI_Allreduce(sendbuf, recvbuf, length, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
	    MYPRINT { VERBOSE("End of MPI_Allreduce %ld\n", i); }
	    errs += verify_reduce(sendbuf, recvbuf, length);
	    /* reset value */
	    for (j = 0; j < length*nprocs; j++) {
		sendbuf[j] = myrank + j + 1;
		recvbuf[j] = -1;
	    }
	}
	if (errs) {
	    printf("[%d] MPI_Allreduce: Errors = %d\n", myrank, errs);
	} else {
	    MYPRINT { printf("MPI_Allreduce: No errors\n"); }
	}
    }
    if (sflag & 0x8) {
	for (i = 0; i < iteration; i++) {
	    MYPRINT { VERBOSE("Start MPI_Gather %ldth\n", i); }
	    MPI_Gather(sendbuf, length, MPI_INT, recvbuf, length, MPI_INT, 0, MPI_COMM_WORLD);
	    MYPRINT { VERBOSE("End of MPI_Gather %ldth\n", i); }
	}
    }
    if (sflag & 0x10) {
	for (i = 0; i < iteration; i++) {
	    VERBOSE("Start of Alltoall %ldth\n", i);
	    MPI_Alltoall(sendbuf, length, MPI_INT, recvbuf, length, MPI_INT, MPI_COMM_WORLD);
	    VERBOSE("End of Alltoall %ldth\n", i);
	}
    }
    if (sflag & 0x20) {
	for (i = 0; i < iteration; i++) {
	    MYPRINT { VERBOSE("Start MPI_Scatter %ldth\n", i); }
	    MPI_Scatter(sendbuf, length, MPI_INT, recvbuf, length, MPI_INT, 0, MPI_COMM_WORLD);
	    MYPRINT { VERBOSE("End of MPI_Scatter %ldth\n", i); }
	}
    }

    MPI_Finalize();
    MYPRINT {
	printf("RESULT(0x%x) coll: PASS\n", sflag); fflush(stdout);
    }
    return 0;
}
