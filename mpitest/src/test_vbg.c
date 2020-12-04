#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <mpi.h>
#include <utf.h>
#include <utofu.h>
#include <jtofu.h>
#include <utf_bg.h>

static int
errprintf(int myrank, const char *fmt, ...)
{
    va_list	ap;
    int		rc;
    printf("[%d] ", myrank); fprintf(stderr, "[%d] ", myrank);
    va_start(ap, fmt);
    rc = vprintf(fmt, ap);
    rc = vfprintf(stderr, fmt, ap);
    va_end(ap);
    fflush(stdout); fflush(stderr);
    return rc;
}

static int
myprintf(int myrank, const char *fmt, ...)
{
    va_list	ap;
    int		rc;

    if (myrank != 0) return 0;
    va_start(ap, fmt);
    rc = vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);
    return rc;
}


#define UTFCALL_CHECK(lbl, rc, func)			\
{							\
    rc = func;						\
    if (rc != UTF_SUCCESS) goto lbl;			\
}

#define MPICALL_CHECK(lbl, rc, func)			\
{							\
    rc = func;						\
    if (rc != MPI_SUCCESS) goto lbl;			\
}


static int
MPI_UTF_init(int nproc, int myrank, utf_coll_group_t *utf_grp)
{
    int	i, rc;
    uint32_t		*rankset = malloc(sizeof(uint32_t) * nproc);
    utf_bg_info_t	*lcl_bginfo = malloc(sizeof(utf_bg_info_t) * nproc);
    utf_bg_info_t	*rmt_bginfo = malloc(sizeof(utf_bg_info_t) * nproc);
    

    if (rankset == NULL || lcl_bginfo == NULL || rmt_bginfo == NULL) {
	errprintf(myrank, "Cannot allocate working memory for VBG procs(%d)\n", nproc);
    }
    for(i = 0; i < nproc; i++){
	rankset[i] = i;
    }
    UTFCALL_CHECK(err1, rc, utf_bg_alloc(rankset, nproc, myrank,
					 jtofu_query_max_proc_per_node(),
					 UTF_BG_TOFU, lcl_bginfo));
    MPICALL_CHECK(err2, rc, MPI_Alltoall(lcl_bginfo, sizeof(utf_bg_info_t), MPI_BYTE,        
					 rmt_bginfo, sizeof(utf_bg_info_t), MPI_BYTE, MPI_COMM_WORLD));
    UTFCALL_CHECK(err3, rc, utf_bg_init(rankset, nproc, myrank, rmt_bginfo, utf_grp));
    MPICALL_CHECK(err4, rc, MPI_Barrier(MPI_COMM_WORLD));
    free(lcl_bginfo); free(rmt_bginfo); free(rankset);
    return MPI_SUCCESS;
err1:
    errprintf(myrank, "error on utf_bg_alloc %d\n", rc);  goto ext;
err2:
    errprintf(myrank, "error on MPI_Alltoall %d\n", rc); goto ext;
err3:
    errprintf(myrank, "error on utf_bg_init %d\n", rc); goto ext;
err4:
    errprintf(myrank, "error on MPI_Barrier %d\n", rc);
ext:
    return -1;
}

int 
main(int argc, char *argv[])
{
    int		nproc, myrank;
    int64_t	in, out, data;
    utf_coll_group_t utf_grp;
    int	i, rc = 0;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nproc);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    MPICALL_CHECK(err, rc, MPI_UTF_init(nproc, myrank, &utf_grp));

    myprintf(myrank, "Starting utf_barrier\n");
    for(i = 0; i < 1000000; i++){
	UTFCALL_CHECK(err0, rc,  utf_barrier(utf_grp));
	while (utf_poll_barrier(utf_grp) == UTF_ERR_NOT_COMPLETED) { }
    }
    myprintf(myrank, "End of utf_barrier\n");

    in = myrank; out = 0;
    myprintf(myrank, "Start of utf_reduce\n");
    for(i=0; i<1000000; i++){
	rc = utf_reduce(utf_grp, &in, 1, NULL, &out, NULL, UTF_DATATYPE_INT64_T, UTF_REDUCE_OP_SUM, 0);
	if (rc != UTF_SUCCESS) goto err;
	while (utf_poll_reduce (utf_grp, (void **)&data) == UTF_ERR_NOT_COMPLETED) { }
    }
    
    myprintf(myrank, "End of utf_reduce nproc(%d) out=%ld\n", nproc, out);
err:
    MPI_Finalize();
    return rc;
err0:
    errprintf(myrank, "utf_barrier error rc = %d\n", rc);
    goto err;
}
