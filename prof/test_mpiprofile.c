/* fccpx -Nclang -ffj-fjprof -o test_profile test_profile.c */
#include <mpi.h>
#include <stdio.h>
#ifndef NOT_FCOMPILER
#include "fj_tool/fapp.h"
#endif

int	ar[1024][1024];

int
main(int argc, char **argv)
{
    int	nprocs, myrank;
    int i, j;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    if (myrank == 0) printf("Hello starts\n");

#ifndef NOT_FCOMPILER
    fapp_start("section-1", 1, 0);
#endif
    for (i = 0; i < 1024; i++) {
	for (j = 0; j < 1024; j++) {
	    ar[i][j] = i;
	}
    }
#ifndef NOT_FCOMPILER
    fapp_stop("section-1", 1, 0);
    fapp_start("section-2", 1, 0);
#endif
    for (i = 0; i < 1024; i++) {
	for (j = 0; j < 1024; j++) {
	    ar[i][j] = i;
	}
    }
#ifndef NOT_FCOMPILER
    fapp_stop("section-2", 1, 0);
#endif
    MPI_Finalize();
    if (myrank == 0) printf("Hello ends\n");
    return 0;
}
