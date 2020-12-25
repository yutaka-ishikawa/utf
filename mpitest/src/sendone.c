#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "testlib.h"

int	sendbuf[128], recvbuf[128];
int	buf[128];

void
dry_run(int sender, int receiver)
{
    MPI_Status stat;
    int errs = 0;
    int	rc, i;
    if (myrank == sender) {
	for (i = 0; i < 10; i++) {
	    SEND_RECV(1);
	}
    } else {
	for (i = 0; i < 10; i++) {
	    RECV_SEND(1);
	}
    }
    if (errs > 0) {
	printf("%s: errs(%d) error last error code(%d)\n", __func__, errs, stat.MPI_ERROR);
    }
}


int
main(int argc, char **argv)
{
    int	rc;
    test_init(argc, argv);

    dry_run(0, 1);
    if (myrank == 0) {
	rc = MPI_Send(buf, 1, MPI_INT, 1, 1000, MPI_COMM_WORLD);
	printf("[%d] Send rc = %d\n", myrank, rc); fflush(stdout);
    } else {
	MPI_Status	stat;
	rc = MPI_Recv(buf, 1, MPI_INT, 0, 1000, MPI_COMM_WORLD, &stat);
	printf("[%d] Recv rc = %d MPI_SOURCE(%d) MPI_TAG(%d) MPI_ERROR(%d)\n",
	       myrank, rc, stat.MPI_SOURCE, stat.MPI_TAG, stat.MPI_ERROR); fflush(stdout);
    }
    MPI_Finalize();
    return 0;
}

