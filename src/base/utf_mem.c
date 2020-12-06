#include <stdlib.h>
#include <malloc.h>
#include <utofu.h>
#include "utf_conf.h"
#include "utf_list.h"
#include "utf_sndmgt.h"
#include "utf_debug.h"
#include "utf_queue.h"
#include "utf_externs.h"
#include "utf_errmacros.h"
#include "utf_tofu.h"
#include "utf_timer.h"

int utf_tcq_count, utf_mrq_count;
int utf_sreq_count, utf_rreq_count;
int utf_dflag;
int utf_rflag;
int utf_initialized;
int utf_mode_msg;
int utf_mode_trans;
int utf_mode_mrail;
int utf_rma_cq_cnt;

/*
 *	utf_egrmgt:       1863 kiB (sizeof(utf_egrmgt) * PROC_MAX = 3 * 635904)
 *	utf_egr_rbuf     51200.256 kiB (COM_RBUF_SIZE*COM_PEERS+0.256 = (512*1024)*100)
 *	utf_egr_sbuf:      256 KiB (512*4*128)
 *	utf_rank2scntridx: 621 KiB (663552)
 *	utf_scntr:	    XX KiB (500*128)
 *	utf_rcntr:
 */
#define MALGN(sz) __attribute__ ((aligned(sz)))
uint8_t MALGN(256)	utf_rank2scntridx[PROC_MAX]; /* dest. rank to index of sender control (sidx) */
/* stag area */
utf_sndmgt_t MALGN(256)	utf_egrmgt[PROC_MAX];	/* remote info of eager receiver buffer */
struct utf_egr_rbuf MALGN(256)	utf_egr_rbuf;		/* eager receive buffer */
struct utf_egr_sbuf MALGN(256)	utf_egr_sbuf[COM_SBUF_SIZE]; /* eager send buffer per rank */
struct utf_send_cntr MALGN(256)	utf_scntr[SND_CNTRL_MAX]; /* sender control */
/**/
struct utf_recv_cntr MALGN(256)	utf_rcntr[RCV_CNTRL_MAX]; /* receiver control */
struct utf_msgreq MALGN(256)	utf_msgrq[MSGREQ_SIZE];
struct utf_msglst MALGN(256)	utf_msglst[MSGLST_SIZE];
struct utf_rma_cq MALGN(256)	utf_rmacq_pool[COM_RMACQ_SIZE];
struct utf_send_msginfo MALGN(256) utf_rndz_pool[MSGREQ_SEND_SZ];	/* used for rendezvous at sender */


utfslist_t	utf_explst;	/* expected message list */
utfslist_t	utf_uexplst;	/* unexpected message list */
utfslist_t	tfi_tag_explst;	/* fi: expected tagged message list */
utfslist_t	tfi_tag_uexplst;/* fi: unexpected tagged message list */
utfslist_t	tfi_msg_explst;	/* fi: expected message list */
utfslist_t	tfi_msg_uexplst;/* fi: unexpected message list */

utfslist_t	utf_egr_sbuf_freelst;	/* free list of utf_egr_sbuf */
utfslist_t	utf_scntr_freelst;	/* free list of utf_scntr */
utfslist_t	utf_msgreq_freelst;	/* free list of utf_msgrq */
utfslist_t	utf_msglst_freelst;
utfslist_t	utf_rndz_freelst;
utfslist_t	utf_rget_proglst;
utfslist_t	utf_rndz_proglst;
utfslist_t	utf_rmacq_waitlst;
utfslist_t	utf_rmacq_freelst;

/* stadd */
utofu_stadd_t	utf_egrmgt_stadd;	/* stadd of utf_egrmgt */
utofu_stadd_t	utf_egr_rbuf_stadd;	/* stadd of utf_egr_rbuf: packet buffer */
utofu_stadd_t	utf_egr_sbuf_stadd;	/* stadd of utf_egr_sbuf: packet buffer */
utofu_stadd_t	utf_sndctr_stadd;	/* stadd of utf_scntr */
utofu_stadd_t	utf_sndctr_stadd_end;	/* stadd of utf_scntr */
utofu_stadd_t	utf_rcntr_stadd;	/* stadd of utf_rcntr */
utofu_stadd_t	utf_rndz_stadd;		/* stadd of utf_rndz_freelst */
utofu_stadd_t	utf_rndz_stadd_end;	/* stadd of utf_rndz_freelst */
utofu_stadd_t	utf_rmacq_stadd;	/* rmacq for injectdata */

/**/
uint8_t	utf_zero256[256];

utofu_stadd_t
utf_mem_reg(utofu_vcq_hdl_t vcqh, void *buf, size_t size)
{
    utofu_stadd_t	stadd = 0;

    utf_tmr_begin(TMR_UTF_MEMREG);
    UTOFU_CALL(1, utofu_reg_mem, vcqh, buf, size, 0, &stadd);
    DEBUG(DLEVEL_PROTO_RMA|DLEVEL_ADHOC) {
	utf_printf("%s: size(%ld/0x%lx) vcqh(%lx) buf(%p) stadd(%lx)\n", __func__, size, size, vcqh, buf, stadd);
    }
    utf_tmr_end(TMR_UTF_MEMREG);
    return stadd;
}

void
utf_mem_dereg(utofu_vcq_id_t vcqh, utofu_stadd_t stadd)
{
    utf_tmr_begin(TMR_UTF_MEMDEREG);
    UTOFU_CALL(1, utofu_dereg_mem, vcqh, stadd, 0);
    DEBUG(DLEVEL_PROTO_RMA|DLEVEL_ADHOC) {
	utf_printf("%s: vcqh(%lx) stadd(%lx)\n", __func__, vcqh, stadd);
    }
    utf_tmr_end(TMR_UTF_MEMDEREG);
    return;
}


void
utf_mem_init()
{
    int	i;

    utfslist_init(&utf_explst, NULL);
    utfslist_init(&utf_uexplst, NULL);

    memset(utf_egrmgt, 0, sizeof(utf_egrmgt));
    UTOFU_CALL(1, utofu_reg_mem_with_stag, utf_info.vcqh, (void*) utf_egrmgt,
	       sizeof(utf_egrmgt), STAG_EGRMGT, 0, &utf_egrmgt_stadd);
    /* Initializing -1 because marker is zero */
    memset(&utf_egr_rbuf, -1, sizeof(utf_egr_rbuf));
    UTOFU_CALL(1, utofu_reg_mem_with_stag, utf_info.vcqh, (void*) &utf_egr_rbuf,
	       sizeof(utf_egr_rbuf), STAG_RBUF, 0, &utf_egr_rbuf_stadd);
    utf_egr_rbuf.head.cntr = RCV_CNTRL_INIT;
    utf_egr_rbuf.head.chntail.rank = -1;
    utf_egr_rbuf.head.chntail.sidx = -1;
    utf_egr_rbuf.head.chntail.recvidx = 0;

    /**/
    memset(&utf_egr_sbuf, 0, sizeof(utf_egr_sbuf));
    utfslist_init(&utf_egr_sbuf_freelst, NULL);
    for (i = 0; i < COM_SBUF_SIZE; i++) {
	utfslist_append(&utf_egr_sbuf_freelst, &utf_egr_sbuf[i].slst);
    }
    DEBUG(DLEVEL_INIFIN) {
	utf_printf("%s: MEMORY utf_egr_sbuf[0] = %p utf_egr_sbuf[COM_SBUF_SIZE-1] = %p\n",
		   __func__, &utf_egr_sbuf[0], &utf_egr_sbuf[COM_SBUF_SIZE-1]);
    }

    UTOFU_CALL(1, utofu_reg_mem_with_stag, utf_info.vcqh, (void*) utf_egr_sbuf,
	       sizeof(utf_egr_sbuf), STAG_SBUF, 0, &utf_egr_sbuf_stadd);
    /**/
    for(i = 0; i < PROC_MAX; i++) {
	utf_rank2scntridx[i] = 0xff;
    }
    /**/
    memset(utf_scntr, 0, sizeof(utf_scntr));
    UTOFU_CALL(1, utofu_reg_mem_with_stag, utf_info.vcqh, (void*) utf_scntr,
	       sizeof(utf_scntr), STAG_SNDCTR, 0, &utf_sndctr_stadd);
    utfslist_init(&utf_scntr_freelst, NULL);
    for (i = 0; i < SND_CNTRL_MAX; i++) {
	utfslist_append(&utf_scntr_freelst, &utf_scntr[i].slst);
	utf_scntr[i].mypos = i;
	utf_scntr[i].chn_next.rank = RANK_ALL1;
    }
    utf_sndctr_stadd_end = (uint64_t) utf_sndctr_stadd + sizeof(utf_scntr);
    DEBUG(DLEVEL_INIFIN) {
	utf_printf("%s: MEMORY head(%p) utf_scntr[0] = %p utf_scntr[SND_CNTRL_MAX-1] = %p\n",
		   __func__, utf_scntr_freelst.head, &utf_scntr[0], &utf_scntr[SND_CNTRL_MAX-1]);
    }
    /**/
    for (i = 0; i < RCV_CNTRL_MAX; i++) {
	utf_rcntr[i].state = R_NONE;
	utf_rcntr[i].mypos = i;
    }
    /**/
    memset(utf_msgrq, 0, sizeof(utf_msgrq));
    utfslist_init(&utf_msgreq_freelst, NULL);
    for (i = 0; i < MSGREQ_SIZE; i++) {
	utfslist_append(&utf_msgreq_freelst, &utf_msgrq[i].slst);
    }

    utfslist_init(&utf_msglst_freelst, NULL);
    for (i = 0; i < MSGLST_SIZE; i++) {
	utfslist_append(&utf_msglst_freelst, &utf_msglst[i].slst);
	utf_msglst[i].reqidx = 0;
    }

    /* rendezvous */
    memset(utf_rndz_pool, 0, sizeof(utf_rndz_pool));
    UTOFU_CALL(1, utofu_reg_mem_with_stag, utf_info.vcqh, (void*) utf_rndz_pool,
	       sizeof(utf_rndz_pool), STAG_RNDV, 0, &utf_rndz_stadd);
    utf_rndz_stadd_end = utf_rndz_stadd + sizeof(utf_rndz_pool);
    utfslist_init(&utf_rget_proglst, NULL);
    utfslist_init(&utf_rndz_proglst, NULL);
    utfslist_init(&utf_rndz_freelst, NULL);
    for (i = 0; i < MSGREQ_SEND_SZ; i++) {                                             
        utfslist_append(&utf_rndz_freelst, &utf_rndz_pool[i].slst);
	utf_rndz_pool[i].mypos = i;
	DEBUG(DLEVEL_INIFIN) {
	    utf_printf("%s: utf_rndz_pool[%d]=%p mypos(%d)\n", __func__, i, &utf_rndz_pool[i], utf_rndz_pool[i].mypos);
	}
    } 
    utf_tcq_count = 0;

    /* rma */
    UTOFU_CALL(1, utofu_reg_mem_with_stag, utf_info.vcqh, (void*) utf_rmacq_pool,
	       sizeof(utf_rmacq_pool), STAG_RMACQ, 0, &utf_rmacq_stadd);
    utfslist_init(&utf_rmacq_waitlst, NULL);
    utfslist_init(&utf_rmacq_freelst, NULL);
    for (i = 0; i < COM_RMACQ_SIZE; i++) {
	utfslist_append(&utf_rmacq_freelst, &utf_rmacq_pool[i].slst);
	utf_rmacq_pool[i].mypos = i;
    }
    utf_rma_cq_cnt = COM_RMACQ_SIZE;

    memset(utf_zero256, 0, sizeof(utf_zero256));
    UTOFU_CALL(1, utofu_reg_mem_with_stag, utf_info.vcqh, (void*) utf_rcntr,
	       sizeof(utf_rcntr), STAG_RCVCTR, 0, &utf_rcntr_stadd);
}

void *
utf_malloc(size_t sz)
{
    void	*ptr;
    ptr = malloc(sz);
    if (ptr == NULL) {
	utf_printf("%s: cannot allocate memory size(%ld)\n", __func__, sz);
	abort();
    }
    DEBUG(DLEVEL_MEMORY) {
	utf_printf("[MEMORY] %s: addr(%p) size(%lx)\n", __func__, ptr, sz);
    }
    return ptr;
}

void
utf_free(void *ptr)
{
    DEBUG(DLEVEL_MEMORY) {
	utf_printf("[MEMORY] %s: addr(%p)\n", __func__, ptr);
    }
    free(ptr);
}

void
utf_mem_finalize()
{
    UTOFU_CALL(1, utofu_dereg_mem, utf_info.vcqh, utf_egrmgt_stadd, 0);
    UTOFU_CALL(1, utofu_dereg_mem, utf_info.vcqh, utf_egr_rbuf_stadd, 0);
    UTOFU_CALL(1, utofu_dereg_mem, utf_info.vcqh, utf_egr_sbuf_stadd, 0);
    UTOFU_CALL(1, utofu_dereg_mem, utf_info.vcqh, utf_sndctr_stadd, 0);
}

void
utf_scntr_free(int idx)
{
    struct utf_send_cntr *head;
    uint16_t	headpos;
    headpos = utf_rank2scntridx[idx];
    if (headpos != (uint16_t) -1) {
	head = &utf_scntr[headpos];
	utfslist_insert(&utf_scntr_freelst, &head->slst);
	utf_rank2scntridx[idx] = -1;
    } else {
	abort();
    }
}

struct utf_rma_cq *
utf_rmacq_alloc()
{
    utfslist_entry_t *slst = utfslist_remove(&utf_rmacq_freelst);
    struct utf_rma_cq *cq;
    if (slst == NULL) {
	DEBUG(DLEVEL_WARN) {
	    utf_printf("%s: WARNING no more RMA CQ entry\n", __func__);
	}
	return NULL;
    }
    cq = container_of(slst, struct utf_rma_cq, slst);
    --utf_rma_cq_cnt;
    //utf_printf("%s: YI++++++ cnt=%d cq=%p\n", __func__, utf_rma_cq_cnt, cq);
    return cq;
}

void
utf_rmacq_free(struct utf_rma_cq *cq)
{
    utf_rma_cq_cnt++;
    //utf_printf("%s: YI------ cnt=%d cq=%p\n", __func__, utf_rma_cq_cnt, cq);
    utfslist_insert(&utf_rmacq_freelst, &cq->slst);
}


void
utf_mem_show(FILE *fp)
{
    int	i;

    fprintf(fp, "eager receive buffer addresses:\n"
	    " utf_egr_rbuf : %p (stadd: 0x%lx)"
	    " utf_egr_rbuf.rbuf[0] : %p (stadd: 0x%lx)"
	    " utf_egr_rbuf.rbuf[%d*126] : %p (stadd: 0x%lx)"
	    " utf_egr_rbuf.rbuf[%d*%d] : %p\n",
	    &utf_egr_rbuf, utf_egr_rbuf_stadd,
	    &utf_egr_rbuf.rbuf, utf_egr_rbuf_stadd,
	    COM_RBUF_SIZE, &utf_egr_rbuf.rbuf[COM_RBUF_SIZE*126],
	    utf_egr_rbuf_stadd + (uint64_t)&((struct utf_egr_rbuf*)0)->rbuf[COM_RBUF_SIZE*126],
	    COM_RBUF_SIZE, RCV_CNTRL_MAX, &utf_egr_rbuf.rbuf[COM_RBUF_SIZE*RCV_CNTRL_MAX]);
    fprintf(fp, "receiver control utf_rcntr: utf_rcntr_stadd(0x%lx)\n", utf_rcntr_stadd);
    for (i = 0; i < 4; i++) {
	fprintf(fp, "[%d]:%p, ", 126 - i, &utf_rcntr[126 - i]);
    }
    fprintf(fp, "\n");
}


void
utf_egrrbuf_show(FILE *fp)
{
    int	i;
    fprintf(fp, "PEERS(egr_rbuf):");
    for (i = 0; i < COM_PEERS; i++) {
	struct utf_packet	*pkt = &utf_egr_rbuf.rbuf[COM_RBUF_SIZE*i];
	if ((i % 8) == 0) fprintf(stderr, "\n\t");
	fprintf(fp, " SRC(%d)", pkt->hdr.src);
    }
    fprintf(fp, "\n");
}


void
utf_debugdebug(struct utf_msgreq *req)
{
    req->reclaim = 1;
}

void
utf_nullfunc()
{
    utf_info.counter++;
}


char *
utf_pkt_getinfo(struct utf_packet *pkt, int *mrkr, int *sidx)
{

    static char	pbuf[256];
#if 0
    snprintf(pbuf, 256, "src(%d) tag(0x%lx) size(%ld) pyldsz(%d) rndz(%d) flgs(0x%x) mrker(%d) sidx(%d)",
	     pkt->hdr.src, pkt->hdr.tag, (uint64_t) pkt->hdr.size,
	     pkt->hdr.pyldsz, pkt->hdr.rndz, pkt->hdr.flgs, pkt->hdr.marker, pkt->hdr.sidx);
#endif
    *mrkr = pkt->hdr.marker;
    *sidx = pkt->hdr.sidx;
    return pbuf;
}

void
utf_egrbuf_show(FILE *fp)
{
    fprintf(fp, "EAGER RECEIVE BUFFER INFO\n");
    fprintf(fp, "\tCNTR = %ld\n"
	       "\t&utf_egr_buf[0] = %p\n"
	       "\t&utf_rcntr[0] = %p\n",
	       utf_egr_rbuf.head.cntr, &utf_egr_rbuf.rbuf[0], &utf_rcntr[0]);
}
