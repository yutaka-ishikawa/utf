#include <utofu.h>

extern void	utf_init(int argc, char **argv, int *myrank, int *nprocs, int *ppn);
extern void	utf_finalize();
extern int	utf_rank(int *);
extern int	utf_nprocs(int *);
extern int	utf_vcqh(utofu_vcq_hdl_t *);
extern void	*utf_malloc(size_t);
extern void	utf_free(void*);
extern int	utf_printf(const char *fmt, ...);
extern void	show_compile_options(FILE*, char *buf, ssize_t sz);

extern char	*recvbuf;	/* receiver buffer */
extern char	*sendbuf;	/* sender buffer */
extern utofu_stadd_t	recvstadd;	/* stadd of eager receiver buffer */
extern utofu_stadd_t	sendstadd;	/* stadd of eager receiver buffer */
