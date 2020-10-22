#ifndef UTF_H
#define UTF_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define UTF_SUCCESS		0
#define UTF_MSG_AVL		1
#define UTF_ERR_NOMORE_SNDCNTR	-2
#define UTF_ERR_NOMORE_SNDBUF	-3
#define UTF_ERR_NOMORE_REQBUF	-4
#define UTF_ERR_NOMORE_MINFO	-5

typedef	union {
    struct {
	uint16_t	type;
	int16_t		reqid1;
	int16_t		reqid2;
	uint16_t	aux;
    };
    uint64_t	id;
} UTF_reqid;

extern int	utf_init(int argc, char **argv, int *rank, int *nprocs, int *ppn);
extern int	utf_shm_init(size_t, void **);
extern int	utf_shm_finalize();
extern int	utf_finalize(int wipe);
extern int	utf_send(void *buf, size_t size, int dst, uint64_t tag, UTF_reqid *reqid);
extern int	utf_recv(void *buf, size_t size, int src, int tag, UTF_reqid *reqid);
extern int	utf_sendrecv(void *sbuf, size_t ssize, int src, int stag,
			     void *rbuf, size_t rsize, int dst, int rtag, UTF_reqid *reqid);
extern int	utf_wait(UTF_reqid);
extern int	utf_waitcmpl(UTF_reqid);
extern int	utf_test(UTF_reqid);
extern int	utf_rank(int *rank);
extern int	utf_nprocs(int *psize);
extern int	utf_intra_rank(int *rank);
extern int	utf_intra_nprocs(int *psize);
extern void	utf_fence();

extern int	utf_printf(const char *fmt, ...);

#define UTF_CALL(label, rc, func, ...) do {	\
    char msg[256];				\
    rc = func(__VA_ARGS__);			\
    if (rc < 0) goto label;		\
} while (0);

/* for debugging */
extern void	utf_mem_show(FILE*);
extern void	utf_vname_show(FILE*);
extern void	utf_cqtab_show(FILE*);
extern void	utf_egrrbuf_show(FILE*);
extern void	utf_egrbuf_show(FILE*);

#endif /* ~UTF_H */
