/*
 *
 */
#include <pmix.h>
#include <utofu.h>
#include <jtofu.h>

#define TST_RECV_BUF_SIZE	(1024*1024)
#define TST_SEND_BUF_SIZE	(16*1024)
#define TST_RECV_TAG		13
#define TST_SEND_TAG		14
#define CONF_TOFU_CMPID	07
#pragma pack(1)
struct tofu_vname {
    union jtofu_phys_coords	xyzabc;
    union jtofu_log_coords	xyz;
    uint8_t			cid : 3;/* component id */
    uint8_t			v : 1;	/* valid */
    uint32_t			vpid;
    uint8_t			tniq[8];/* (tni + tcq)[8] */
    uint8_t			pent;
    union jtofu_path_coords	pcoords[5]; /* 3B x 5 = 15B */
    utofu_path_id_t		pathid;    /* default path id */
    utofu_vcq_id_t		vcqid;
};
#pragma pack()
#define TNIQ2TNI(a)	((a)>>4)
#define TNIQ2CQ(a)	((a)&0x0f)

struct utf_info {
    pmix_proc_t		pmix_proc[1];
    pmix_proc_t		pmix_wproc[1];
    pmix_info_t		*pmix_info;
    jtofu_job_id_t	jobid;
    int			myrank;
    int			mypid;
    int			nprocs;
    int			myppn;
    int			mynrnk;
    utofu_vcq_id_t	vcqid;
    utofu_vcq_hdl_t	vcqh;
    utofu_tni_id_t	tniid;
    uint64_t		*myfiaddr;
    struct tofu_vname	*vname;
    struct tni_info	*tinfo;
    struct cqsel_table	*cqseltab;	/* shared memory */
    int			cqselid;	/* shmem id */
    size_t		nnodes;
    union jtofu_phys_coords	*phys_node;	/* needs for PMIx_Resolve_peers */
    size_t              max_mtu;
    size_t              max_piggyback_size;
    size_t              max_edata_size;
    size_t		ntni;
    utofu_tni_id_t	tniids[TOFU_NIC_SIZE];
    utofu_vcq_hdl_t	vcqhs[TOFU_NIC_SIZE];
    utofu_vcq_id_t	vcqids[TOFU_NIC_SIZE];
    uint64_t		counter;
    void		*shmaddr;	/* user shmem area */
    int			shmid;		/* user shmem id */
};

extern struct utf_info utf_info;
extern struct tofu_vname *utf_get_peers(uint64_t **fi_addr, int *npp, int *ppnp, int *rnkp);

/* nrank is rank within node */
static inline void
utf_tni_select(int ppn, int nrnk, utofu_tni_id_t *tni, utofu_cq_id_t *cq)
{
    switch (ppn) {
    case 1: *tni = 0; if (cq) *cq = 0; break;
    case 2: /* 0, 6 */
	*tni = nrnk; if (cq) *cq = nrnk*6; break;
    case 3: /* 0, 4, 8 */
	*tni = nrnk; if (cq) *cq = nrnk*4; break;
    case 4: /* 0, 2, 5, 8 */
	*tni = nrnk;
	if (cq) {
	    switch(nrnk) {
	    case 0: *cq = 0; break;
	    case 1: *cq = 2; break;
	    case 2: *cq = 5; break;
	    default: /* this default statement eliminates compiler warning */
	    case 3: *cq = 8; break;
	    }
	}
	break;
    case 5: case 6: case 7: case 8:  /* 0, 2, 5, 8 */
	*tni = (nrnk%2)*3;
	if (cq) {
	    switch(nrnk/2) {
	    case 0: *cq = 0; break;
	    case 1: *cq = 2; break;
	    case 2: *cq = 5; break;
	    default: /* this default statement eliminates compiler warning */
	    case 3: *cq = 8; break;
	    }
	}
	break;
    case 9: case 10: case 11: case 12: /* 0, 2, 5, 8 */
	*tni = (nrnk%3)*2;
	if (cq) {
	    switch (nrnk/3) {
	    case 0: *cq = 0; break;
	    case 1: *cq = 2; break;
	    case 2: *cq = 5; break;
	    default: /* this default statement eliminates compiler warning */
	    case 3: *cq = 8; break;
	    }
	}
	break;
    default:
	*tni = nrnk%6;
	if (cq) {
	    if (ppn >= 13 && ppn <=24) { /* 0, 2, 5, 8 */
		switch (nrnk/6) {
		case 0: *cq = 0; break;
		case 1: *cq = 2; break;
		case 2: *cq = 5; break;
		default:
		case 3: *cq = 8; break;
		}
	    } else if (ppn >= 25 && ppn <= 30) { /* 0, 2, 5, 8, 10 */
		switch (nrnk/6) {
		case 0: *cq = 0; break;
		case 1: *cq = 2; break;
		case 2: *cq = 5; break;
		case 3: *cq = 8; break;
		default:
		case 4: *cq = 10; break;
		}
	    } else if (ppn >= 31 && ppn <= 36) { /* 0, 2, 5, 8, 9, 10 */
		switch (nrnk/6) {
		case 0: *cq = 0; break;
		case 1: *cq = 2; break;
		case 2: *cq = 5; break;
		case 3: *cq = 8; break;
		case 4: *cq = 9; break;
		default:
		case 5: *cq = 10; break;
		}
	    } else if (ppn >= 37 && ppn <= 42) { /* 0, 2, 5, 6, 8, 9, 10 */
		switch (nrnk/6) {
		case 0: *cq = 0; break;
		case 1: *cq = 2; break;
		case 2: *cq = 5; break;
		case 3: *cq = 6; break;
		case 4: *cq = 8; break;
		case 5: *cq = 9; break;
		default:
		case 6: *cq = 10; break;
		}
	    } else { /* ppn >= 43 && ppn <= 48 */
		if (nrnk <= 17) {
		    *cq = (nrnk/6)*2;
		} else if (nrnk <= 29) {
		    *cq = (nrnk/6) + 2;
		} else {
		    *cq = (nrnk/6) + 3;
		}
	    }
	}
    }
}

#if 0 /* before 2020/9/30 */
static inline void
utf_tni_select(int ppn, int nrnk, utofu_tni_id_t *tni, utofu_cq_id_t *cq)
{
    switch (ppn) {
    case 1: *tni = 0; if (cq) *cq = 0; break;
    case 2: /* 0, 6 */
	*tni = nrnk; if (cq) *cq = nrnk*6; break;
    case 3: /* 0, 3, 6 */
	*tni = nrnk; if (cq) *cq = nrnk*3; break;
    case 4: /* 0, 3, 6, 9 */
	*tni = nrnk; if (cq) *cq = nrnk*3; break;
    case 5: case 6: case 7: case 8:
	*tni = (nrnk%2)*3; if (cq) *cq = (nrnk/2)*3; break;
    case 9: case 10: case 11: case 12:
	*tni = (nrnk%3)*2; if (cq) *cq = (nrnk/3)*3; break;
    default:
	if (ppn >= 13 && ppn <=24) {
	    *tni = nrnk%6; if (cq) *cq = (nrnk/6)*3;
	} else if (ppn >= 25 && ppn <= 42) {
	    *tni = nrnk%6; if (cq) *cq = ((nrnk/6)*3)%10;
	} else {
	    *tni = nrnk%6; if (cq) *cq = ((nrnk/6)*3)%10;
	    if (cq && nrnk >= 42) *cq = 10;
	}
    }
}
#endif /* */

#define FMT_PHYS_COORDS	"%02d:%02d:%02d:%02d:%02d:%02d"

static inline char *
pcoords2string(union jtofu_phys_coords jcoords, char *buf, size_t len)
{
    static char	pbuf[50];
    size_t	ln;
    if (buf == NULL) {
	buf = pbuf;
	len = 50;
    }
    ln = snprintf(buf, sizeof(FMT_PHYS_COORDS) + 1, FMT_PHYS_COORDS,
		  jcoords.s.x, jcoords.s.y, jcoords.s.z,
		  jcoords.s.a, jcoords.s.b, jcoords.s.c);
    assert(ln <= len);
    return buf;
}

static inline char *
lcoords2string(union jtofu_log_coords coords, char *buf, size_t len)
{
    static char	pbuf[50];
    size_t	ln;
    if (buf == NULL) {
	buf = pbuf;
	len = 50;
    }
    ln = snprintf(buf, len, "%02d:%02d:%02d", coords.s.x, coords.s.y, coords.s.z);
    assert(ln <= len);
    return buf;
}

static inline char *
vcqh2string(utofu_vcq_hdl_t vcqh, char *buf, size_t len)
{
    static char	pbuf[128];
    int uc;
    utofu_vcq_id_t vcqi = -1UL;
    uint8_t	xyz[8];
    uint16_t	tni[1], tcq[1], cid[1];

    if (buf == NULL) {
	buf = pbuf;
	len = 128;
    }
    buf[0] = 0;
    if ((uc = utofu_query_vcq_id(vcqh, &vcqi)) != UTOFU_SUCCESS) goto bad;
    if ((uc = utofu_query_vcq_info(vcqi, xyz, tni, tcq, cid)) != UTOFU_SUCCESS) goto bad;
    snprintf(buf, len, "vcqh(%lx) vcqi(%lx) xyzabc(%02d:%02d:%02d:%02d:%02d:%02d) tni(%d) cq(%d)",
	     vcqh, vcqi, xyz[0], xyz[1], xyz[2], xyz[3], xyz[4], xyz[5],
	     tni[0], cid[0]);
bad:
    return buf;
}

#define MAKE_KEY(stadd, virt)	\
    (((stadd) & 0xfffffff000000000LL) | ((uint64_t)(virt)&0x0000000fffffffffLL))
#define CALC_STADD(key, virt)	\
    (((key) & 0xfffffff000000000LL) \
     | (((((uint64_t)(virt)&0x0000000fffffffffLL)) \
	 -  ((key)&0x0000000fffffffffLL)) + ((key)&0xffffLL)))
#define EXTRCT_STADD(key) ((uint64_t)(key)&0xfffffff00000ffffLL)
