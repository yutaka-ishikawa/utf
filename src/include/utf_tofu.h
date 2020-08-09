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

struct utf_info {
    pmix_proc_t		pmix_proc[1];
    pmix_proc_t		pmix_wproc[1];
    pmix_info_t		*pmix_info;
    jtofu_job_id_t	jobid;
    int			myrank;
    int			nprocs;
    int			myppn;
    int			mynrnk;
    utofu_vcq_id_t	vcqid;
    utofu_vcq_hdl_t	vcqh;
    utofu_tni_id_t	tniid;
    uint64_t		*myfiaddr;
    struct tofu_vname	*vname;
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

static inline char *
pcoords2string(union jtofu_phys_coords jcoords, char *buf, size_t len)
{
    static char	pbuf[50];
    if (buf == NULL) {
	buf = pbuf;
	len = 50;
    }
    snprintf(buf, len, "%02d:%02d:%02d:%02d:%02d:%02d",
	       jcoords.s.x, jcoords.s.y, jcoords.s.z,
	       jcoords.s.a, jcoords.s.b, jcoords.s.c);
    return buf;
}
