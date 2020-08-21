#include <utofu.h>
#include "utf_conf.h"
#include "utf.h"
#include "utf_externs.h"
#include "utf_debug.h"
#include "utf_errmacros.h"
#include "utf_sndmgt.h"
#include "utf_queue.h"
#include "utf_tofu.h"
#include "utf_timer.h"
#include "utf_msgmacros.h"

extern void	utf_scntr_free(int idx);
extern void	utf_msgreq_free(struct utf_msgreq *req);
extern int	utf_send_start(struct utf_send_cntr *usp, struct utf_send_msginfo *minfo);

extern int	utf_tcq_count;
extern int	utf_mode_msg;

extern uint8_t		utf_rank2scntridx[PROC_MAX]; /* dest. rank to sender control index (sidx) */
extern utfslist_t		utf_egr_sbuf_freelst;
extern struct utf_egr_sbuf	utf_egr_sbuf[COM_SBUF_SIZE]; /* eager send buffer per rank */
extern utofu_stadd_t	utf_egr_sbuf_stadd;
extern utfslist_t		utf_scntr_freelst;
extern struct utf_send_cntr	utf_scntr[SND_CNTRL_MAX]; /* sender control */
extern struct utf_msgreq	utf_msgrq[REQ_SIZE];

static inline struct utf_egr_sbuf *
utf_egr_sbuf_alloc(utofu_stadd_t *stadd)
{
    utfslist_entry_t *slst = utfslist_remove(&utf_egr_sbuf_freelst);
    struct utf_egr_sbuf *uesp;
    int	pos;
    uesp = container_of(slst, struct utf_egr_sbuf, slst);
    if (!uesp) {
	utf_printf("%s: No more eager sender buffer\n", __func__);
	return NULL;
    }
    pos = uesp - utf_egr_sbuf;
    *stadd = utf_egr_sbuf_stadd + sizeof(struct utf_egr_sbuf)*pos;
    return uesp;
}


static inline int
utf_scntr_alloc(int dst, struct utf_send_cntr **uspp)
{
    int rc = UTF_SUCCESS;
    struct utf_send_cntr *scp;
    uint8_t	headpos;
    headpos = utf_rank2scntridx[dst];
    if (headpos != 0xff) {
	*uspp = &utf_scntr[headpos];
    } else {
	/* No head */
	utfslist_entry_t *slst = utfslist_remove(&utf_scntr_freelst);
	if (slst == NULL) {
	    rc = UTF_ERR_NOMORE_SNDCNTR;
	    goto err;
	}
	scp = container_of(slst, struct utf_send_cntr, slst);
	utf_rank2scntridx[dst]  = scp->mypos;
	scp->dst = dst;
	utfslist_init(&scp->smsginfo, NULL);
	scp->state = S_NONE;
	scp->flags = 0;
	scp->svcqid = utf_info.vcqid;
	scp->rvcqid = utf_info.vname[dst].vcqid;
	scp->mient = 0;
	DEBUG(DLEVEL_UTOFU) {
	    utf_printf("%s: dst(%d) flag_path(0x%lx)\n", __func__, dst, scp->flags);
	}
	*uspp = scp;
    }
err:
    return rc;
}

static inline void
minfo_setup(struct utf_send_msginfo *minfo, int rank, uint64_t tag, uint64_t size,
	    void *usrbuf, struct utf_egr_sbuf *sbufp,
	    struct utf_send_cntr *usp, struct utf_msgreq *req)
{
    minfo->msghdr.src  = utf_info.myrank;
    minfo->msghdr.tag  = tag;
    minfo->msghdr.size = size;
    minfo->msghdr.sidx = usp->mypos; /* 8 bit */
    minfo->mreq = req;
    sbufp->pkt.hdr.src  = rank;
    sbufp->pkt.hdr.tag  = tag;
    sbufp->pkt.hdr.hall = 0;	/* marker field is now 0 */
    sbufp->pkt.hdr.size = size;
    if (size <= MSG_EAGER_PIGBACK_SZ) {
	minfo->cntrtype = SNDCNTR_BUFFERED_EAGER_PIGBACK;
	memcpy(sbufp->pkt.pyld.msgdata, usrbuf, size);
	sbufp->pkt.hdr.pyldsz = size;
	req->type = REQ_SND_BUFFERED_EAGER;
    } else if (size <= MSG_EAGER_SIZE) {
	minfo->cntrtype = SNDCNTR_BUFFERED_EAGER;
	memcpy(sbufp->pkt.pyld.msgdata, usrbuf, size);
	sbufp->pkt.hdr.pyldsz = size;
	req->type = REQ_SND_BUFFERED_EAGER;
    } else if (utf_mode_msg != MSG_RENDEZOUS) {
	minfo->cntrtype = SNDCNTR_INPLACE_EAGER;
	minfo->usrbuf = usrbuf;
	memcpy(sbufp->pkt.pyld.msgdata, usrbuf, MSG_PYLDSZ);
	sbufp->pkt.hdr.pyldsz = MSG_PYLDSZ;
	req->type = REQ_SND_INPLACE_EAGER;
    } else {
	minfo->cntrtype = SNDCNTR_RENDEZOUS;
	minfo->rgetaddr.nent = 1;
	sbufp->pkt.pyld.rndzdata.vcqid[0]
	    = minfo->rgetaddr.vcqid[0] = usp->svcqid;
	sbufp->pkt.pyld.rndzdata.stadd[0]
	    = minfo->rgetaddr.stadd[0] = utf_mem_reg(utf_info.vcqh, usrbuf, size);
	sbufp->pkt.pyld.rndzdata.nent = 1;
	sbufp->pkt.hdr.pyldsz = MSG_RCNTRSZ;
	sbufp->pkt.hdr.rndz = MSG_RENDEZOUS;
	req->type = REQ_SND_RENDEZOUS;
    }
    minfo->sndbuf = sbufp;
    req->hdr = minfo->msghdr; /* copy the header */
    req->state = REQ_PRG_NORMAL;
}

void
utf_setmsgmode(int mode)
{
    utf_mode_msg = mode;
}

int
utf_send(void *buf, size_t size, int dst, uint64_t tag, UTF_reqid *ridx)
{
    int	rc = 0;
    struct utf_send_cntr *usp;
    struct utf_egr_sbuf	*sbufp;
    struct utf_msgreq	*req;
    struct utf_send_msginfo *minfo;
    struct utf_egr_sbuf	*sbuf;

    utf_tmr_begin(TMR_UTF_SEND_POST);
    if (dst < 0 || dst > utf_info.nprocs) {
	return -1;
    }
    if (utf_tcq_count) {
	/* progress */
	utf_tcqprogress();
    }
    if ((req = utf_msgreq_alloc()) == NULL) {
	rc = UTF_ERR_NOMORE_REQBUF;
	goto ext;
    }
    if((rc = utf_scntr_alloc(dst, &usp)) != UTF_SUCCESS) {
	goto err1;
    }
    /* wait for completion of previous send massages */
retry:
    minfo = &usp->msginfo[usp->mient];
    if (minfo->cntrtype != SNDCNTR_NONE) {
	utf_progress();
	goto retry;
    }
    usp->mient = (usp->mient + 1) % COM_SCNTR_MINF_SZ;
    sbuf = (minfo->sndbuf == NULL) ?
	utf_egr_sbuf_alloc(&minfo->sndstadd) : minfo->sndbuf;
    if (sbuf == NULL) {
	utf_printf("%s: usp(%p)->mient(%d)\n", __func__, usp, usp->mient);
	rc = UTF_ERR_NOMORE_SNDBUF;
	goto err2;
    }
    minfo_setup(minfo, utf_info.myrank, tag, size, buf, sbuf, usp, req);
    ridx->reqid1 = utf_msgreq2idx(req);
    if (usp->state == S_NONE) {
	utf_send_start(usp, minfo);
    }
ext:
    utf_tmr_end(TMR_UTF_SEND_POST);
    return rc;
err2:
    utf_scntr_free(dst);
err1:
    utf_msgreq_free(req);
    ridx->id = -1;
    ridx->reqid1 = -1;
    goto ext;
}

int
utf_recv(void *buf, size_t size, int src, int tag,  UTF_reqid *ridx)
{
    struct utf_msgreq	*req;
    int	idx;
    int	rc = 0;

    utf_tmr_begin(TMR_UTF_RECV_POST);
    if ((idx = utf_uexplst_match(src, tag, 0)) != -1) {
	int	cpsz;
	req = utf_idx2msgreq(idx);
	if (req->rndz) {  /* rendezvous message */
	    struct utf_recv_cntr *urp = req->rcntr;
	    req->buf = buf;
	    req->expsize = size;
	    rget_start(urp, req);
	} else if (req->state == REQ_DONE) {
	    cpsz = size < req->hdr.size ? size : req->hdr.size;
	    memcpy(buf, req->buf, cpsz);
	    /* Thi is unexpected message.
	     * This means the buffer is allocated dynamically */
	    utf_free(req->buf);
	    req->state = REQ_DONE;
	    ridx->id = 0;
	    ridx->reqid1 = utf_msgreq2idx(req);
	    rc = UTF_MSG_AVL;
	} else {
	    /* request type is changed and waiting for request done */
	    utf_printf("%s: Does this case work ?\n", __func__);
	    req->type = REQ_RECV_EXPECTED;
	    ridx->reqid1 = utf_msgreq2idx(req);
	}
    } else {
	if ((req = utf_msgreq_alloc()) == NULL) {
	    rc = UTF_ERR_NOMORE_REQBUF;
	    goto err;
	}
	req->hdr.src = src; req->hdr.tag = tag;
	/* req->hdr.size = size; req->hdr.size will be set at the message arrival */
	req->buf = buf;
	req->expsize = size;
	req->ustatus = REQ_NONE; req->state = REQ_PRG_NORMAL;
	req->type = REQ_RECV_EXPECTED;	req->rsize = 0;
	utf_msglst_append(&utf_explst, req);
	ridx->id = 0;
	ridx->reqid1 = utf_msgreq2idx(req);
	rc  = UTF_SUCCESS;
    }
err:
    utf_tmr_end(TMR_UTF_RECV_POST);
    return rc;
}

int
utf_sendrecv(void *sbuf, size_t ssize, int src, int stag,
	     void *rbuf, size_t rsize, int dst, int rtag, UTF_reqid *reqid)
{
    utf_printf("%s: not yet implemented\n", __func__);
    return 0;
}


int
utf_test(UTF_reqid reqid)
{
    struct utf_msgreq	*req;

    if (reqid.reqid1 < 0) return -1;

    utf_progress();
    req = utf_idx2msgreq(reqid.reqid1);
    if (req->state == REQ_DONE) return 0;
    return 1;
}

/*
 * utf_wait: wait for request completion
 *	in case of buffered message, wait for freeing buffer
 *	in the sendengine. the REQ_PRG_RECLAIM state.
 */
int
utf_wait(UTF_reqid reqid)
{
    volatile struct utf_msgreq	*req;
    int	ttp;

    utf_tmr_begin(TMR_UTF_WAIT);
    if (reqid.reqid1 < 0) return -1;

    req = utf_idx2msgreq(reqid.reqid1);
    if (req->type == REQ_SND_BUFFERED_EAGER) {
	if (req->state != REQ_DONE) {
	    /* still in-progress */
	    req->state = REQ_PRG_RECLAIM;
	    goto skip;
	}
    } else {
	if (req->type == REQ_RECV_EXPECTED) {
	    utf_tmr_begin(TMR_UTF_RCVPROGRESS);
	}
	while (req->state != REQ_DONE) {
	    utf_progress();
	}
    }
    utf_msgreq_free((struct utf_msgreq*) req); /* state is reset to REQ_NONE */
skip:
    utf_tmr_end(TMR_UTF_WAIT);
    return 0;
}

/*
 * utf_waitcmpl: wait for request completion
 *	Unlike utf_wait, the buffered message is also
 *	the same procedure, i.e., wait for completion.
 */
int
utf_waitcmpl(UTF_reqid reqid)
{
    volatile struct utf_msgreq	*req;
    int	ttp;

    utf_tmr_begin(TMR_UTF_WAIT);
    if (reqid.reqid1 < 0) return -1;

    req = utf_idx2msgreq(reqid.reqid1);
    if (req->state == REQ_NONE) {
	/* its already done */
	goto skip;
    }
    if (req->type == REQ_RECV_EXPECTED) {
	utf_tmr_begin(TMR_UTF_RCVPROGRESS);
    }
    while (req->state != REQ_DONE
	   && req->state != REQ_NONE) {
	utf_progress();
    }
    if (req->state == REQ_DONE) {
	/* In case of REQ_SND_BUFFERED_EAGER, the request object has been freed
	 * in the sendending.
	 */
	utf_msgreq_free((struct utf_msgreq*) req);
    }
skip:
    utf_tmr_end(TMR_UTF_WAIT);
    return 0;
}

void
utf_req_wipe()
{
    struct utf_msgreq	*req;
    UTF_reqid	reqid;
    int	id;
    int	i, rc;

    for (i = 0; i < REQ_SIZE; i++) {
	switch (utf_msgrq[i].state) {
	case REQ_NONE: continue;
	case REQ_DONE:
	    id = utf_msgreq2idx(&utf_msgrq[i]);
	    utf_printf("Warning: the user code does not wait for request completion. reqid=%d\n",
		       id);
	    break;
	default:
	    id = utf_msgreq2idx(&utf_msgrq[i]);
	    utf_printf("Warning: Still posted requests are being handled. reqid=%d state(%d\n",
		       id, utf_msgrq[i].state);
	}
	reqid.id = 0;
	reqid.reqid1 = id;
	rc = utf_waitcmpl(reqid);
	utf_printf("\tDone\n");
    }
}

#if 0
inline void
debug_show_rbuf(struct utf_msgreq *req)
{
    utf_printf("%s: reqid(%d) req(%p) cntr(%d)\n", __func__, reqid, req, utf_egr_rbuf.head.cntr);
    if (req->type == REQ_RECV_EXPECTED) {
	int j;
	for (j = utf_egr_rbuf.head.cntr + 1; j <= RCV_CNTRL_MAX; j++) {
	    struct utf_recv_cntr	*urp = &utf_rcntr[j];
	    struct utf_packet	*msgbase = utf_recvbuf_get(j);
	    struct utf_packet	*pktp;
	    int	sidx;
	    pktp = msgbase + urp->recvidx;
	    utf_printf("%s: RECVBUFCHECK urp(%p)->recvidx = %d pktp(%p)->marker = 0x%x\n",
		       __func__, urp, urp->recvidx, pktp, pktp->marker);
	}
    }
}
#endif /* 0 */
