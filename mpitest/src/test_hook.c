#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#define LIB_CALL(rc, funcall, lbl) do {	\
    rc = funcall;			\
    if (rc != 0) {			\
	goto lbl;			\
    }					\
} while(0);

extern int utf_MPI_hook(MPI_Comm, uint32_t *addr_tab, uint32_t *count);

void
show_address(int myrank, int count, uint32_t *addr_tab)
{
    int	i;
    for (i = 0; i < count; i++) {
	printf("[%d] rank(%d) = %d\n", myrank, i, addr_tab[i]);
    }
}

int 
main(int argc, char *argv[])
{
    int		nprocs, myrank;
    int		rc;
    uint32_t	*addr_tab, count;

    LIB_CALL(rc, MPI_Init(&argc, &argv), ferr);
    LIB_CALL(rc, MPI_Comm_size(MPI_COMM_WORLD, &nprocs), ext0);
    LIB_CALL(rc, MPI_Comm_rank(MPI_COMM_WORLD, &myrank), ext0);

    count = nprocs;
    addr_tab = malloc(sizeof(uint32_t)*nprocs);
    LIB_CALL(rc, utf_MPI_hook(MPI_COMM_WORLD, addr_tab, &count), ext1);
    if (count != nprocs) {
	printf("utf_MPI_hook: error return rc(%d) count(%d)\n", rc, count);
	goto ext1;
    }
    show_address(myrank, count, addr_tab);
    {
	MPI_Comm	newcomm;
	int	color  = myrank / 2;
	int	key    = myrank % (nprocs/2);
	int	new_nprocs, new_myrank;
	printf("[%d] color(%d) key(%d)\n", myrank, color, key);
	LIB_CALL(rc, MPI_Comm_split(MPI_COMM_WORLD, color, key, &newcomm), ext1);
	LIB_CALL(rc, MPI_Comm_size(newcomm, &new_nprocs), ext1);
	LIB_CALL(rc, MPI_Comm_rank(newcomm, &new_myrank), ext1);
	printf("[%d] color(%d) key(%d) new nprocs(%d) new myrank(%d)\n", myrank, color, key, new_nprocs, new_myrank);
	count = new_nprocs;
	LIB_CALL(rc, utf_MPI_hook(newcomm, addr_tab, &count), ext1);
	show_address(new_myrank, count, addr_tab);
    }
ext1:
    free(addr_tab);
ext0:
    if (rc == 0) {
	printf("DONE: NO-ERROR\n");
    } else {
	printf("DONE: ERROR rc(%d)\n", rc);
    }
    MPI_Finalize();
    return 0;
ferr:
    printf("DONE: ERROR Cannot initialize MPI\n");
    return -1;
}
