/*
 * file operation with flock 
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "tsim_debug.h"

#define LOCK_FILE	"./bfile.lck"

int
mylock(char *path)
{
    int	cc;
    int fd;

    if ((fd = open(path, O_CREAT|O_RDWR, 0666)) < 0) {
	fprintf(stderr, "The file %s cannot be created\n", path);
	perror("open");	fflush(stderr);
	exit(-1);
    }
    if ((cc = lockf(fd, F_LOCK, 128)) < 0) {
	fprintf(stderr, "The file %s cannot be locked (%d)\n", path, cc);
        perror("lockf"); fflush(stderr);        
        exit(-1);                                                                
    }  
#if 0
    if ((cc = flock(fd, LOCK_EX)) < 0) {
	fprintf(stderr, "The file %s cannot be locked (%d)\n", path, cc);
	perror("flock"); fflush(stderr);
	exit(-1);
    }
#endif
    return fd;
}

int
myunlock(int fd)
{
    int	cc;
    cc = close(fd);
    return cc;
}


int
locked_operation(char *path, int (*func)(int fd, void**), void **opt)
{
    int		fd;
    int		cc, rv;

    if ((fd = open(path, O_CREAT|O_RDWR, 0666)) < 0) {
	fprintf(stderr, "The file %s cannot be created\n", path);
	perror("open");	fflush(stderr);
	exit(-1);
    }
    rv = func(fd, opt);
    if (rv < 0) {
	cc = rv; goto err;
    }
    //syncfs(fd);
    cc = close(fd);
err:
    return cc;
}

static inline int
count_marker(char *bufp, int size, char marker)
{
    int	i;
    int count = 0;
    for (i = 0; i < size; i++) {
	if (bufp[i] == 0 || bufp[i] != marker) break;
	count++;
    }
    return count;
}

int
peek(int fd, void **opt)
{
    char	*bufp;
    long	count = 0;
    long volatile *rstp;
    ssize_t	sz, size;
    char	marker;

    /* parameter extract */
    bufp = opt[0];
    size = (ssize_t) opt[1];
    rstp = (long*) opt[2];
    marker = '0' + (long) opt[4];

    memset(bufp, 0, size);
    sz = read(fd, bufp, size);
    if (sz < 0) {
	return -1;
    }
    count = count_marker(bufp, (int) size, marker);
    *rstp = count;
    return 0;
}

int
update(int fd, void **opt)
{
    char	*bufp;
    ssize_t	sz, size;
    long volatile *rstp;
    off_t	off;
    int		marker;

    /* parameter extract */
    bufp = opt[0];
    size = (ssize_t) opt[1];
    rstp = (long*) opt[2];
    off = (long) opt[3]; /* myrank == offset */
    marker = '0' + (long) opt[4];

    memset(bufp, 0, size);
    sz = read(fd, bufp, size);
    if (sz < 0) {
	return -1;
    }
    bufp[off] = marker;
    lseek(fd, off, SEEK_SET);
    sz = write(fd, &bufp[off], 1);
    if (sz != 1) {
	return -2;
    }
    *rstp = count_marker(bufp, (int) size, marker);
    DEBUG {
	fprintf(stderr, "%s: rslt(%ld) @ rank %d\n", __func__, *(long*)opt[2], (int) (long)opt[3]); fflush(stderr);
    }
    return 0;
}

static char	pathbuf[1024];
static long	ntimes = 0;


int
fbarrier(char *path, int nprocs, int myrank)
{
    char	*bufp;
    void	*opt[5];
    int		cc;
    volatile long rst;

    bufp = malloc(nprocs);
    if (bufp == 0) {
	fprintf(stderr, "%s: cannot allocate working memory\n", __func__); fflush(stderr);
	exit(-1);
    }
    memset(bufp, 0, nprocs);
    rst = 0;
    opt[0] = (void*) bufp;  opt[1] = (void*)(long) nprocs; opt[2] = (void*) &rst;
    opt[3] = (void*) (long) myrank;
    opt[4] = (void*) (long) (ntimes%4); /* store value */

    snprintf(pathbuf, 1024, "%s-%d", path, (int) (ntimes % 2));
    fprintf(stderr, "%s: update %s\n", __func__, pathbuf); fflush(stderr);
    cc = locked_operation(pathbuf, update, opt);
    if (cc < 0) {
	fprintf(stderr, "fbarrier: error(%d)\n", cc); fflush(stderr);
    }
    DEBUG {
	fprintf(stderr, "cur rslt(%ld) expected(%d) in %s at rank %d\n", rst, nprocs, pathbuf, myrank);  fflush(stderr);
    }
    while (rst != nprocs) {
	cc = locked_operation(pathbuf, peek, opt);
	if (cc < 0) {
	    fprintf(stderr, "fbarrier: error(%d)\n", cc); fflush(stderr);
	}
    }
    DEBUG {
	fprintf(stderr, "passed cur rslt(%ld) expected(%d) in %s at rank %d\n", rst, nprocs, pathbuf, myrank);  fflush(stderr);
    }
    ntimes++;
    free(bufp);
    return cc;
}

#ifdef TEST_LOCAL
/*
 * Test program:
 *	"FILE"-0 "FILE"-1 must be removed before testing
 */
#define ITER	10
#define FILE	"./bfile.txt"
int
main(int argc, char **argv)
{
    int	i, nprocs, myrank, cc;

    if (argc != 2) {
	fprintf(stderr, "%s <nprocs>\n", argv[0]); exit(-1);
    }
    nprocs = atoi(argv[1]);
    {
	char *cp = getenv("PMI_RANK");
	if (cp) {
	    myrank = atoi(cp);
	} else {
	    myrank = 0;
	}
    }
    DEBUG {
	fprintf(stderr, "nprocs=%d myrank=%d\n", nprocs, myrank); fflush(stderr);
    }
    for (i = 0; i < ITER; i++) {
	fbarrier(FILE, nprocs, myrank);
	printf("pass\n"); fflush(stdout);
    }
    return 0;
}
#endif /* TEST_LOCAL */
