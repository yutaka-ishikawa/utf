#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <mpi.h>
#include <utf.h>

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

int 
main(int argc, char *argv[])
{
    int		nprocs, myrank, utf_myrank, utf_nprocs, utf_ppn;
    int		rc;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    MPICALL_CHECK(err, rc, utf_init(argc, argv, &utf_myrank, &utf_nprocs, &utf_ppn));
    printf("myrank=%d nprocs=%d utf_myrank=%d utf_nprocs=%d utf_ppn=%d\n",
	   myrank, nprocs, utf_myrank, utf_nprocs, utf_ppn);

    utf_finalize(1);
    MPI_Finalize();
    return 0;
err:
    printf("utf_init error: %d\n", rc);
    return 0;
}
