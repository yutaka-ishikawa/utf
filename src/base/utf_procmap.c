#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pmix.h>
#include <utofu.h>
#include <jtofu.h>
#include <pmix_fjext.h>
#include <limits.h>
#include "utf_conf.h"
#include "utf_externs.h"
#include "utf_errmacros.h"
#include "utf_debug.h"
#include "utf_tofu.h"
#include "utf_cqmgr.h"

#define LIB_DLCALL(var, funcall, lbl, evar, str) do { \
	var = funcall;			\
	if (var == NULL) {		\
		evar = str; goto lbl;	\
	}				\
} while(0);

#define LIB_DLCALL2(rc, funcall, lbl, evar, str) do {	\
	rc = funcall;			\
	if (rc != 0) {			\
		evar = str; goto lbl;	\
	}				\
} while(0);

#define LIB_CALL(rc, funcall, lbl, evar, str) do { \
	rc = funcall;		      \
	if (rc != 0) {			\
		evar = str; goto lbl;	\
	}				\
} while(0);

/*
 * 31             24 23           16 15            8 7 6 5  2 1 0
 * +---------------+---------------+---------------+---+----+---+
 * |              x|              y|              z|  a|   b|  c|
 * +---------------+---------------+---------------+---+----+---+
 */
union tofu_coord {
    struct { uint32_t	c: 2, b: 4, a: 2, z: 8, y: 8, x: 8; };
    uint32_t	val;
};

#define PATH_PMIXLIB	"/usr/lib/FJSVtcs/ple/lib64/libpmix.so"

struct utf_info utf_info;
utofu_vcq_id_t	*tab_vcqid;

static pmix_status_t	(*myPMIx_Init)(pmix_proc_t *proc, pmix_info_t info[], size_t ninfo);
static pmix_status_t	(*myPMIx_Get)(const pmix_proc_t *proc, const char key[],
				      const pmix_info_t info[], size_t ninfo,
				      pmix_value_t **val);
static pmix_status_t	(*myPMIx_Put)(pmix_scope_t scope, const char key[],
				      pmix_value_t *val);
static pmix_status_t	(*myPMIx_Commit)(void);
static pmix_status_t	(*myPMIx_Fence)(const pmix_proc_t *procs, size_t nprocs,
					const pmix_info_t info[], size_t ninfo);
static pmix_status_t	(*myPMIx_Finalize)(const pmix_info_t info[], size_t ninfo);

static uint32_t
generate_hash_string(char *cp, int len)
{
    uint32_t	hval = 0;
    int	i;
    for (i = 0; i < len; i++) {
	hval += *cp;
    }
    return hval;
}


/*
 * utf_jtofuinit(int pmixclose)
 *	pmix library is initialized and then getinitializing pmix_myrank pmix_nprocs
 */
static int
utf_jtofuinit(int pmixclose)
{
    pmix_value_t	*pval;
    char *errstr;
    int	rc;

    fprintf(stderr, "%s: calling PMIX_Init\n", __func__); fflush(stderr);
    /* PMIx initialization */
    LIB_CALL(rc, PMIx_Init(utf_info.pmix_proc, NULL, 0),
	     err, errstr, "PMIx_Init");
    fprintf(stderr, "%s: nspace(%s)\n", __func__, utf_info.pmix_proc->nspace); fflush(stderr);
    utf_info.myrank = utf_info.pmix_proc->rank;
    {	/* PMIx info is created */
	int	flag = 1;
	PMIX_INFO_CREATE(utf_info.pmix_info, 1);
	PMIX_INFO_LOAD(utf_info.pmix_info, PMIX_COLLECT_DATA, &flag, PMIX_BOOL);
    }
    /* PMIx wildcard proc is created */
    memcpy(utf_info.pmix_wproc, utf_info.pmix_proc, sizeof(pmix_proc_t));
    utf_info.pmix_wproc->rank = PMIX_RANK_WILDCARD;
    /* Getting # of processes */
    LIB_CALL(rc, PMIx_Get(utf_info.pmix_wproc, PMIX_JOB_SIZE, NULL, 0, &pval),
	     err, errstr, "Cannot get PMIX_JOB_SIZE");
    utf_info.nprocs = pval->data.uint32;

    /* Getting Fujitsu's rank pmap and initializing jtofu library */
    LIB_CALL(rc, PMIx_Get(utf_info.pmix_wproc, FJPMIX_RANKMAP, NULL, 0, &pval),
	     err, errstr, "Cannot get FJPMIX_RANKMAP");
    utf_info.jobid = generate_hash_string(utf_info.pmix_proc->nspace, PMIX_MAX_NSLEN);
    fprintf(stderr, "%s: rank(%d) nprocs(%d) pval->data.ptr(%p)\n", __func__, utf_info.myrank, utf_info.nprocs, pval->data.ptr); fflush(stderr);
    jtofu_initialize(utf_info.jobid, utf_info.myrank,  pval->data.ptr);

    if (pmixclose) {
	LIB_CALL(rc, PMIx_Finalize(NULL, 0),
		 err, errstr, "PMIx_Finalize");
    }
    return 0;
err:
    fprintf(stderr, "%s\n", errstr);
    fprintf(stdout, "%s\n", errstr);
    return -1;
}

/*
 * utf_vname_vcqid:
 *	setup vcqid in vname
 */
void
utf_vname_vcqid(struct tofu_vname *vnmp)
{
    utofu_vcq_id_t  vcqid;
    size_t	ncnt;

    UTOFU_CALL(1, utofu_construct_vcq_id,  (uint8_t*) &vnmp->xyzabc,
	       vnmp->tniq[0]>>4, vnmp->tniq[0]&0x0f, vnmp->cid, &vcqid);
    /* getting candidates of path coord, vnmp->pcoords */
    JTOFU_CALL(1, jtofu_query_onesided_paths, &vnmp->xyzabc, 5, vnmp->pcoords, &ncnt);
    vnmp->pent = ncnt;
    /* pathid is set using the first candidate of vnmp->pcoords */
    UTOFU_CALL(1, utofu_get_path_id, vcqid, vnmp->pcoords[0].a, &vnmp->pathid);
    /* The default pathid is also embeded into vcqid */
#if 0
    UTOFU_CALL(1, utofu_set_vcq_id_path, &vcqid, vnmp->pcoords[0].a);
#endif
    /* suggested by Hatanaka-san 2020/07/20 */
    UTOFU_CALL(1, utofu_set_vcq_id_path, &vcqid, 0);
    {
	union jtofu_path_coords pcoords;
	utofu_path_id_t     paid;
	pcoords.s.pa = vnmp->xyzabc.s.a; pcoords.s.pb = vnmp->xyzabc.s.b;
	pcoords.s.pc = vnmp->xyzabc.s.c;
	utofu_get_path_id(vcqid, pcoords.a, &paid);
	// utf_printf("%s: uc(%d) assumed pathid(0x%x) default pathid(0x%x)\n", __func__, uc, paid, vnmp->pathid);
    }
    vnmp->vcqid = vcqid;
}

static inline int
utf_peers_init()
{
    struct tofu_vname *vnmp;
    union jtofu_phys_coords pcoords;
    union jtofu_phys_coords *physnode;
    union jtofu_log_coords lcoords;
    size_t	sz;
    int		rank, node;
    int		ppn, nnodes;	/* process per node, # of nodes  */
    uint8_t	*pmarker;

    utf_jtofuinit(0);
    if (utf_rflag || getenv("TOFULOG_DIR")) {
	utf_redirect();
    }
    ppn = jtofu_query_max_proc_per_node();
    utf_printf("%s: ppn(%d) nprocs(%d), jobid(%x)\n", __func__, ppn, utf_info.nprocs, utf_info.jobid);
    /* node info */
    sz = sizeof(struct tofu_vname)*utf_info.nprocs;
    vnmp = utf_info.vname = utf_malloc(sz);
    memset(vnmp, 0, sz);
    nnodes = utf_info.nnodes = utf_info.nprocs/ppn;
    sz = sizeof(union jtofu_phys_coords)*nnodes;
    physnode = utf_info.phys_node = utf_malloc(sz);
    /* vcqid is also copied */
    sz = sizeof(utofu_vcq_id_t)*utf_info.nprocs;
    tab_vcqid =  utf_malloc(sz);
    memset(tab_vcqid, 0, sz);
    /* fi addr */
    sz = sizeof(uint64_t)*utf_info.nprocs;
    utf_info.myfiaddr = utf_malloc(sz);
    memset(utf_info.myfiaddr, 0, sz);
    /* */
    sz = utf_info.nprocs*sizeof(uint8_t);
    pmarker = utf_malloc(sz);
    memset(pmarker, UCHAR_MAX, sz);
    node = 0;
    utf_printf("%s: RANK LIST\n", __func__);
    for (rank = 0; rank < utf_info.nprocs; rank++) {
	if(pmarker[rank] == UCHAR_MAX) {
	    utofu_tni_id_t	tni;
	    utofu_cq_id_t	cq;
	    int	i;
	    jtofu_rank_t	nd_ranks[48];
	    size_t		nranks;
	    /* xyzabc */
	    JTOFU_CALL(1, jtofu_query_phys_coords, utf_info.jobid,
		       rank, &pcoords);
	    JTOFU_CALL(1, jtofu_query_log_coords, utf_info.jobid,
		       rank, &lcoords);
	    assert(node < nnodes);
	    physnode[node++] = pcoords;
	    /* rank info */
	    JTOFU_CALL(1, jtofu_query_ranks_from_phys_coords, utf_info.jobid,
		       &pcoords, ppn, nd_ranks, &nranks);
	    for (i = 0; i < nranks; i++) {
		jtofu_rank_t	this_rank;
		/* tni & cqi */
		utf_tni_select(ppn, i, &tni, &cq);
		this_rank = nd_ranks[i];
		if (this_rank == utf_info.myrank) {
		    utf_info.mynrnk = i;	/* rank within node */
		}
		assert(this_rank < utf_info.nprocs);
		vnmp[this_rank].tniq[0] = ((tni << 4) & 0xf0) | (cq & 0x0f);
		vnmp[this_rank].vpid = rank;
		vnmp[this_rank].cid = CONF_TOFU_CMPID;
		memcpy(&vnmp[this_rank].xyzabc, &pcoords, sizeof(pcoords));
		memcpy(&vnmp[this_rank].xyz, &lcoords, sizeof(lcoords));
		utf_vname_vcqid(&vnmp[this_rank]);
		tab_vcqid[this_rank] = vnmp[this_rank].vcqid;
		vnmp[this_rank].v = 1;
		utf_info.myfiaddr[this_rank] = this_rank;
		pmarker[this_rank] = i;
	    }
	}
	/* show */
	utf_printf("\t<%d> %s, vcqid(%lx), tni(%d), cq(%d), cid(%d)\n",
		   rank, pcoords2string(pcoords, NULL, 0),
		   vnmp[rank].vcqid, vnmp[rank].tniq[0]>>4, vnmp[rank].tniq[0]&0x0f,
		   vnmp[rank].cid);
    }
    {
	int	i;
	utf_printf("NODE LIST:\n");
	for (i = 0; i < node; i++) {
	    utf_printf("\t<%d> %s\n", i, pcoords2string(physnode[i], NULL, 0));
	}
    }
    utf_free(pmarker);
    utf_info.myppn = ppn;
    {
	utofu_tni_id_t	tni_prim;
        utofu_tni_id_t	*tnis = 0;
        size_t  ni, ntni = 0;
	int	uc, vhent;

	/* my primary tni */
	utf_tni_select(utf_info.myppn, utf_info.mynrnk, &tni_prim, 0);
	utf_printf("%s: ppn(%d) nrank(%d) tni_prim=%d\n", __func__, utf_info.myppn, utf_info.mynrnk, tni_prim);
	utf_info.tniid = tni_prim;

	/* other VCQ handles are created */
	UTOFU_CALL(1, utofu_create_vcq_with_cmp_id, tni_prim, 0x7, 0, &utf_info.vcqh);
	UTOFU_CALL(1, utofu_query_vcq_id, utf_info.vcqh, &utf_info.vcqid);
	utf_info.tniids[0] = tni_prim;
	utf_info.vcqhs[0] = utf_info.vcqh;
	utf_info.vcqids[0] = utf_info.vcqid;
        UTOFU_CALL(1, utofu_get_onesided_tnis, &tnis, &ntni);
	utf_info.ntni = ntni;
	for (ni = 0, vhent = 1; ni < ntni; ni++) {
	    utofu_tni_id_t	tni_id = tnis[ni];
	    if (tni_id == tni_prim) continue;
	    UTOFU_CALL(1, utofu_create_vcq_with_cmp_id, tni_id, 0x7, 0, &utf_info.vcqhs[vhent]);
	    UTOFU_CALL(1, utofu_query_vcq_id, utf_info.vcqhs[vhent], &utf_info.vcqids[vhent]);
	    utf_info.tniids[vhent] = tnis[ni];
	    vhent++;
	}
	if (tnis) free(tnis);
	utf_printf("%s: MY CQ LIST:\n", __func__);
	for (ni = 0; ni < ntni; ni++) {
	    utf_printf("\t: ni(%d) vcqh(%lx) vcqid(%lx) --> %s\n",
		       utf_info.tniids[ni], utf_info.vcqhs[ni], utf_info.vcqids[ni],
		       vcqh2string(utf_info.vcqhs[ni], NULL, 0));
	}
    }
    {	/* other attributes */
        struct utofu_onesided_caps *cap;
	UTOFU_CALL(1, utofu_query_onesided_caps, utf_info.tniid, &cap);
	utf_info.max_mtu = cap->max_mtu;
	utf_info.max_piggyback_size = cap->max_piggyback_size;
	utf_info.max_edata_size = cap->max_edata_size;
    }
    utf_cqselect_init(utf_info.myppn, utf_info.mynrnk, utf_info.ntni, utf_info.tniids,
		      utf_info.vcqhs);
    utf_printf("%s: returns\n", __func__);
    return 1;
}

/*
 * fi_addr - pointer to tofu vcqhid array
 * npp - number of processes
 * ppnp - process per node
 * rankp - my rank
 */
struct tofu_vname *
utf_get_peers(uint64_t **fi_addr, int *npp, int *ppnp, int *rnkp)
{
    static int notfirst = 0;

    if (notfirst == 0) {
	notfirst = utf_peers_init();
    }
    if (notfirst == -1) {
	*fi_addr = NULL;
	*npp = 0; *ppnp = 0; *rnkp = -1;
	return NULL;
    }
    if (fi_addr) {
	if (*fi_addr) {
	    int	rank;
	    uint64_t	*addr = *fi_addr;
	    for (rank = 0; rank < utf_info.nprocs; rank++) {
		addr[rank] = (uint64_t) rank;
	    }
	} else {
	    *fi_addr = utf_info.myfiaddr;
	}
    }
    *npp = utf_info.nprocs;
    *ppnp = utf_info.myppn;
    *rnkp = utf_info.myrank;
    return utf_info.vname;
}

#define PMIX_TOFU_SHMEM	"TOFU_SHM"

void	*
utf_shm_init(size_t sz, char *mykey)
{
    int	rc;
    char	*errstr;
    int	lead_rank = (utf_info.myrank/utf_info.myppn)*utf_info.myppn;
    key_t	key;
    int		shmid;
    char	buf[128];
    void	*addr;

    if (utf_info.myrank == lead_rank) {
	pmix_value_t	pv;
	strcpy(buf, mykey);
	key = ftok(buf, 1);
	shmid = shmget(key, sz, IPC_CREAT | 0666);
	if (shmid < 0) { perror("error:"); exit(-1); }
	addr = shmat(shmid, NULL, 0);
	/* expose SHMEM_KEY_VAL */
	pv.type = PMIX_STRING;
	pv.data.string = buf;
	PMIx_Put(PMIX_LOCAL, PMIX_TOFU_SHMEM, &pv);
	LIB_CALL(rc, PMIx_Commit(), err, errstr, "PMIx_Commit");
    } else {
	pmix_proc_t		pmix_tproc[1];
	pmix_value_t	*pv;
	char	*errstr;
	int	rc;
	
	memcpy(pmix_tproc, utf_info.pmix_proc, sizeof(pmix_proc_t));
	pmix_tproc->rank = lead_rank; // PMIX_RANK_LOCAL_NODE does not work
	do {
	    rc = PMIx_Get(pmix_tproc, PMIX_TOFU_SHMEM, NULL, 0, &pv);
	    usleep(1000);
	} while (rc == PMIX_ERR_NOT_FOUND);
	if (rc != PMIX_SUCCESS) {
	    errstr = "PMIx_Get";
	    goto err;
	}
	key = ftok(pv->data.string, 1);
	shmid = shmget(key, sz, 0);
	addr = shmat(shmid, NULL, 0);
    }
    return addr;
err:
    fprintf(stderr, "%s: rc(%d)\n", errstr, rc);
    return NULL;
}

int
utf_shm_finalize(void *addr)
{
    return shmdt(addr);
}

void *
utf_cqselect_init(int ppn, int nrnk, int ntni, utofu_tni_id_t *tnis, utofu_vcq_hdl_t *vcqhp)
{
    int	i;
    size_t	psz = sysconf(_SC_PAGESIZE);
    size_t	sz = sysconf(_SC_PAGESIZE);
    struct tni_info	*tinfo;

    sz = ((sizeof(struct cqsel_table) + psz - 1)/sz)*sz;
    utf_printf("pid(%d) %s: nrnk(%d) ntni(%d) sz(%ld) sizeof(struct cqsel_table)=%ld\n", utf_info.mypid, __func__, nrnk, ntni, sz, sizeof(struct cqsel_table));

    utf_info.cqseltab = (struct cqsel_table*) utf_shm_init(sz, SHMEM_KEY_VAL_FMT);
    if (utf_info.cqseltab == NULL) {
	utf_printf("%s: cannot allocate shared memory for CQ management\n", __func__);
	abort();
    }
    if (nrnk == 0) {
	memset(utf_info.cqseltab->snd_len, 0, sizeof(utf_info.cqseltab->snd_len));
	memset(utf_info.cqseltab->rcv_len, 0, sizeof(utf_info.cqseltab->rcv_len));
    }
    tinfo = utf_info.tinfo = &utf_info.cqseltab->node[nrnk];
    tinfo->ppn = ppn;
    tinfo->nrnk = nrnk;
    tinfo->ntni = ntni;
    for (i = 0; i < ntni; i++) {
	assert(tnis[i] < 6);
	tinfo->idx[i] = tnis[i];
	tinfo->vcqhdl[i] = vcqhp[i];
	utofu_query_vcq_id(vcqhp[i], &tinfo->vcqid[i]);
	utf_printf("pid(%d) \t[%d]idx[%d]: vcqh(0x%lx) vcqid(0x%lx)\n", utf_info.mypid, i, tnis[i],
		   vcqhp[i], tinfo->vcqid[i]);
    }
    tinfo->usd[0] = 1;	/* this is used primary including receiving */
    return (void*) tinfo;
}

int
utf_cqselect_finalize()
{
    int	rc;
    utf_cqtab_show();
    if (utf_info.cqseltab) {
	rc = utf_shm_finalize(utf_info.cqseltab);
	if (rc < 0) {
	    perror("YIXXXXXXX  SHMEM");
	}
	utf_info.cqseltab = NULL;
	utf_info.tinfo = NULL;
    }
    return 0;
}

void
utf_fence()
{
    int	rc;
    char *errstr;

    utf_printf("%s: begin\n", __func__);
    LIB_CALL(rc, PMIx_Fence(utf_info.pmix_wproc, 1, utf_info.pmix_info, 1),
	     err, errstr, "PMIx_Fence");
    utf_printf("%s: end\n", __func__);
    return;
err:
    fprintf(stderr, "%s\n", errstr);
}

void
utf_procmap_finalize()
{
    int	i;
    utf_cqselect_finalize();
    utf_free(utf_info.myfiaddr); utf_info.myfiaddr = NULL;
    utf_free(utf_info.vname); utf_info.vname = NULL;
    utf_free(utf_info.phys_node); utf_info.phys_node = NULL;
    for (i = 0; i < utf_info.ntni; i++) {
	if (utf_info.vcqhs[i]) {
	    utf_printf("%s: vcqhs[%d]: 0x%lx\n", __func__, i, utf_info.vcqhs[i]);
	    UTOFU_CALL(1, utofu_free_vcq, utf_info.vcqhs[i]);
	    utf_info.vcqhs[i] = 0;
	}
    }
    PMIx_Finalize(NULL, 0);
}

void
utf_vname_show(FILE *fp)
{
    struct tofu_vname	*vnp = utf_info.vname;
    int	i;

    for (i = 0; i < utf_info.nprocs; i++) {
	fprintf(fp, "\t[%d] xyzabc(%s) xyz(%s) vcqid(0x%lx) cid(%d) tni(%d) cq(%d)\n",
		i, pcoords2string(vnp[i].xyzabc, NULL, 0),
		lcoords2string(vnp[i].xyz, NULL, 0),
		vnp[i].vcqid, vnp[i].cid, TNIQ2TNI(vnp[i].tniq[0]), TNIQ2CQ(vnp[i].tniq[0]));
    }
}

void
utf_tni_show()
{
    int	i;
    utf_printf("TNI info\n");
    for (i = 0; i < TOFU_NIC_SIZE; i++) {
	utf_printf("\tTNI[%d]: put len(0x%lx) get len(0x%lx)\n",
		   i, utf_info.cqseltab->snd_len[i], utf_info.cqseltab->rcv_len[i]);
    }
}

void
utf_cqtab_show()
{
    int i;
    struct tni_info	*tinfo = &utf_info.cqseltab->node[utf_info.mynrnk];
    if (utf_info.mynrnk == 0) {
	utf_tni_show();
    }
    utf_printf("CQ table tinfo(%p) entries(%d) nrank(%d)\n", tinfo, tinfo->ntni, utf_info.mynrnk);
    for (i = 0; i < tinfo->ntni; i++) {
	utf_printf("\t[%d]idx[%d]: vcqh(0x%lx) vhcid(0x%lx) busy(%d)\n",
		   i, tinfo->idx[i], tinfo->vcqhdl[i], tinfo->vcqid[i], tinfo->usd[i]);
    }
    utf_printf("TNI left message\n");
    for (i = 0; i < tinfo->ntni; i++) {
	utf_printf("\t[%d]: send rest(%ld) recv rest(%ld)\n",
		   i, utf_info.cqseltab->snd_len[i], utf_info.cqseltab->rcv_len[i]);
    }
}

/* Fujitsu compiler claim the following statement, but it is OK */
#define NODE_NAME       "host-%s,"
#define NODE_NAME_FMT	("host-" FMT_PHYS_COORDS)
#define NODE_NM_LEN     (6 + 3*6 - 1)	/* excluding termination */

pmix_status_t
PMIx_Resolve_nodes(const char *nspace, char **nodelist)
{
    int	np, tppn, rnk, i;
    size_t	sz;
    char	*lst, *ptr;

    utf_get_peers(NULL, &np, &tppn, &rnk);
    utf_printf("%s: jid=%d nprocs=%d\n", __func__, atoi(nspace), np);
    sz = NODE_NM_LEN*utf_info.nnodes + 2; /* two more */
    lst = malloc(sz);
    if (lst == NULL) {
        utf_printf("%s: Cannot allocate memory size(%d)\n", __func__, sz);
        return -1;
    }
    memset(lst, 0, sz);
    ptr = lst;
    for (i = 0; i < utf_info.nnodes; i++) {
	/* Fujitsu compiler claim the following statement, but it is OK */
        snprintf(ptr, NODE_NM_LEN+1, NODE_NAME, pcoords2string(utf_info.phys_node[i], NULL, 0));
        ptr += NODE_NM_LEN;
    }
    *(ptr - 1) = 0;
    utf_printf("\tlist=%s\n", lst);
    *nodelist = lst;
    return PMIX_SUCCESS;
}

pmix_status_t
PMIx_Resolve_peers(const char *nodename, const char *nspace,
		   pmix_proc_t **procs, size_t *nprocs)
{
    pmix_status_t xc = PMIX_SUCCESS;
    int         srank;
    int         i;
    pmix_proc_t *pr;
    size_t	nranks;
    int		ppn;
    union jtofu_phys_coords	pcoords;
    jtofu_rank_t	nd_ranks[48];

    /*
     * In case of 1 process per 1 node, srank is rank of node.
     */
    utf_printf("%s: nodename=%s nspace=%s\n", __func__, nodename, nspace); fflush(stderr);
    sscanf(nodename, NODE_NAME_FMT, 
	   &pcoords.s.x, &pcoords.s.y, &pcoords.s.z,
	   &pcoords.s.a, &pcoords.s.b, &pcoords.s.c);
    utf_printf("\t:\t%s\n\t", pcoords2string(pcoords, NULL, 0));
    JTOFU_CALL(1, jtofu_query_ranks_from_phys_coords, utf_info.jobid,
	       &pcoords, utf_info.myppn, nd_ranks, &nranks);
    PMIX_PROC_CREATE(pr, nranks); /* pmix_proc_t has two members: nspace and rank */
    for (i = 0; i < nranks; i++) {
	memset(&pr[i], 0, sizeof(pmix_proc_t));
	strncpy(pr[i].nspace, nspace, PMIX_MAX_NSLEN);
	pr[i].rank = nd_ranks[i];
	fprintf(stderr, " rank(%d)", nd_ranks[i]);
    }
    fprintf(stderr, "\n"); fflush(stderr);
    *procs = pr;
    *nprocs = nranks;
    return xc;
}
