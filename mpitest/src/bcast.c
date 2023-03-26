/*
 * 
 */
#include <mpi.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "testlib.h"
#include <utf.h>

#define LEN_INIT	1
int	*buf;

int
verify(int *bp, int len)
{
    int	rc = 0;
    int	i;
    for (i = 0; i < len; i++) {
	if (buf[i] != i) {
	    printf("buf[%d] != %d but %d\n", i, i, buf[i]);
	    rc++;
	}
    }
    return rc;
}

int
main(int argc, char** argv)
{
    int	tsz;
    int	errs = 0;
    int	i, sz, count;

    length = LEN_INIT;	// in byte
    iteration = 1;
    sflag = 0x10;	// Alltoall in default
    test_init(argc, argv);

#ifndef FJMPI
    if (vflag) {
	utf_vname_show(stdout);
    }
#endif
    MPI_Type_size(MPI_INT, &tsz);
    buf = (int*) malloc(length*tsz);
    sz = length/tsz;
    if (buf == NULL) {
	fprintf(stderr, "Cannot allocate buffer\n");
	exit(-1);
    }
    if (myrank == 0) {
	printf("MPI_INT size (%d) C int size(%ld) iteration(%d)\n", tsz, sizeof(int), iteration);
	for (i = 0; i < length; i++) {
	    buf[i] = i;
	}
    } else {
	for (i = 0; i < length; i++) {
	    buf[i] = -1;
	}
    }
    for (count = 1; count <= sz; count = count <<1) {
	for (i = 0; i < iteration; i++) {
	    MPI_Bcast(buf, count, MPI_INT, 0, MPI_COMM_WORLD);
//	    errs += verify(buf, count);
	}
	errs += verify(buf, count);
#if 0
	if (errs == 0) {
	    printf("[%d] %d B (%d int) PASS\n", myrank, count*tsz, count);
	} else {
	    printf("[%ds] %d B (%d int) ERROR\n", myrank, count*tsz, count);
	}
#endif
    }
    if (errs == 0) {
	printf("[%d] %d B (%d int) PASS\n", myrank, count*tsz, count);
    } else {
	printf("[%ds] %d B (%d int) ERROR\n", myrank, count*tsz, count);
    }
    MPI_Finalize();
    return 0;
}
