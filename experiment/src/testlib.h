#define VERBOSE(...) do {	\
    if (vflag) {		\
	printf("[%d]: ", myrank);	\
	printf(__VA_ARGS__); \
	fprintf(stderr, "[%d]: ", myrank);	\
	fprintf(stderr, __VA_ARGS__); fflush(stdout); fflush(stderr);	\
    }				\
} while(0)

extern int	vflag, dflag, mflag, sflag, Mflag, pflag, myrank, nprocs, iteration;;
extern size_t	length;

extern void	test_init(int, char**);
extern int	myprintf(const char *fmt, ...);
extern void	mytmrinit();
extern void	mytmrfinalize(char*);
