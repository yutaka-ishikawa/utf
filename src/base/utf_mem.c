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
int utf_dflag;
int utf_rflag;
int utf_initialized;
int utf_mode_msg;
int utf_mode_trans;
int utf_mode_mrail;

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
struct utf_msgreq MALGN(256)	utf_msgrq[REQ_SIZE];
struct utf_msglst MALGN(256)	utf_msglst[REQ_SIZE];
struct utf_rma_cq MALGN(256)	utf_rmacq_pool[COM_RMACQ_SIZE];


int	utf_tcq_count;
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
utfslist_t	utf_rget_proglst;
utfslist_t	utf_rmacq_waitlst;
utfslist_t	utf_rmacq_freelst;

/* stadd */
utofu_stadd_t	utf_egrmgt_stadd;	/* stadd of utf_egrmgt */
utofu_stadd_t	utf_egr_rbuf_stadd;	/* stadd of utf_egr_rbuf: packet buffer */
utofu_stadd_t	utf_egr_sbuf_stadd;	/* stadd of utf_egr_sbuf: packet buffer */
utofu_stadd_t	utf_sndctr_stadd;	/* stadd of utf_scntr */
utofu_stadd_t	utf_sndctr_stadd_end;	/* stadd of utf_scntr */

utofu_stadd_t
utf_mem_reg(utofu_vcq_hdl_t vcqh, void *buf, size_t size)
{
    utofu_stadd_t	stadd = 0;

    utf_tmr_begin(TMR_UTF_MEMREG);
    UTOFU_CALL(1, utofu_reg_mem, vcqh, buf, size, 0, &stadd);
    utf_printf("%s: size(%ld/0x%lx) vcqh(%lx) buf(%p) stadd(%lx)\n", __func__, size, size, vcqh, buf, stadd);
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
    utf_printf("%s: vcqh(%lx) stadd(%lx)\n", __func__, vcqh, stadd);
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
    utf_printf("%s: MEMORY utf_egr_sbuf[0] = %p utf_egr_sbuf[COM_SBUF_SIZE-1] = %p\n",
	       __func__, &utf_egr_sbuf[0], &utf_egr_sbuf[COM_SBUF_SIZE-1]);

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
    utf_printf("%s: MEMORY head(%p) utf_scntr[0] = %p utf_scntr[SND_CNTRL_MAX-1] = %p\n",
	       __func__, utf_scntr_freelst.head, &utf_scntr[0], &utf_scntr[SND_CNTRL_MAX-1]);
    /**/
    for (i = 0; i < RCV_CNTRL_MAX; i++) {
	utf_rcntr[i].state = R_NONE;
	utf_rcntr[i].mypos = i;
    }
    /**/
    memset(utf_msgrq, 0, sizeof(utf_msgrq));
    utfslist_init(&utf_msgreq_freelst, NULL);
    for (i = 0; i < REQ_SIZE; i++) {
	utfslist_append(&utf_msgreq_freelst, &utf_msgrq[i].slst);
    }

    utfslist_init(&utf_msglst_freelst, NULL);
    for (i = 0; i < REQ_SIZE; i++) {
	utfslist_append(&utf_msglst_freelst, &utf_msglst[i].slst);
	utf_msglst[i].reqidx = 0;
    }

    utfslist_init(&utf_rget_proglst, NULL);
    utf_tcq_count = 0;

    utfslist_init(&utf_rmacq_waitlst, NULL);
    utfslist_init(&utf_rmacq_freelst, NULL);
    for (i = 0; i < COM_RMACQ_SIZE; i++) {
	utfslist_append(&utf_rmacq_freelst, &utf_rmacq_pool[i].slst);
    }
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
    return ptr;
}

void
utf_free(void *ptr)
{
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
    utfslist_entry_t	*cur, *prev;
    int	found = 0;
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
	utf_printf("%s: no more RMA CQ entry\n", __func__);
	abort();
    }
    cq = container_of(slst, struct utf_rma_cq, slst);
    return cq;
}

void
utf_rmacq_free(struct utf_rma_cq *cq)
{
    utfslist_insert(&utf_rmacq_freelst, &cq->slst);
}


void
utf_mem_show()
{
    utf_printf("eager receive buffer addresses:\n"
	       " utf_egr_rbuf : 0x%lx (stadd: 0x%lx)"
	       " utf_egr_rbuf.rbuf[0] : 0x%lx (stadd: 0x%lx)"
	       " utf_egr_rbuf.rbuf[%d*126] : 0x%lx (stadd: 0x%lx)"
	       " utf_egr_rbuf.rbuf[%d*%d] : 0x%lx\n",
	       &utf_egr_rbuf, utf_egr_rbuf_stadd,
	       &utf_egr_rbuf.rbuf, utf_egr_rbuf_stadd,
	       COM_RBUF_SIZE, &utf_egr_rbuf.rbuf[COM_RBUF_SIZE*126],
	       utf_egr_rbuf_stadd + (uint64_t)&((struct utf_egr_rbuf*)0)->rbuf[COM_RBUF_SIZE*126],
	       COM_RBUF_SIZE, RCV_CNTRL_MAX, &utf_egr_rbuf.rbuf[COM_RBUF_SIZE*RCV_CNTRL_MAX]);
}

void
utf_debugdebug(struct utf_msgreq *req)
{
    req->ustatus = REQ_OVERRUN;
}
