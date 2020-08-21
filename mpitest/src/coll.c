/*
 * This is a test for the dev1 branch
 */
#include <mpi.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "testlib.h"

#define LEN_INIT	1
int	*sendbuf;
int	*recvbuf;

#define MYPRINT	if (myrank == 0)

int
main(int argc, char** argv)
{
    int	tsz;
    size_t	i, sz;

    length = LEN_INIT;
    test_init(argc, argv);

    MPI_Type_size(MPI_INT, &tsz);
    sz = length*nprocs*tsz;
    sendbuf = malloc(sz);
    recvbuf = malloc(sz);
    for (i = 0; i < length*nprocs; i++) sendbuf[i] = myrank + i + 1;

    MYPRINT {
	printf("sendbuf=%p recvbuf=%p\n"
	       "MPI_INT SIZE: %d\n"
	       "length(%ld) byte(%ld) nprocs(%d)\n",
	       sendbuf, recvbuf, tsz, length, sz, nprocs); fflush(stdout);
    }
#if 0
    MYPRINT { VERBOSE("After MPI_Init with %d\n", nprocs); }
    MPI_Barrier(MPI_COMM_WORLD);
    MYPRINT { VERBOSE("After MPI_Barrier with %d\n", nprocs); }

    MPI_Reduce(sendbuf, recvbuf, length, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    MYPRINT { VERBOSE("After MPI_Reduce with %d\n", nprocs); }

    MPI_Allreduce(MPI_IN_PLACE, sendbuf, length, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    MYPRINT { VERBOSE("After MPI_Allreduce with %d\n", nprocs); }

    MPI_Gather(sendbuf, length, MPI_INT, recvbuf, length, MPI_INT, 0, MPI_COMM_WORLD);
    MYPRINT { VERBOSE("After MPI_Gather with %d\n", nprocs); }
#endif

    VERBOSE("Start of Alltoall %d\n", nprocs);
    MPI_Alltoall(sendbuf, length, MPI_INT, recvbuf, length, MPI_INT, MPI_COMM_WORLD);
    VERBOSE("End of Alltoall %d\n", nprocs);

    MPI_Finalize();
    MYPRINT { printf("RESULT coll: PASS\n"); fflush(stdout); }
    return 0;
}
