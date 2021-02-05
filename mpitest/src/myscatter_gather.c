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
scatter_run(int th)
{
    int	src = th % nprocs;
    int	tag = 10 + th;

    if (myrank == src) {
	int	dst;
	for (dst = 0; dst < nprocs; dst++) {
	    if (dst == src) continue;
	    VERBOSE("%dth  S****** Issue MPI_Send to %d\n", th, dst);
	    MPI_Send(sbuf, length, MPI_BYTE, dst, tag, MPI_COMM_WORLD);
	    VERBOSE("%dth  S****** MPI_Send done\n", th);
	}
	VERBOSE("%dth  S****** send done\n", th);
    } else {
	MPI_Status stat;
	VERBOSE("%dth  S****** Issue MPI_Recv\n", th);
	MPI_Recv(rbuf, length, MPI_BYTE, src, tag, MPI_COMM_WORLD, &stat);
	VERBOSE("%dth  S****** MPI_Recv done\n", th);
    }
}

void
gather_run(int th)
{
    int	dst = th % nprocs;
    int	tag = 50000 + th;

    if (myrank == dst) {
	int	i;
	MPI_Status stat;
	for (i = 1; i < nprocs; i++) {
	    VERBOSE("%dth  G****** Issue MPI_Recv\n", th);
	    MPI_Recv(rbuf, length, MPI_BYTE, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &stat);
	    //VERBOSE("%d\n", stat.MPI_SOURCE);
	    //printf("%04d\n", stat.MPI_SOURCE); fflush(stdout);
	}
	VERBOSE("%dth  G****** MPI_Recv done\n", th);
    } else {
	VERBOSE("%dth  G****** Issue MPI_Send to %d\n", th, dst);
	MPI_Send(sbuf, length, MPI_BYTE, dst, tag, MPI_COMM_WORLD);
	VERBOSE("%dth  G****** MPI_Send done\n", th);
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
	printf("myscatter_gather: nprocs(%d) length(%ld) iteration(%d)\n", nprocs, length, iteration); fflush(stdout);
    }
    for (iter = 0; iter < iteration; iter++) {
	scatter_run(iter);
	gather_run(iter);
    }
    printf(">%d : Going-to-BARRIER\n", myrank);  fflush(stdout);
    MPI_Barrier(MPI_COMM_WORLD);
    if (myrank == 0) {
	printf("DONE:\n"); fflush(stdout);
    }
ext:
    MPI_Finalize();
    return 0;
}
