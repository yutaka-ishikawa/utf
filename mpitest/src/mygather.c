/*
 * Intention is testing the UTF chain mode.
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
    int	tag = 1000;

    if (myrank == 0) {
	int	i;
	MPI_Status stat;
	for (i = 1; i < nprocs; i++) {
	    VERBOSE("%dth  ****** Issue MPI_Recv\n", th);
	    MPI_Recv(rbuf, length, MPI_BYTE, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &stat);
	    VERBOSE("%d\n", stat.MPI_SOURCE);
	}
	VERBOSE("%dth  ****** MPI_Recv done\n", th);
    } else {
	int	dst = 0;
	VERBOSE("%dth  ****** Issue MPI_Send %d\n", th, dst);
	MPI_Send(sbuf, length, MPI_BYTE, dst, tag, MPI_COMM_WORLD);
	VERBOSE("%dth  ****** MPI_Send done\n", th);
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
    // printf(">%d : Going-to-BARRIER\n", myrank);  fflush(stdout);
    MPI_Barrier(MPI_COMM_WORLD);
    if (myrank == 0) {
	printf("DONE:\n"); fflush(stdout);
    }
ext:
    MPI_Finalize();
    return 0;
}
