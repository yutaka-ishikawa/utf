/*
 *
 */
extern struct utf_msgreq	utf_msgrq[REQ_SIZE];
extern utfslist_t		utf_msgreq_freelst;
extern utfslist_t		utf_egr_sbuf_freelst;
extern utofu_stadd_t		utf_sndctr_stadd;	/* stadd of utf_scntr */
extern struct utf_egr_rbuf	utf_egr_rbuf;		/* eager receive buffer */
extern struct utf_recv_cntr	utf_rcntr[RCV_CNTRL_MAX]; /* receiver control */
extern utfslist_t	utf_rget_proglst;
extern int     utf_tcq_count, utf_mrq_count;

extern void	utf_tcqprogress();
extern int	utf_mrqprogress();
//extern void	utf_recvengine(struct utf_recv_cntr *urp, struct utf_packet *pkt, int sidx);

static inline struct utf_msgreq *
utf_idx2msgreq(uint32_t idx)
{
    return &utf_msgrq[idx];
}

static inline int
utf_msgreq2idx(struct utf_msgreq *req)
{
    return req - utf_msgrq;
}

static inline struct utf_msgreq 
*utf_msgreq_alloc()
{
    utfslist_entry_t *slst = utfslist_remove(&utf_msgreq_freelst);
    if (slst != 0) {
	return container_of(slst, struct utf_msgreq, slst);
    } else {
	utf_printf("%s: No more request object\n", __func__);
	return NULL;
    }
}

static inline void
utf_msgreq_free(struct utf_msgreq *req)
{
    req->state = REQ_NONE;
    utfslist_insert(&utf_msgreq_freelst, &req->slst);
}

static inline struct utf_msglst *
utf_msglst_alloc()
{
    utfslist_entry_t *slst = utfslist_remove(&utf_msglst_freelst);
    struct utf_msglst	*mp;

    if (slst != NULL) {
	mp = container_of(slst, struct utf_msglst, slst);
	return mp;
    } else {
	utf_printf("%s: No more msglst\n", __func__);
	abort();
	return NULL;
    }
}

/* void utf_msglst_free(struct utf_msglst *msl) is defined in utf_queue.h */

static inline struct utf_msglst *
utf_msglst_insert(struct utfslist *head, struct utf_msgreq *req)
{
    struct utf_msglst	*mlst;

    mlst = utf_msglst_alloc();
    mlst->hdr = req->hdr;
    mlst->reqidx = utf_msgreq2idx(req);
    utfslist_append(head, &mlst->slst);
    return mlst;
}

static inline int
utf_remote_add(utofu_vcq_hdl_t vcqh,
	       utofu_vcq_id_t rvcqid, unsigned long flgs,
	       uint64_t val, utofu_stadd_t rstadd,
	       uint64_t edata, void *cbdata)
{
    flgs |= UTOFU_ONESIDED_FLAG_LOCAL_MRQ_NOTICE
	 | UTOFU_ONESIDED_FLAG_STRONG_ORDER;
    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("remote_add: val(0x%lx) rvcqid(0x%lx)\n", val, rvcqid);
    }
    UTOFU_CALL(1, utofu_armw8,
	       vcqh, rvcqid,
	       UTOFU_ARMW_OP_ADD,
	       val, rstadd, edata, flgs, cbdata);
//    utf_mrq_count++;
    return 0;
}

static inline int
utf_remote_armw4(utofu_vcq_hdl_t vcqh,
		 utofu_vcq_id_t rvcqid, unsigned long flgs,
		 enum utofu_armw_op op, uint64_t val,
		 utofu_stadd_t rstadd,
		 uint64_t edata, void *cbdata)
{
    /* local mrq notification is supressed */
    flgs |= UTOFU_ONESIDED_FLAG_REMOTE_MRQ_NOTICE
	| UTOFU_ONESIDED_FLAG_STRONG_ORDER;
    /* | UTOFU_MRQ_TYPE_LCL_ARMW; remote notification */
    UTOFU_CALL(1, utofu_armw4, vcqh, rvcqid, op,
	       val, rstadd, edata, flgs, cbdata);
    return 0;
}

static inline int
remote_piggysend(utofu_vcq_hdl_t vcqh,
		 utofu_vcq_id_t rvcqid, void *data,  utofu_stadd_t rstadd,
		 size_t len, uint64_t edata, unsigned long flgs, void *cbdata)
{
    size_t	sz;

    flgs |= UTOFU_ONESIDED_FLAG_TCQ_NOTICE
	 | UTOFU_ONESIDED_FLAG_CACHE_INJECTION
	 | UTOFU_ONESIDED_FLAG_PADDING
	 | UTOFU_ONESIDED_FLAG_STRONG_ORDER;
    UTOFU_CALL(1, utofu_put_piggyback,
	       vcqh, rvcqid, data, rstadd, len, edata, flgs, cbdata);
    utf_tcq_count++;
    return 0;
}

/*
 * TCQ_NOTICE event driven is employed in the current implementation
 */
static inline int
remote_put(utofu_vcq_hdl_t vcqh,
	   utofu_vcq_id_t rvcqid, utofu_stadd_t lstadd,
	   utofu_stadd_t rstadd, size_t len, uint64_t edata,
	   unsigned long flgs, void *cbdata)
{
    size_t	sz;

    flgs |= UTOFU_ONESIDED_FLAG_TCQ_NOTICE
	 | UTOFU_ONESIDED_FLAG_STRONG_ORDER
	 | UTOFU_ONESIDED_FLAG_CACHE_INJECTION;
    UTOFU_CALL(1, utofu_put,
	       vcqh, rvcqid,  lstadd, rstadd, len, edata, flgs, cbdata);
    utf_tcq_count++;
    return 0;
}


static inline int
remote_get(utofu_vcq_hdl_t vcqh,
	   utofu_vcq_id_t rvcqid, utofu_stadd_t lstadd,
	   utofu_stadd_t rstadd, size_t len, uint64_t edata,
	   unsigned long flgs, void *cbdata)
{

    flgs |= UTOFU_ONESIDED_FLAG_LOCAL_MRQ_NOTICE
	 | UTOFU_ONESIDED_FLAG_STRONG_ORDER;
    UTOFU_CALL(1, utofu_get,
	       vcqh, rvcqid,  lstadd, rstadd, len, edata, flgs, cbdata);
//    utf_mrq_count++;
    return 0;
}

static inline int
utf_remote_swap(utofu_vcq_hdl_t vcqh,
		utofu_vcq_id_t rvcqid, unsigned long flgs, uint64_t val,
		utofu_stadd_t rstadd, uint64_t edata, void *cbdata)
{
    utf_printf("%s: NOT YET\n", __func__); abort();
}

static inline int
utf_remote_cswap(utofu_vcq_hdl_t vcqh,
		 utofu_vcq_id_t rvcqid, unsigned long flgs,
		 uint64_t old_val, uint64_t new_val,
		 utofu_stadd_t rstadd, uint64_t edata, void *cbdata)
{
    utf_printf("%s: NOT YET\n", __func__); abort();
}

static inline struct utf_packet	*
utf_recvbuf_get(int idx)
{
    struct utf_packet	 *bp = &utf_egr_rbuf.rbuf[COM_RBUF_SIZE*idx];
    return bp;
}

static inline void
utf_egr_sbuf_free(struct utf_egr_sbuf *uesp)
{
    utfslist_insert(&utf_egr_sbuf_freelst, &uesp->slst);
}

static inline int
eager_copy_and_check(struct utf_recv_cntr *urp,
		     struct utf_msgreq *req, struct utf_packet *pkt)
{
    size_t	cpysz;

    cpysz = PKT_PYLDSZ(pkt);
    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("%s: req->rsize(%ld) req->hdr.size(%ld) cpysz(%ld) "
		 "EMSG_SIZE(msgp)=%ld\n",
		 __func__, req->rsize, req->hdr.size, cpysz, PKT_PYLDSZ(pkt));
    }
    memcpy(&req->buf[req->rsize], PKT_DATA(pkt), cpysz);
    req->rsize += cpysz;
    if (req->hdr.size == req->rsize) {
	req->state = REQ_DONE;
	urp->state = R_DONE;
    } else {
	/* More data will arrive */
	urp->state = R_BODY;
    }
    return urp->state;
}

static char *rstate_symbol[] =
{	"R_FREE", "R_NONE", "R_HEAD", "R_BODY",	"R_WAIT_RNDZ",
	"R_DO_RNDZ", "R_DO_READ", "R_DO_WRITE", "R_DONE" };


static inline void
rget_start(struct utf_recv_cntr *urp, struct utf_msgreq *req)
{
    int	sidx = req->hdr.sidx;

    /* req->hdr.size is defined by the sender
     * req->expsize is defined by the receiver */
    urp->state = R_DO_RNDZ;
    req->bufinfo.stadd[0] = utf_mem_reg(utf_info.vcqh, req->buf, req->expsize);
    utfslist_append(&utf_rget_proglst, &urp->rget_slst);
    DEBUG(DLEVEL_PROTO_RENDEZOUS) {
	utf_printf("%s: Receiving Rendezous reqsize(0x%lx) "
		   "rvcqid(0x%lx) lcl_stadd(0x%lx) rmt_stadd(0x%lx) "
		   "sidx(%d) mypos(%d)\n",
		   __func__, req->expsize,
		   req->rgetsender.vcqid[0], req->bufinfo.stadd[0],
		   req->rgetsender.stadd[0], sidx, urp->mypos);
    }
    req->rsize =
	(req->expsize > TOFU_RMA_MAXSZ) ? TOFU_RMA_MAXSZ : req->expsize;
    remote_get(utf_info.vcqh, req->rgetsender.vcqid[0], req->bufinfo.stadd[0],
	       req->rgetsender.stadd[0], req->rsize, sidx, 0, 0);
}

static inline void
rget_continue(struct utf_recv_cntr *urp, struct utf_msgreq *req)
{
    int	sidx = req->hdr.sidx;
    size_t	restsz, transsz;

    /* req->hdr.size is defined by the sender
     * req->expsize is defined by the receiver */
    utfslist_append(&utf_rget_proglst, &urp->rget_slst);
    DEBUG(DLEVEL_PROTO_RENDEZOUS) {
	utf_printf("%s: Continueing Rendezous reqsize(0x%lx) "
		   "rvcqid(0x%lx) lcl_stadd(0x%lx) rmt_stadd(0x%lx) "
		   "sidx(%d) mypos(%d)\n",
		   __func__, req->expsize,
		   req->rgetsender.vcqid[0], req->bufinfo.stadd[0],
		   req->rgetsender.stadd[0], sidx, urp->mypos);
    }
    restsz = req->expsize - req->rsize;
    transsz = (restsz > TOFU_RMA_MAXSZ) ? TOFU_RMA_MAXSZ : restsz;
    remote_get(utf_info.vcqh, req->rgetsender.vcqid[0], req->bufinfo.stadd[0] + req->rsize,
	       req->rgetsender.stadd[0] + req->rsize, transsz, sidx, 0, 0);
    req->rsize += transsz;
}

/*
 * receiver engine
 */
static inline void
utf_recvengine(struct utf_recv_cntr *urp, struct utf_packet *pkt, int sidx)
{
    struct utf_msgreq	*req;

    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("%s: urp(%p)->state(%s)\n",
		 __func__, urp, rstate_symbol[urp->state]);
    }
    switch (urp->state) {
    case R_NONE: /* Begin receiving message */
    {
	int	idx;
	if ((idx = utf_explst_match(PKT_MSGSRC(pkt), PKT_MSGTAG(pkt), 1)) != -1) {
	    req = utf_idx2msgreq(idx);
	    req->rndz = pkt->hdr.rndz;
	    if (req->rndz) {
		memcpy(&req->rgetsender, &pkt->pyld.rndzdata, sizeof(struct utf_vcqid_stadd));
		req->hdr = pkt->hdr;
		req->rcntr = urp;
		rget_start(urp, req); /* state is changed to R_DO_RNDZ */
	    } else { /* eager */
		if (eager_copy_and_check(urp, req, pkt) == R_DONE) goto done;
		req->hdr = pkt->hdr;
		urp->req = req;
	    }
	} else { /* New Unexpected message */
	    req = utf_msgreq_alloc();
	    req->hdr = pkt->hdr;
	    req->rsize = 0; req->ustatus = 0; req->type = REQ_RECV_UNEXPECTED;
	    req->rndz = pkt->hdr.rndz;
	    if (req->rndz) {
		memcpy(&req->rgetsender, &pkt->pyld.rndzdata, sizeof(struct utf_vcqid_stadd));
		req->state = R_WAIT_RNDZ;
		req->rcntr = urp;
	    } else {/* eager */
		req->buf = utf_malloc(PKT_MSGSZ(pkt));
		if (eager_copy_and_check(urp, req, pkt) == R_DONE) goto done;
	    }
	}
	urp->req = req;
	goto exiting;
    }
	break;
    case R_BODY:
	req = urp->req;
	if (eager_copy_and_check(urp, req, pkt) == R_DONE) goto done;
	/* otherwise, continue to receive message */
	break;
    case R_DO_RNDZ: /* R_DO_RNDZ --> R_DONE */
    {
	utofu_stadd_t	stadd = SCNTR_ADDR_CNTR_FIELD(sidx);
	DEBUG(DLEVEL_PROTO_RENDEZOUS) {
	    utf_printf("%s: R_DO_RNDZ done urp(%p) req(%p)->vcqid(%lx) rsize(%ld) expsize(%ld)\n",
		       __func__, urp, req, req->rgetsender.vcqid[0], req->rsize, req->expsize);
	}
	req = urp->req;
	if (req->rsize != req->expsize) {
	    /* continue to issue */
	    rget_continue(urp, req);
	    break;
	}
	/*
	 * notification to the sender: local mrq notification is supressed
	 * currently local notitication is enabled
	 */
	utf_remote_armw4(utf_info.vcqh, req->rgetsender.vcqid[0], 0,
			 UTOFU_ARMW_OP_OR, SCNTR_OK,
			 stadd + SCNTR_RGETDONE_OFFST, sidx, 0);
	if (req->bufinfo.stadd[0]) {
	    utf_mem_dereg(utf_info.vcqh, req->bufinfo.stadd[0]);
	    req->bufinfo.stadd[0] = 0;
	}
	req->rsize = req->hdr.size;
	req->state = REQ_DONE;
    }	/* fall into ... */
    case R_DONE: done:
	if (req->type == REQ_RECV_UNEXPECTED) {
	    /* Regiger it to unexpected queue */
	    DEBUG(DLEVEL_PROTOCOL) {
		utf_printf("%s: register it to unexpected queue\n", __func__);
	    }
	    utf_msglst_insert(&utf_uexplst, req);
	} else {
	    DEBUG(DLEVEL_PROTOCOL) {
		utf_printf("%s: Expected message arrived (idx=%d)\n", __func__,
			 utf_msgreq2idx(req));
	    }
	}
	/* reset the state */
	urp->state = R_NONE;
	break;
    case R_HEAD:
    default:
	break;
    }
    exiting:;
}

static inline int
utf_progress()
{
    int		j;
    int		arvd = 0;
    if (utf_tcq_count) utf_tcqprogress();
    for (j = utf_egr_rbuf.head.cntr + 1; j <= RCV_CNTRL_INIT/*RCV_CNTRL_MAX*/ ; j++) {
	struct utf_recv_cntr	*urp = &utf_rcntr[j];
	struct utf_packet	*msgbase = utf_recvbuf_get(j);
	struct utf_packet	*pktp;
	int	sidx;
	pktp = msgbase + urp->recvidx;
	if (pktp->marker == MSG_MARKER) {
	    /* message arrives */
	    utf_tmr_end(TMR_UTF_RCVPROGRESS);
	    sidx = pktp->hdr.sidx;
	    utf_recvengine(urp, pktp, sidx);
	    pktp->marker = 0;
	    urp->recvidx++;
	    if (IS_COMRBUF_FULL(urp)) {
		utofu_stadd_t	stadd = SCNTR_ADDR_CNTR_FIELD(sidx);
		urp->recvidx = 0;
		if (urp->svcqid == 0) {
		    urp->svcqid = utf_info.vname[pktp->hdr.src].vcqid;
		}
		utf_remote_armw4(utf_info.vcqh, urp->svcqid, urp->flags,
				 UTOFU_ARMW_OP_OR, SCNTR_OK,
				 stadd + SCNTR_RST_RECVRESET_OFFST, sidx, 0);
	    }
	    arvd = 1;
	}
    }
    if (!arvd) utf_mrqprogress();
}
