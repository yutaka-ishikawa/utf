/*
 * This is a test for the dev1 branch
 */
#include <mpi.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "testlib.h"
#ifndef FJMPI
#include <utf.h>
#endif
#include <utf_tsc.h>

#define LEN_INIT	(1024*1024)
#define MAX_TM_ENT	64
char	*sendbuf;
char	*recvbuf;
uint64_t	hz;
uint64_t	tm_max[MAX_TM_ENT];

#ifdef FJMPI
const char	*marker = "pingpong-fjmpi";
#else
const char	*marker = "pingpong-mpich";
#endif


#define MYPRINT	if (myrank == 0)

#define CLK2USEC(tm)	((double)(tm) / ((double)hz/(double)1000000))
void
show(const char *name, int iter, size_t nbyte, uint64_t tm)
{
    float	usec = (float)CLK2USEC(tm)/(float)iter;
    printf("@%s, %d, %s, %d, %ld, %8.4f, %8.4f\n",
	   marker, nprocs, name, iter, nbyte, usec, ((float)nbyte) / usec);
}

int
verify(int *sendbuf, int length)
{
    int errs = 0;
    int	k;
    for (k = 0; k < length; k++) {
	if (recvbuf[k] != k) {
	    printf("sendbuf[%d] = %d, expect = %d\n", k, recvbuf[k], k);
	    errs++;
	}
    }
    return errs;
}

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
	printf("%s: error last error code(%d)\n", __func__, stat.MPI_ERROR);
    }
}

int
main(int argc, char** argv)
{
    size_t	i, len;
    uint64_t	st, et;
    uint64_t	tm;
    int		tment, sender, receiver;
    MPI_Status stat;
    int		errs = 0;
#ifndef FJMPI
    char	*cp;
    int		tmr = 0;
    cp = getenv("UTF_TIMER");
    if (cp && atoi(cp) == 1) {
	tmr = 1;
    }
#endif

    length = 1;
    mlength = LEN_INIT;
    iteration = 1;
    test_init(argc, argv);

#ifndef FJMPI
    if (vflag) {
	utf_vname_show(stdout);
    }
#endif
    sendbuf = malloc(mlength);
    recvbuf = malloc(mlength);
    MYPRINT {
	printf("sendbuf=%p recvbuf=%p max len(%ld) min len(%ld) nprocs(%d) iteration(%d)\n",
	       sendbuf, recvbuf, mlength, length, nprocs, iteration); fflush(stdout);
    }
    if (sendbuf == NULL || recvbuf == NULL) {
	MYPRINT { printf("Cannot allocate buffers: sz=%ldMiB * 2\n", (uint64_t)(((double)mlength)/(1024.0*1024.0))); }
	MPI_Finalize();
	exit(-1);
    }
    for (i = 0; i < mlength; i++) {
	sendbuf[i] = i;
	recvbuf[i] = (char) -1;
    }
    hz = tick_helz(0);
    sender = 0; receiver = 1; st = et = 0;
    MPI_Barrier(MPI_COMM_WORLD);
    dry_run(sender, receiver);

#ifndef FJMPI
    if (tmr) {
	mytmrinit();
    }
#endif
    for (len = length, tment = 0; len <= mlength && tment < MAX_TM_ENT; len <<= 1, tment++) {
	int rc;
	if (myrank == sender) {
	    st = tick_time();
	    for (i = 0; i < iteration; i++) {
		SEND_RECV(len);
	    }
	    et = tick_time();
	} else if (myrank == receiver) {
	    st = tick_time();
	    for (i = 0; i < iteration; i++) {
		RECV_SEND(len);
	    }
	    et = tick_time();
	}
	tm = et - st;
	MPI_Reduce(&tm, &tm_max[tment], 1, MPI_LONG_LONG, MPI_MAX, 0, MPI_COMM_WORLD);
    }
    if (myrank == 0) {
	printf("#name, nprocs, bench, iteration, byte, usec, MB/sec\n");
	for (len = length, tment = 0; len <= mlength && tment < MAX_TM_ENT; len <<= 1, tment++) {
	    show("PINGPONG", iteration, len, tm_max[tment]);
	}
	fflush(stdout);
    }
#ifndef FJMPI
    {
	char	*cp = getenv("UTF_TIMER");
	if (cp && atoi(cp) == 1) {
	    /* timer report */
	    mytmrfinalize("pingpong-mpich");
	}
    }
#endif
    MPI_Finalize();
    return 0;
}
