/*
 *	sendrecv
 *	-D option
 *		   2 : change of sender and receiver, sender is rank 1
 *		   4 : using nonblocking receive
 *		   8 : receive operation delayed, an incoming message is enqueued into unexpected
 *		0x18 :   with displaying progress message
 *		0x80 : debug mode
 */
#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "testlib.h"

#define	ELAPSE		5
#define	LENGTH		8
//#define BSIZE		(32*1024*1024)

int	sender, receiver;
//int	s_array[BSIZE/sizeof (int)];
//int	r_array[BSIZE/sizeof (int)];
int	*s_array, *r_array;
int	errs;

void
mysend(int nth, unsigned char *buf, int size, int peer)
{
    VERBOSE("%dth MPI_Send size(%ld) to rank %d\n", nth, size*sizeof(int), peer);
    VERBOSE("\t buf[0] = %x buf[%d-1] = %x\n", buf[0], size, buf[size-1]);

    MPI_Send(buf, size, MPI_INT, peer, 1000, MPI_COMM_WORLD);
    VERBOSE("%dth MPI_Send done\n", nth);
}

void
myrecv(int nth, unsigned char *buf, int size, int peer, int nonblk, int rdelay)
{
    MPI_Status	stat;
    MPI_Request	req;
    int		flg = 0;
    int		i;

    VERBOSE("%dth MPI_Recv size(%ld) from rank %d with delay %d msec\n",
	    nth, size*sizeof(int), peer, rdelay > 0 ? 100 : 0);
    if (rdelay && peer == 0) {
	extern void	utf_progress();
	int	i;
	int	dl = 1000; /* 1 msec */
	for (i = 0; i < 5; i++) {
	    usleep(dl);
	    utf_progress();
	}
    }
    // fi_tofu_cntrl(0, rdelay == 1 ? 0 : 1);
    if (nonblk) {
	MPI_Irecv(buf, size, MPI_INT, peer,
		  1000, MPI_COMM_WORLD, &req);
	for (i = 0; i < 200; i++) { /* 200 msec */
	    MPI_Test(&req, &flg, &stat);
	    if (flg != 0) break;
	    usleep(1000);
	}
	VERBOSE("%dth retried (%d) buf[0] = %x buf[%d-1] = %x\n",
		nth, i, buf[0], size, buf[size-1]);
    } else {
	MPI_Recv(buf, size, MPI_INT, peer,
		 1000, MPI_COMM_WORLD, &stat);
	VERBOSE("%dth buf[0] = %x buf[%d-1] = %x\n",
		nth, buf[0], size, buf[size-1]);
    }
}

void
init_buf(int *ip, int iter, int off, int len)
{
    int	i;
    for (i = 0; i < len; i++) {
	ip[i] = iter + i + off;
    }
}

int
check_buf(int *ip, int iter, int off, int len)
{
    int	i, errs = 0;
    for (i = 0; i < len; i++) {
	if (ip[i] != iter + i + off) {
	    printf("[%d] ip[%d] must be %d, but %d\n", myrank, i, iter + i + off, ip[i]);
	    errs++;
	}
    }
    return errs;
}

void
test(int length, int iteration, int dflag, int sender, int receiver)
{
    int		i, s_ping, s_pong;
    unsigned char	*sp, *rp;
    int		nonblk = 0;
    int		rdelay = 0;

    /* the same buffer and same size in default */
    s_ping = length / sizeof (int); s_pong = s_ping;
    sp = (unsigned char*) s_array; rp = (unsigned char*) r_array;
    if (dflag & 1) {
	/* different buffer, pong is short message */
	sp = (unsigned char*) s_array; rp = (unsigned char*) r_array;
	s_pong = 8;
    }
    /* dflag & 2: change of roll, sender and receiver */
    if (dflag & 2) {
	sender = 1; receiver = 0;
    }
    if (dflag & 4) {
	/* Using Nonblock Receive */
	nonblk = 1;
    }
    if (dflag & 8) {
	/* receive operation is delayed so that
	 * an incoming message is enqueued into unexp list
	 */
	rdelay = 1;
	// if (dflag & 0x10) rdelay = 2;
    }
    if (myrank == 0) {
	VERBOSE("**ping size(%ld) pong size(%ld)\n",
		sizeof(int)*s_ping, sizeof(int)*s_pong);
    }
    if (myrank == sender) {
	VERBOSE("send(%p) then recv(%p) in rank %d, peer(%d)\n",
		sp, rp, myrank, receiver);
	for (i = 0; i < iteration; i++) {
	    init_buf((int*) sp, 0x80, i, s_ping);
	    memset(rp, 0, s_ping);
	    /**/
	    mysend(i, sp, s_ping, receiver);
	    if (dflag & 16) continue;
	    myrecv(i, rp, s_pong, receiver, nonblk, rdelay);
	    errs = check_buf((int*) rp, 0x08, i, s_ping);
	}
    } else if (myrank == receiver) {
	VERBOSE("recv(%p) then send(%p) in rank %d, peer(%d)\n",
		rp, sp, myrank, sender);
	for (i = 0; i < iteration; i++) {
	    init_buf((int*) sp, 0x08, i, s_ping);
	    memset(rp, 0, s_ping);
	    /**/
	    myrecv(i, rp, s_ping, sender, nonblk, rdelay);
	    if (dflag & 16) continue;
	    mysend(i, sp, s_pong, sender);
	    errs = check_buf((int*) rp, 0x80, i, s_ping);
	}
    } else {
	VERBOSE("Not participate in rank(%d)\n",  myrank);
    }
}

int
main(int argc, char **argv)
{
    /* debug option is set */
    // fi_tofu_setdopt(0x3, 0x3);
    length = LENGTH; iteration = 3;
    sender = 0; receiver = 1;
    test_init(argc, argv);

#if 0
    if (dflag & 0x80) { /* debug mode */
	fi_tofu_setdopt(0x3, 0x3);
    }
    if (myrank == 0 && dflag) {
	printf("Debug flag is set %d(0x%x)\n", dflag, dflag); fflush(stdout);
    }
#endif
    errs = 0;
    s_array = malloc(length);
    r_array = malloc(length);
    if (s_array == NULL || r_array == NULL) {
	printf("Cannot allocate buffer. Too large message size.\n");
	exit(-1);
    }
    memset(s_array, 0, length);
    memset(r_array, 0, length);

    if (sender > nprocs || receiver > nprocs) {
	printf("sender(%d) or receiver(%d) is out of range\n",
	       sender, receiver);
	exit(-1);
    }

    test(length, iteration, dflag, sender, receiver);

    if (myrank == sender) {
	printf("sendrecv iter(%d) length(%ld): ", iteration, length);
	if (errs) {
	    printf("FAIL (%d)\n", errs);
	} else {
	    printf("PASS\n");
	}
    }

    VERBOSE("MPI_Finalize\n");
    MPI_Finalize();
    VERBOSE("Exiting\n");

    return 0;
}
