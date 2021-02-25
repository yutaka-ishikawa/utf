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

int
main(int argc, char** argv)
{

    test_init(argc, argv);
    if (vflag) {
	utf_vname_show(stdout);
    }
    VERBOSE("Hello\n");
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    if (myrank == 0) {
	printf("RESULT hello procs(%d): PASS\n", nprocs); fflush(stdout);
    }
    return 0;
}
