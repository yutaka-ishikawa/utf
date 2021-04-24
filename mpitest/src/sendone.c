#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "testlib.h"

extern int	myprintf(const char *fmt, ...);

#define DEFAULT_LEN	1
#define DEFAULT_ITER	1

#define BUF_LEN		(1024*1024)
int	st_sendbuf[BUF_LEN], st_recvbuf[BUF_LEN];
int	*sendbuf, *recvbuf;

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
    int	i, j, rc, errs = 0;

    length = DEFAULT_LEN;
    iteration = DEFAULT_ITER;
    test_init(argc, argv);

    if (length > BUF_LEN) {
	if (myrank == 0) myprintf("%s: length must be smaller than %d, but %d\n", BUF_LEN, length);
	goto ext;
    }
    if (sflag) {
	sendbuf = st_sendbuf;
	recvbuf = st_recvbuf;
    } else {
	sendbuf = malloc(length*sizeof(int));
	recvbuf = malloc(length*sizeof(int));
    }
    if (sendbuf == NULL || recvbuf == NULL) {
	myprintf("Cannot allocate buffers: sz=%ldMiB * 2\n", (uint64_t)(((double)length*sizeof(int))/(1024.0*1024.0)));
	goto ext;
    }
    // dry_run(0, 1);
    if (myrank == 0) {
	myprintf("MPICH SENDONE test length(%d) iteration(%d) %s sendbuf(%p) recvbuf(%p)\n", length, iteration, sflag ? "STATIC" : "DYNAMIC", sendbuf, recvbuf);
	for (i = 0; i < length; i++) {
	    sendbuf[i] = i;
	}
	for (j = 0; j < iteration; j++) {
	    rc = MPI_Send(sendbuf, length, MPI_INT, 1, 1000, MPI_COMM_WORLD);
	    if (rc != MPI_SUCCESS) {
		myprintf("Send rc = %d\n", rc);
	    }
	}
    } else {
	MPI_Status	stat;

	for (j = 0; j < iteration; j++) {
	    for (i = 0; i < length; i++) {
		recvbuf[i] = -1;
	    }
	    rc = MPI_Recv(recvbuf, length, MPI_INT, 0, 1000, MPI_COMM_WORLD, &stat);
	    if (rc != MPI_SUCCESS) {
		myprintf("Recv rc = %d MPI_SOURCE(%d) MPI_TAG(%d) MPI_ERROR(%d)\n",
			 rc, stat.MPI_SOURCE, stat.MPI_TAG, stat.MPI_ERROR); fflush(stdout);
	    }
	    /* verify */
	    errs = 0;
	    for (i = 0; i < length; i++) {
		if (recvbuf[i] != i) {
		    printf("recvbuf[%d] must be %d, but %d\n", i, i, recvbuf[i]);
		    errs++;
		}
	    }
	}
	if (errs) {
	    printf("ERRORS %d\n", errs);
	} else {
	    printf("SUCCESS\n");
	}
    }
ext:
    MPI_Finalize();
    return 0;
}

