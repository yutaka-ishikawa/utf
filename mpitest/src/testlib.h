#define VERBOSE(...) do {	\
    if (vflag) {		\
	printf("[%d]: ", myrank);	\
	printf(__VA_ARGS__); \
	fprintf(stderr, "[%d]: ", myrank);	\
	fprintf(stderr, __VA_ARGS__); fflush(stdout); fflush(stderr);	\
    }				\
} while(0)

#if 0
#define MYVERBOSE(...) do {	\
    if (Vflag) {		\
	printf("[%d]: ", myrank);	\
	printf(__VA_ARGS__); \
	fprintf(stderr, "[%d]: ", myrank);	\
	fprintf(stderr, __VA_ARGS__); fflush(stdout); fflush(stderr);	\
    }				\
} while(0)
#endif

#define SEND_RECV(len) do {						\
    rc = MPI_Send(sendbuf, len, MPI_BYTE, receiver, 0, MPI_COMM_WORLD); \
    rc |= MPI_Recv(recvbuf, len, MPI_BYTE, receiver, 0, MPI_COMM_WORLD, &stat);\
    if (rc != 0) errs++;\
} while(0);

#define RECV_SEND(len) do {						\
    rc = MPI_Recv(recvbuf, len, MPI_BYTE, sender, 0, MPI_COMM_WORLD, &stat); \
    rc |= MPI_Send(sendbuf, len, MPI_BYTE, sender, 0, MPI_COMM_WORLD);	\
    if (rc != 0) errs++;\
} while(0);

extern int	bflag, vflag, Vflag, dflag, mflag, sflag, Mflag, pflag, wflag, myrank, nprocs, iteration;
extern size_t	length, mlength;

extern void	test_init(int, char**);
extern void	mytmrinit();
extern void	mytmrfinalize(char *pname);
