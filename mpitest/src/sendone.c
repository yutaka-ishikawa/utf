#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "testlib.h"

extern int	myprintf(const char *fmt, ...);

#define BUF_LEN		(1024*1024)
#define DEFAULT_LEN	1

int	sendbuf[BUF_LEN], recvbuf[BUF_LEN];
int	buf[BUF_LEN];

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
    int	i, rc, errs;

    length = DEFAULT_LEN;
    test_init(argc, argv);

    if (length > BUF_LEN) {
	if (myrank == 0) myprintf("%s: length must be smaller than %d, but %d\n", BUF_LEN, length);
	goto ext;
    }
    // dry_run(0, 1);
    if (myrank == 0) {
	myprintf("MPICH SENDONE test length(%d)\n", length);
	for (i = 0; i < length; i++) {
	    buf[i] = i;
	}
	rc = MPI_Send(buf, length, MPI_INT, 1, 1000, MPI_COMM_WORLD);
	if (rc != MPI_SUCCESS) {
	    myprintf("Send rc = %d\n", rc);
	}
    } else {
	MPI_Status	stat;
	for (i = 0; i < length; i++) {
	    buf[i] = -1;
	}
	rc = MPI_Recv(buf, length, MPI_INT, 0, 1000, MPI_COMM_WORLD, &stat);
	if (rc != MPI_SUCCESS) {
	    myprintf("Recv rc = %d MPI_SOURCE(%d) MPI_TAG(%d) MPI_ERROR(%d)\n",
		     rc, stat.MPI_SOURCE, stat.MPI_TAG, stat.MPI_ERROR); fflush(stdout);
	}
	/* verify */
	errs = 0;
	for (i = 0; i < length; i++) {
	    if (buf[i] != i) {
		printf("buf[%d] must be %d, but %d\n", i, i, buf[i]);
		errs++;
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

