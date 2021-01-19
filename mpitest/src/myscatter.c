/*
 * This is a test for the dev1 branch
 *
 */
#include <mpi.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "testlib.h"
#ifndef FJMPI
#include <utf.h>
#endif
#include <utf_tsc.h>

#define LEN_INIT	1
char	*sbuf;
char	*rbuf;

void
run(int th)
{
    int	src = 0;
    int	tag = 1000;

    if (myrank == 0) {
	int	dst;
	for (dst = 1; dst < nprocs; dst++) {
	    VERBOSE("%dth  ****** Issue MPI_Send %d\n", th, dst);
	    MPI_Send(sbuf, length, MPI_BYTE, dst, tag, MPI_COMM_WORLD);
	    VERBOSE("%dth  ****** MPI_Send done\n", th);
	}
	VERBOSE("%dth  ****** send done\n", th);
	dst = 3;
	VERBOSE("%dth  !!!!!! Issue MPI_Send %d\n", th, dst);
	MPI_Send(sbuf, length, MPI_BYTE, dst, tag, MPI_COMM_WORLD);
    } else {
	MPI_Status stat;
	VERBOSE("%dth  ****** Issue MPI_Recv\n", th);
	MPI_Recv(rbuf, length, MPI_BYTE, src, tag, MPI_COMM_WORLD, &stat);
	VERBOSE("%dth  ****** MPI_Recv done\n", th);
	if (myrank == 3) {
	    VERBOSE("%dth  !!!!!! Issue MPI_Recv\n", th);
	    MPI_Recv(rbuf, length, MPI_BYTE, src, tag, MPI_COMM_WORLD, &stat);
	}
    }
}

int
main(int argc, char** argv)
{
    int	iter;
    length = LEN_INIT;
    iteration = 1;
    test_init(argc, argv);

    sbuf = malloc(length);
    rbuf = malloc(length);
    if (sbuf == NULL || rbuf == NULL) {
	fprintf(stderr, "Cannot allocate buffer\n"); fflush(stdout);
	goto ext;
    }
    if (myrank == 0) {
	printf("myscatter: nprocs(%d) length(%ld) iteration(%d)\n", nprocs, length, iteration); fflush(stdout);
    }
    for (iter = 0; iter < iteration; iter++) {
	run(iter);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    if (myrank == 0) {
	printf("DONE:\n"); fflush(stdout);
    }
ext:
    MPI_Finalize();
    return 0;
}
