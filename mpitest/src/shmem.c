/*
 * This is a test for utf shmem implementation for VBG
 */
#include <mpi.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include <utf.h>
#include "testlib.h"

#define COUNT	1000

struct shared_data {
    atomic_ulong	data;
};

int
main(int argc, char** argv)
{
    struct shared_data	*sd;
    size_t	sz = 16*1024;
    void	*shmaddr;
    int		ppn;
    int	rc, i;

    test_init(argc, argv);

    rc = utf_shm_init(sz, &shmaddr);
    if (rc != 0) {
	printf("utf_shm_init error (%d)\n", rc);
	return -1;
    }
    utf_intra_nprocs(&ppn);
    if (myrank == 0) printf("%d process per node\n", ppn);
    sd = (struct shared_data*) shmaddr;
    if (myrank == 0) {
	atomic_init(&sd->data, 0);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    /* All processes increment the shared data */
    for (i = 0; i < COUNT; i++) {
	atomic_fetch_add(&sd->data, 1);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    if (sd->data == COUNT*ppn) {
	if (myrank == 0) printf("SUCCESS\n");
    } else {
	printf("[%d] data = %ld, must be %d\n", myrank, sd->data, COUNT*ppn);
    }
    utf_shm_finalize();
    MPI_Finalize();
    return 0;
}
