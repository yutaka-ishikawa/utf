#ifndef UTF_H
#define UTF_H

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <utofu.h>

//#define UTF_SUCCESS		0
#define UTF_MSG_AVL		1
#define UTF_ERR_NOMORE_SNDCNTR	-2
#define UTF_ERR_NOMORE_SNDBUF	-3
#define UTF_ERR_NOMORE_REQBUF	-4
#define UTF_ERR_NOMORE_MINFO	-5

enum utf_return_code {
     UTF_SUCCESS              = UTOFU_SUCCESS,

     UTF_ERR_NOT_FOUND        = UTOFU_ERR_NOT_FOUND,
     UTF_ERR_NOT_COMPLETED    = UTOFU_ERR_NOT_COMPLETED,
     UTF_ERR_NOT_PROCESSED    = UTOFU_ERR_NOT_PROCESSED,
     UTF_ERR_BUSY             = UTOFU_ERR_BUSY,
     UTF_ERR_USED             = UTOFU_ERR_USED,
     UTF_ERR_FULL             = UTOFU_ERR_FULL,
     UTF_ERR_NOT_AVAILABLE    = UTOFU_ERR_NOT_AVAILABLE,
     UTF_ERR_NOT_SUPPORTED    = UTOFU_ERR_NOT_SUPPORTED,

     UTF_ERR_TCQ_OTHER        = UTOFU_ERR_TCQ_OTHER,
     UTF_ERR_TCQ_DESC         = UTOFU_ERR_TCQ_DESC,
     UTF_ERR_TCQ_MEMORY       = UTOFU_ERR_TCQ_MEMORY,
     UTF_ERR_TCQ_STADD        = UTOFU_ERR_TCQ_STADD,
     UTF_ERR_TCQ_LENGTH       = UTOFU_ERR_TCQ_LENGTH,

     UTF_ERR_MRQ_OTHER        = UTOFU_ERR_MRQ_OTHER,
     UTF_ERR_MRQ_PEER         = UTOFU_ERR_MRQ_PEER,
     UTF_ERR_MRQ_LCL_MEMORY   = UTOFU_ERR_MRQ_LCL_MEMORY,
     UTF_ERR_MRQ_RMT_MEMORY   = UTOFU_ERR_MRQ_RMT_MEMORY,
     UTF_ERR_MRQ_LCL_STADD    = UTOFU_ERR_MRQ_LCL_STADD,
     UTF_ERR_MRQ_RMT_STADD    = UTOFU_ERR_MRQ_RMT_STADD,
     UTF_ERR_MRQ_LCL_LENGTH   = UTOFU_ERR_MRQ_LCL_LENGTH,
     UTF_ERR_MRQ_RMT_LENGTH   = UTOFU_ERR_MRQ_RMT_LENGTH,

     UTF_ERR_BARRIER_OTHER    = UTOFU_ERR_BARRIER_OTHER,
     UTF_ERR_BARRIER_MISMATCH = UTOFU_ERR_BARRIER_MISMATCH,

     UTF_ERR_INVALID_ARG      = UTOFU_ERR_INVALID_ARG,
     UTF_ERR_INVALID_POINTER  = UTOFU_ERR_INVALID_POINTER,
     UTF_ERR_INVALID_FLAGS    = UTOFU_ERR_INVALID_FLAGS,
     UTF_ERR_INVALID_COORDS   = UTOFU_ERR_INVALID_COORDS,
     UTF_ERR_INVALID_PATH     = UTOFU_ERR_INVALID_PATH,
     UTF_ERR_INVALID_TNI_ID   = UTOFU_ERR_INVALID_TNI_ID,
     UTF_ERR_INVALID_CQ_ID    = UTOFU_ERR_INVALID_CQ_ID,
     UTF_ERR_INVALID_BG_ID    = UTOFU_ERR_INVALID_BG_ID,
     UTF_ERR_INVALID_CMP_ID   = UTOFU_ERR_INVALID_CMP_ID,
     UTF_ERR_INVALID_VCQ_HDL  = UTOFU_ERR_INVALID_VCQ_HDL,
     UTF_ERR_INVALID_VCQ_ID   = UTOFU_ERR_INVALID_VCQ_ID,
     UTF_ERR_INVALID_VBG_ID   = UTOFU_ERR_INVALID_VBG_ID,
     UTF_ERR_INVALID_PATH_ID  = UTOFU_ERR_INVALID_PATH_ID,
     UTF_ERR_INVALID_STADD    = UTOFU_ERR_INVALID_STADD,
     UTF_ERR_INVALID_ADDRESS  = UTOFU_ERR_INVALID_ADDRESS,
     UTF_ERR_INVALID_SIZE     = UTOFU_ERR_INVALID_SIZE,
     UTF_ERR_INVALID_STAG     = UTOFU_ERR_INVALID_STAG,
     UTF_ERR_INVALID_EDATA    = UTOFU_ERR_INVALID_EDATA,
     UTF_ERR_INVALID_NUMBER   = UTOFU_ERR_INVALID_NUMBER,
     UTF_ERR_INVALID_OP       = UTOFU_ERR_INVALID_OP,
     UTF_ERR_INVALID_DESC     = UTOFU_ERR_INVALID_DESC,
     UTF_ERR_INVALID_DATA     = UTOFU_ERR_INVALID_DATA,

     UTF_ERR_OUT_OF_RESOURCE  = UTOFU_ERR_OUT_OF_RESOURCE,
     UTF_ERR_OUT_OF_MEMORY    = UTOFU_ERR_OUT_OF_MEMORY,

     UTF_ERR_FATAL            = UTOFU_ERR_FATAL,

     UTF_ERR_RESOURCE_BUSY    = -1024,
     UTF_ERR_INTERNAL         = -1025
};

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
extern void	utf_vname_show(FILE*);
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
