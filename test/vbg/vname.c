#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <libgen.h>
#if defined(TOFUGIJI)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#endif

#include <limits.h>
#if (!defined(TOFUGIJI))
#include <stdatomic.h>
#endif
#include "utf_bg_internal.h"

#if 0
struct tofu_vname {
〇    union jtofu_phys_coords     xyzabc;
〇    union jtofu_log_coords      xyz;
×    uint8_t                     cid : 3;/* component id */
〇    uint8_t                     v : 1;  /* valid */
〇    uint32_t                    vpid;
×    uint8_t                     tniq[8];/* (tni + tcq)[8] */
×    uint8_t                     pent;
×    union jtofu_path_coords     pcoords[5]; /* 3B x 5 = 15B */
×    utofu_path_id_t             pathid;    /* default path id */
×    utofu_vcq_id_t              vcqid;
};
#endif

struct utf_info utf_info;

static char my_hostname[64];
#if defined(TOFUGIJI)
static int my_leader;
#endif

static void set_host(void)
{
    static bool setflag=false;

    if (!setflag) {
        gethostname(my_hostname,64);
        setflag=true;
    }
    return;
}
static inline uint32_t hash_str_for_vname(const char *str)
{
    register uint32_t hash = 0;

    while( *str ) {
        hash += *str++;
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash = (hash + (hash << 15));

    hash &= ~(0x8000);

    return hash;
}

static void utf_shm_print_info(int wrank,int wsize,struct tofu_vname *vname)
{
#if defined(DEBUGLOG)
    int rank;

    fprintf(stderr,"DEBUGLOG: %s:%s wrank=%d wsize=%d utf_info: jobid=%d myrank=%d nprocs=%d ppn=%d myppn=%d lrank=%d nnodes=%zu vname=%p shmaddr=%p shmid=%d \n"
       ,my_hostname,__func__
       ,wrank,wsize
       ,utf_info.jobid
       ,utf_info.myrank
       ,utf_info.nprocs,utf_info.ppn,utf_info.myppn
       ,utf_info.lrank,utf_info.nnodes
       ,utf_info.vname,utf_info.shmaddr,utf_info.shmid);


    if (NULL == vname) { return; }

    for (rank=0;rank<utf_info.nprocs;rank++) {
        fprintf(stderr,"DEBUGLOG: %s:%s rank=%d/%d vname phy6daddr=%02u-%02u-%02u-%1u-%1u-%1u log3daddr=%u-%u-%u v=%d vpid=%u \n"
            ,my_hostname,__func__
            ,rank,utf_info.nprocs
            ,vname[rank].xyzabc.s.x
            ,vname[rank].xyzabc.s.y
            ,vname[rank].xyzabc.s.z
            ,vname[rank].xyzabc.s.a
            ,vname[rank].xyzabc.s.b
            ,vname[rank].xyzabc.s.c
            ,vname[rank].xyz.s.x
            ,vname[rank].xyz.s.y
            ,vname[rank].xyz.s.z
            ,vname[rank].v
            ,vname[rank].vpid
        );
    }
#endif

    return;
}

static jtofu_job_id_t jobid;
int make_vname(int wrank,int wsize)
{
    int rank,i,node,nnodes;
    uint8_t     *pmarker=NULL;
    union jtofu_phys_coords jcoords;
    union jtofu_log_coords lcoords;
#if (!defined(TOFUGIJI))
    pmix_proc_t myproc;
#endif
    struct tofu_vname *vname;

    set_host();

#if (!defined(TOFUGIJI))
    memset((void *)&myproc,0,sizeof(pmix_proc_t));
    if (PMIX_SUCCESS !=  PMIx_Init(&myproc,NULL,0)) {
        return UTF_ERR_INTERNAL;
    }
    utf_info.myrank = myproc.rank;
    jobid = (jtofu_job_id_t)hash_str_for_vname((const char *)myproc.nspace);
    if (PMIX_SUCCESS != PMIx_Finalize(NULL, 0)) {
        return UTF_ERR_INTERNAL;
    } 
#else
    {
    char *tempval;
    if (NULL != (tempval=getenv("UTOFU_WORLD_ID"))) {
        jobid = atoi(tempval);
    }
    else if (NULL != (tempval=getenv("OMPI_MCA_ess_base_jobid"))) {
        jobid = atoi(tempval);
    }
    else if (NULL != (tempval=getenv("OMPI_MCA_orte_ess_jobid"))) {
        jobid = atoi(tempval);
    }
    else {
        return UTF_ERR_INTERNAL;
    }
    utf_info.myrank = wrank;
    }
#endif

    vname = (struct tofu_vname *)malloc(sizeof(struct tofu_vname) * wsize);
    if (NULL == vname) {
        return UTF_ERR_INTERNAL;
    }

    pmarker = (uint8_t *)malloc(sizeof(uint8_t) * wsize);
    if (NULL == pmarker) {
        free(vname);
        return UTF_ERR_INTERNAL;
    }
    memset(pmarker, UCHAR_MAX,sizeof(uint8_t) * wsize);

    utf_info.ppn  = jtofu_query_max_proc_per_node();
    utf_info.nprocs = wsize; 

    node = 0;
    nnodes = 0;
    for (rank=0;rank<utf_info.nprocs;rank++) {
        if (pmarker[rank] == UCHAR_MAX) {
            jtofu_rank_t        nd_ranks[48];
            size_t              nranks;
#if defined(TOFUGIJI)
            int my_node;
#endif
 
            /* xyzabc */
            JTOFU_CALL(1, jtofu_query_phys_coords, jobid,
                       rank, &jcoords);
            JTOFU_CALL(1, jtofu_query_log_coords, jobid,
                       rank, &lcoords); 
            assert(node < NODE_MAX);
            node++;
            nnodes++;

            /* rank info */
            JTOFU_CALL(1, jtofu_query_ranks_from_phys_coords, jobid,
                       &jcoords, utf_info.ppn, nd_ranks, &nranks);

#if defined(TOFUGIJI)
            my_node=0;
            for (i = 0; i < nranks; i++) {
              if (wrank == nd_ranks[i]) { my_node=1; break; }
            }
#endif

            for (i = 0; i < nranks; i++) {
                jtofu_rank_t    this_rank;

#if defined(TOFUGIJI)
                if ((my_node) && (!i)) { my_leader=nd_ranks[i]; }
#endif

                this_rank = nd_ranks[i];
                if (this_rank == utf_info.myrank) {
                    utf_info.mynrnk = i;        /* rank within node */
                    utf_info.myppn = nranks;    /* myrank on a node has nranks processes */
                    utf_info.lrank = nd_ranks[0]; /* leader rank */
                }
                assert(this_rank < utf_info.nprocs);
                /* 6次元 */
                memcpy(&vname[this_rank].xyzabc, &jcoords, sizeof(jcoords));
                /* 3次元 */
                memcpy(&vname[this_rank].xyz, &lcoords, sizeof(lcoords));
                /* VPID == MPI_COMM_WORLD rank */
                vname[this_rank].vpid = this_rank;
                /* 有効 */
                vname[this_rank].v = 1;

                pmarker[this_rank] = i;
            }
        } /* UCHAR_MAX */
    } /* rank */

    utf_info.vname  = vname;
    utf_info.nnodes = nnodes;
    utf_info.jobid  = jobid;

    utf_shm_print_info(wrank,wsize,vname);

    free(pmarker);
    return UTF_SUCCESS;
}

////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////

#define LIB_CALL(rc, funcall, lbl, evar, str) do { \
        rc = funcall;                 \
        if (rc != 0) {                  \
                evar = str; goto lbl;   \
        }                               \
} while(0);

#define SYS_CALL(rc, syscall, lbl, erstr) do { \
        rc = syscall;                   \
        if (rc < 0) {                   \
                perror(erstr);          \
                goto lbl;               \
        }                               \
} while(0);

#define SHMEM_UTF	"-utf"
#define SHMEM_USR	"-usr"

#if defined(TOFUGIJI)
static char xfile[128];
static int xfd=-1;
#endif
static void	*
utf_shm_init_internal(size_t sz, char *mykey, char *type, int *idp)
{
    int	rc = 0;
    char	*errstr=NULL;
    int lead_rank = utf_info.lrank;
    key_t	key;
    int		shmid;
    char	path[PATH_MAX+1], pmkey[PATH_MAX+1];
    void	*addr;

    if (strlen(mykey) + strlen(type) > PATH_MAX) {
	utf_printf("%s: the length of key and type (%ld) exceeds PATH_MAX(%d)\n",
		   __func__, strlen(mykey) + strlen(type), PATH_MAX);
	abort();
    }
    /*
     * pmkey is a PMIx key, basename of mykey followed by type.
     * e.g. pmkey is "MPICH-shm-utf" in case of
     * mykey="/tmp/MPICH-shm" and type ="-utf"
     */
    strcpy(pmkey, basename(mykey)); strcat(pmkey, type);
#if defined(DEBUGLOG)
    utf_printf("%s: utf_info.myrank(%d) lead_rank(%d) PMI_X key=%s\n", __func__ , utf_info.myrank, lead_rank, pmkey);
    fflush(stdout);fflush(stderr);
#endif
    if (utf_info.myrank == lead_rank) {
#if (!defined(TOFUGIJI))
	pmix_value_t	pv;
	volatile unsigned long   ul;
#endif
	/* */
#if (!defined(TOFUGIJI))
	snprintf(path, PATH_MAX, "%s-%07d%s", mykey, getpid(), type);
#else
	snprintf(path, PATH_MAX, "%s-%07d%s-%04d", mykey, 4444444, type,my_leader);
        if (-1 == (xfd = open(path,O_RDWR|O_CREAT, 0666))) {
	    goto err;
        }
        strcpy(xfile,path);
#if defined(DEBUGLOG)
	utf_printf("%s: SHMEM PATH(leader open)=%s\n", __func__, xfile);
#endif
#endif
#if defined(DEBUGLOG)
	utf_printf("%s: SHMEM PATH(leader)=%s\n", __func__, path);
	fflush(stdout);fflush(stderr);
#endif
	key = ftok(path, 1);
#if defined(DEBUGLOG)
	utf_printf("%s: (leader) key=%d \n", __func__, key);
#endif
	SYS_CALL(shmid, shmget(key, sz, IPC_CREAT | 0666), errext, __func__);
#if defined(DEBUGLOG)
	utf_printf("%s: (leader) shmid=%d \n", __func__, shmid);
#endif
	addr = shmat(shmid, NULL, 0);
#if defined(DEBUGLOG)
	utf_printf("%s: (leader) shmat=%p \n", __func__, addr);
#endif
	if (addr == (void*) -1) { perror(__func__); goto errext; }
#if (!defined(TOFUGIJI))
	/* the head memory is used for synchronization */
	atomic_init((atomic_ulong*)addr, 1);
	/* expose SHMEM_KEY_VAL */
	pv.type = PMIX_STRING;
	pv.data.string = path;
	PMIx_Put(PMIX_LOCAL, pmkey, &pv);
	LIB_CALL(rc, PMIx_Commit(), err, errstr, "PMIx_Commit");
	/* wait for other processes' progress */
	do {
		usleep(10);
		ul = atomic_load((atomic_ulong*)addr);
	} while (ul != utf_info.myppn);
#if defined(DEBUGLOG)
	utf_printf("%s: (leader) ul=%lu myppn=%d \n", __func__, ul,utf_info.myppn);
#endif
	/* reset the head memory */
	atomic_init((atomic_ulong*)addr, 0);
	SYS_CALL(rc, shmctl(shmid, IPC_RMID, NULL), errext, __func__);
#if defined(DEBUGLOG)
        utf_printf("%s: IPC_RMID \n", __func__);
#endif
	/*
	 * This shared memory is now private, and this region will be destroyed
	 * if all processes exit. It does not matter if processes abnormaly exit.
	 */
#endif
    } else {
	pmix_proc_t		pmix_tproc[1];
	pmix_value_t	*pv=NULL;
	
	memcpy(pmix_tproc, utf_info.pmix_proc, sizeof(pmix_proc_t));
	pmix_tproc->rank = lead_rank; // PMIX_RANK_LOCAL_NODE does not work
#if (!defined(TOFUGIJI))
	do {
	    rc = PMIx_Get(pmix_tproc, pmkey, NULL, 0, &pv);
	    usleep(1000);
	} while (rc == PMIX_ERR_NOT_FOUND);
	if (rc != PMIX_SUCCESS) {
	    errstr = "PMIx_Get";
	    goto err;
	}
#else
        pv = (pmix_value_t *)malloc(sizeof(pmix_value_t));
        if (NULL == pv) { return NULL; }
	snprintf(path, PATH_MAX, "%s-%07d%s-%04d", mykey, 4444444, type,my_leader);
	pv->data.string = path;
	while (0 != access(pv->data.string,F_OK));
#endif
#if defined(DEBUGLOG)
	utf_printf("%s: SHMEM PATH(children)=%s\n", __func__, pv->data.string);
	fflush(stdout);fflush(stderr);
#endif
	key = ftok(pv->data.string, 1);
#if defined(DEBUGLOG)
	utf_printf("%s: (children) key=%d \n", __func__, key);
#endif
	SYS_CALL(shmid, shmget(key, sz, 0), errext, __func__);
#if defined(DEBUGLOG)
	utf_printf("%s: (children) shmid=%d \n", __func__, shmid);
#endif
	addr = shmat(shmid, NULL, 0);
#if defined(DEBUGLOG)
	utf_printf("%s: (children) shmat=%p \n", __func__, addr);
#endif
	if (addr == (void*) -1) { perror(__func__); fflush(stderr); goto errext; }
#if defined(TOFUGIJI)
        if (pv) { free(pv); }
#else
	atomic_fetch_add((atomic_ulong*)addr, 1);
#endif
    }
#if defined(DEBUGLOG)
    utf_printf("%s: returns %d\n", __func__, shmid);
#endif
    *idp = shmid;
    return addr;
err:
    fprintf(stderr, "%s: rc(%d)\n", errstr, rc);
errext:
    abort();
}


/*
 * utf_shm_finalize_internal is used for both user and utf shmem interface
 */
static int
utf_shm_finalize_internal(int shmid, void *addr)
{
    int	rc = 0;
#if defined(TOFUGIJI)
    SYS_CALL(rc, shmctl(shmid, IPC_RMID, NULL), ext, __func__);    
#if defined(DEBUGLOG)
    utf_printf("%s: IPC_RMID \n", __func__);
#endif
#endif
    SYS_CALL(rc, shmdt(addr), ext, __func__);
ext:
    return rc;
}

/*
 * utf_shm_init and utf_shm_finalize are used by the user
 */
static int	shm_usd = 0;
int
utf_shm_init(size_t sz, void **area)
{
    if (sz == 0 || area == NULL) {
#if defined(DEBUGLOG)
	utf_printf("%s: return -1 \n", __func__);
#endif
	return -1;
    }
    if (shm_usd) {
	*area = NULL;
#if defined(DEBUGLOG)
	utf_printf("%s: return -2 \n", __func__);
#endif
	return -2;
    }
    shm_usd = 1;
    *area = utf_info.shmaddr = utf_shm_init_internal(sz, SHMEM_KEY_VAL_FMT, SHMEM_USR, &utf_info.shmid);
    if (utf_info.shmaddr == NULL) {
#if defined(DEBUGLOG)
	utf_printf("%s: return -3 \n", __func__);
#endif
	return -3;
    }
    utf_shm_print_info(utf_info.myrank,utf_info.nprocs,NULL);
#if defined(DEBUGLOG)
    utf_printf("%s: return 0 \n", __func__);
#endif
    return 0;
}

int
utf_shm_finalize()
{
    int	rc = 0;

    if (!shm_usd) return 0;
    if (utf_info.shmaddr) {
	rc = utf_shm_finalize_internal(utf_info.shmid, utf_info.shmaddr);
    }
    if (rc == 0) {
	utf_info.shmaddr = NULL;
    }
#if defined(TOFUGIJI)
    if (-1 != xfd) {
       close(xfd);
#if defined(DEBUGLOG)
       utf_printf("%s: UNLINK %s \n", __func__, xfile);
#endif
       unlink(xfile);
   }
#endif
#if defined(DEBUGLOG)
	utf_printf("%s: return %d \n", __func__,rc);
#endif
    return rc;
}

