/*
 * This is a test for the dev1 branch
 */
#include <mpi.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "testlib.h"

int
main(int argc, char** argv)
{

    test_init(argc, argv);
    /* debug option is set */
    // fi_tofu_setdopt(0x3, 0x3);

    VERBOSE("Hello\n");
    MPI_Finalize();
    if (myrank == 0) {
	printf("RESULT hello: PASS\n"); fflush(stdout);
    }

    return 0;
}
