#define VERBOSE(...) do {	\
    if (vflag) {		\
	printf("[%d]: ", myrank);	\
	printf(__VA_ARGS__); \
	fprintf(stderr, "[%d]: ", myrank);	\
	fprintf(stderr, __VA_ARGS__); fflush(stdout); fflush(stderr);	\
    }				\
} while(0)

#define MYVERBOSE(...) do {	\
    if (Vflag) {		\
	printf("[%d]: ", myrank);	\
	printf(__VA_ARGS__); \
	fprintf(stderr, "[%d]: ", myrank);	\
	fprintf(stderr, __VA_ARGS__); fflush(stdout); fflush(stderr);	\
    }				\
} while(0)

extern int	vflag, Vflag, dflag, mflag, sflag, Mflag, pflag, wflag, myrank, nprocs, iteration;
extern size_t	length, mlength;

extern void	test_init(int, char**);
extern void	mytmrinit();
extern void	mytmrfinalize(char *pname);
