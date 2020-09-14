/*
 *
 */
extern struct utf_msgreq	utf_msgrq[MSGREQ_SIZE];
extern utfslist_t		utf_msgreq_freelst;
extern utfslist_t		utf_egr_sbuf_freelst;
extern utofu_stadd_t		utf_sndctr_stadd;	/* stadd of utf_scntr */
extern struct utf_egr_rbuf	utf_egr_rbuf;		/* eager receive buffer */
extern struct utf_recv_cntr	utf_rcntr[RCV_CNTRL_MAX]; /* receiver control */
extern utfslist_t	utf_rget_proglst;
extern utfslist_t	utf_rndz_proglst;
extern utfslist_t	utf_rndz_freelst;
//extern utfslist_t	utf_rmacq_waitlst;
extern int	utf_tcq_count, utf_mrq_count;
extern int	utf_sreq_count, utf_rreq_count;

extern void	utf_tcqprogress();
extern int	utf_mrqprogress();
extern void	utf_peers_show();
extern int	utf_recvengine(struct utf_recv_cntr *urp, struct utf_packet *pkt, int sidx);
extern char	*utf_pkt_getinfo(struct utf_packet *pktp, int *mrkr, int *sidx);

#define MIN(a,b) (((a)<(b))?(a):(b))

#ifdef UTF_RSTATESYM_NEEDED
static char *rstate_symbol[] =
{	"R_FREE", "R_NONE", "R_HEAD", "R_BODY",	"R_WAIT_RNDZ",
	"R_DO_RNDZ", "R_DO_READ", "R_DO_WRITE", "R_DONE" };
#endif /* UTF_RSTATESYM_NEEDED */

static inline char*
pkt2string(struct utf_packet *pkt, char *buf, size_t len)
{
    static char	pbuf[256];
    if (buf == NULL) {
	buf = pbuf;
	len = 256;
    }
    snprintf(buf, len, "src(%d) tag(0x%lx) size(%ld) pyldsz(%d) rndz(%d) flgs(0x%x) mrker(%d) sidx(%d)",
	     pkt->hdr.src, pkt->hdr.tag, (uint64_t) pkt->hdr.size,
	     pkt->hdr.pyldsz, pkt->hdr.rndz, pkt->hdr.flgs, pkt->hdr.marker, pkt->hdr.sidx);
    return buf;
}

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

static inline struct utf_msgreq *
utf_sendreq_alloc()
{
    utfslist_entry_t *slst;

    if (utf_sreq_count > MSGREQ_SEND_SZ) {
	return NULL;
    }
    utf_sreq_count++;
    slst = utfslist_remove(&utf_msgreq_freelst);
    if (slst != 0) {
	return container_of(slst, struct utf_msgreq, slst);
    } else {
	utf_printf("%s: No more request object\n", __func__);
	abort();
	return NULL;
    }
}

static inline struct utf_msgreq *
utf_recvreq_alloc()
{
    utfslist_entry_t *slst;

    if (utf_rreq_count > MSGREQ_RECV_SZ) {
	return NULL;
    }
    utf_rreq_count++;
    slst = utfslist_remove(&utf_msgreq_freelst);
    if (slst != 0) {
	return container_of(slst, struct utf_msgreq, slst);
    } else {
	utf_printf("%s: No more request object\n", __func__);
	abort();
	return NULL;
    }
}

static inline void
utf_sendreq_free(struct utf_msgreq *req)
{
    req->state = REQ_NONE;
    req->ustatus = REQ_NONE;
    req->notify = NULL;
    --utf_sreq_count;
    utfslist_insert(&utf_msgreq_freelst, &req->slst);
}

static inline void
utf_recvreq_free(struct utf_msgreq *req)
{
    req->state = REQ_NONE;
    req->ustatus = REQ_NONE;
    req->notify = NULL;
    --utf_rreq_count;
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
    utfslist_insert(head, &mlst->slst);
    return mlst;
}

static inline struct utf_msglst *
utf_msglst_append(struct utfslist *head, struct utf_msgreq *req)
{
    struct utf_msglst	*mlst;

    mlst = utf_msglst_alloc();
    mlst->hdr = req->hdr;
    mlst->reqidx = utf_msgreq2idx(req);
    utfslist_append(head, &mlst->slst);
    return mlst;
}

static inline struct utf_send_msginfo *
utf_rndzprog_alloc()
{
    utfslist_entry_t		*slst = utfslist_remove(&utf_rndz_freelst);
    struct utf_send_msginfo *cpyinfo;
    assert(slst != NULL);
    cpyinfo = container_of(slst, struct utf_send_msginfo, slst);
    return cpyinfo;
}

static inline struct utf_send_msginfo *
find_rndzprog(int pos)
{
    utfslist_entry_t	*cur, *prev;
    struct utf_send_msginfo	*minfo = NULL;
    utfslist_foreach2(&utf_rndz_proglst, cur, prev) {
	minfo = container_of(cur, struct utf_send_msginfo, slst);
	if (minfo->mypos == pos) goto find;
    }
    /* not found */
    return NULL;
find:
    utfslist_remove2(&utf_rndz_proglst, cur, prev);
    return minfo;
}

#if 0
static inline void
utf_rmacq_waitappend(struct utf_rma_cq *rma_cq)
{
    utfslist_append(&utf_rmacq_waitlst, &rma_cq->slst);
}
#endif

/*
 * UTOFU_ONESIDED_FLAG_LOCAL_MRQ_NOTICE is controled by caller !!
 */
static inline struct utf_send_cntr *
utf_remote_add(utofu_vcq_hdl_t vcqh,
	       utofu_vcq_id_t rvcqid, unsigned long flgs,
	       uint64_t val, utofu_stadd_t rstadd,
	       uint64_t edata, void *cbdata)
{
    struct utf_send_cntr *usp;
    flgs |= UTOFU_ONESIDED_FLAG_STRONG_ORDER;
    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("remote_add: val(0x%lx) rvcqid(0x%lx)\n", val, rvcqid);
    }
    UTOFU_MSGCALL(1, usp, vcqh, utofu_armw8,
		  vcqh, rvcqid,
		  UTOFU_ARMW_OP_ADD,
		  val, rstadd, edata, flgs, cbdata);
    return usp;
}

static inline struct utf_send_cntr *
utf_remote_armw4(utofu_vcq_hdl_t vcqh,
		 utofu_vcq_id_t rvcqid, unsigned long flgs,
		 enum utofu_armw_op op, uint64_t val,
		 utofu_stadd_t rstadd,
		 uint64_t edata, void *cbdata)
{
    struct utf_send_cntr *usp;
    /* local mrq notification is supressed */
    flgs |= UTOFU_ONESIDED_FLAG_REMOTE_MRQ_NOTICE
	| UTOFU_ONESIDED_FLAG_STRONG_ORDER;
    /* | UTOFU_MRQ_TYPE_LCL_ARMW; remote notification */
    UTOFU_MSGCALL(1, usp, vcqh, utofu_armw4, vcqh, rvcqid, op,
		  val, rstadd, edata, flgs, cbdata);
    return usp;
}

static inline struct utf_send_cntr *
remote_piggysend(utofu_vcq_hdl_t vcqh,
		 utofu_vcq_id_t rvcqid, void *data,  utofu_stadd_t rstadd,
		 size_t len, uint64_t edata, unsigned long flgs, void *cbdata)
{
    struct utf_send_cntr *usp;
    flgs |= UTOFU_ONESIDED_FLAG_TCQ_NOTICE
	 | UTOFU_ONESIDED_FLAG_CACHE_INJECTION
	 | UTOFU_ONESIDED_FLAG_PADDING
	 | UTOFU_ONESIDED_FLAG_STRONG_ORDER;
    UTOFU_MSGCALL(1, usp, vcqh, utofu_put_piggyback,
		  vcqh, rvcqid, data, rstadd, len, edata, flgs, cbdata);
    utf_tcq_count++;
    return usp;
}

static inline struct utf_send_cntr *
remote_piggysend2(utofu_vcq_hdl_t vcqh,
		 utofu_vcq_id_t rvcqid, void *data,  utofu_stadd_t rstadd,
		 size_t len, uint64_t edata, unsigned long flgs, void *cbdata)
{
    struct utf_send_cntr *usp;
    flgs |= UTOFU_ONESIDED_FLAG_STRONG_ORDER
	 | UTOFU_ONESIDED_FLAG_REMOTE_MRQ_NOTICE;
    UTOFU_MSGCALL(1, usp, vcqh, utofu_put_piggyback,
		  vcqh, rvcqid, data, rstadd, len, edata, flgs, cbdata);
    return usp;
}

//#define USE_PUT_TCQ_NOTICE
/*
 * TCQ_NOTICE event driven is employed in the current implementation
 */
static inline struct utf_send_cntr *
remote_put(utofu_vcq_hdl_t vcqh,
	   utofu_vcq_id_t rvcqid, utofu_stadd_t lstadd,
	   utofu_stadd_t rstadd, size_t len, uint64_t edata,
	   unsigned long flgs, void *cbdata)
{
    struct utf_send_cntr *usp;
#ifdef USE_PUT_TCQ_NOTICE
    flgs |= UTOFU_ONESIDED_FLAG_TCQ_NOTICE
	 | UTOFU_ONESIDED_FLAG_REMOTE_MRQ_NOTICE
	 | UTOFU_ONESIDED_FLAG_STRONG_ORDER
	 | UTOFU_ONESIDED_FLAG_CACHE_INJECTION;
    UTOFU_MSGCALL(1, usp, vcqh, utofu_put,
		  vcqh, rvcqid,  lstadd, rstadd, len, edata, flgs, cbdata);
    utf_tcq_count++;
#else
    flgs = 
	 UTOFU_ONESIDED_FLAG_LOCAL_MRQ_NOTICE
/*	 |UTOFU_ONESIDED_FLAG_REMOTE_MRQ_NOTICE*/
	 | UTOFU_ONESIDED_FLAG_STRONG_ORDER
	 | UTOFU_ONESIDED_FLAG_CACHE_INJECTION;
    UTOFU_MSGCALL(1, usp, vcqh, utofu_put,
		  vcqh, rvcqid,  lstadd, rstadd, len, edata, flgs, NULL);
#endif 
    return usp;
}


static inline struct utf_send_cntr *
remote_get(utofu_vcq_hdl_t vcqh,
	   utofu_vcq_id_t rvcqid, utofu_stadd_t lstadd,
	   utofu_stadd_t rstadd, size_t len, uint64_t edata,
	   unsigned long flgs, void *cbdata)
{
    struct utf_send_cntr *usp;
    flgs |= UTOFU_ONESIDED_FLAG_LOCAL_MRQ_NOTICE
	 | UTOFU_ONESIDED_FLAG_STRONG_ORDER;
    UTOFU_MSGCALL(1, usp, vcqh, utofu_get,
		  vcqh, rvcqid,  lstadd, rstadd, len, edata, flgs, cbdata);
    return usp;
}

static inline struct utf_send_cntr *
utf_remote_swap(utofu_vcq_hdl_t vcqh,
		utofu_vcq_id_t rvcqid, unsigned long flgs, uint64_t val,
		utofu_stadd_t rstadd, uint64_t edata, void *cbdata)
{
    utf_printf("%s: NOT YET\n", __func__); abort();
    return NULL;
}

static inline struct utf_send_cntr *
utf_remote_cswap(utofu_vcq_hdl_t vcqh,
		 utofu_vcq_id_t rvcqid, unsigned long flgs,
		 uint64_t old_val, uint64_t new_val,
		 utofu_stadd_t rstadd, uint64_t edata, void *cbdata)
{
    utf_printf("%s: NOT YET\n", __func__); abort();
    return NULL;
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

static inline void
utf_copy_to_iov(const struct iovec *iov, size_t iov_count, uint64_t iov_offset,
		void *buf, uint64_t bufsize)
{
    if (iov_count == 1) {
	uint64_t size = ((iov_offset > iov[0].iov_len) ?
			 0 : MIN(bufsize, iov[0].iov_len - iov_offset));
	memcpy((char *)iov[0].iov_base + iov_offset, buf, size);
    } else {
	uint64_t done = 0, len;
	char *iov_buf;
	size_t i;
	for (i = 0; i < iov_count && bufsize; i++) {
	    len = iov[i].iov_len;
	    if (iov_offset > len) {
		iov_offset -= len;
		continue;
	    }
	    iov_buf = (char *)iov[i].iov_base + iov_offset;
	    len -= iov_offset;
	    len = MIN(len, bufsize);
	    memcpy(iov_buf, (char *) buf + done, len);
	    iov_offset = 0;
	    bufsize -= len;
	    done += len;
	}
    }
}

static inline int
eager_copy_and_check(struct utf_recv_cntr *urp,
		     struct utf_msgreq *req, struct utf_packet *pkt)
{
    size_t	cpysz;

    cpysz = PKT_PYLDSZ(pkt);
    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("%s: req->rsize(%ld) req->hdr.size(%ld) cpysz(%ld) fi_data(%ld) "
		   "EMSG_SIZE(msgp)=%ld\n",
		   __func__, req->rsize, req->hdr.size, cpysz, req->fi_data, PKT_PYLDSZ(pkt));
    }
    if (pkt->hdr.flgs == 0) { /* utf message */
	memcpy(&req->buf[req->rsize], PKT_DATA(pkt), cpysz);
    } else { /* Fabric */
	if (req->overrun) {
	    /* no copy */
	} else if ((req->rsize + cpysz) > req->rcvexpsz) { /* overrun */
	    size_t	rest = req->rcvexpsz - req->rsize;
	    req->overrun = 1;
	    if (rest > 0) {
		utf_copy_to_iov(req->fi_msg, req->fi_iov_count, req->rsize,
				PKT_FI_MSGDATA(pkt), rest);
	    }
	}  else {
	    if (req->buf) { /* enough buffer area has been allocated */
		memcpy(&req->buf[req->rsize], PKT_FI_MSGDATA(pkt), cpysz);
	    } else {
		utf_copy_to_iov(req->fi_msg, req->fi_iov_count, req->rsize,
				PKT_FI_MSGDATA(pkt), cpysz);
	    }
	}
    }
    req->rsize += cpysz;
    if (req->hdr.size == req->rsize) {
	/* Must receive data from the sender */
	urp->state = R_DONE;
    } else {
	/* More data will arrive */
	urp->state = R_BODY;
    }
    return urp->state;
}

static inline void
rget_start(struct utf_msgreq *req)
{
    int	sidx = req->hdr.sidx;

    /* req->hdr.size is defined by the sender
     * req->usrreqsz is defined by the receiver */
    if (req->usrreqsz < req->hdr.size) {
	req->rcvexpsz = req->usrreqsz;
	req->overrun = 1;
    } else {
	req->rcvexpsz = req->hdr.size;
    }
    if (req->buf) {
	req->bufinfo.stadd[0] = utf_mem_reg(utf_info.vcqh, req->buf, req->rcvexpsz);
    } else { /* Fabric request */
	if (req->fi_iov_count == 1) {
	    req->bufinfo.stadd[0] = utf_mem_reg(utf_info.vcqh, req->fi_msg[0].iov_base, req->rcvexpsz);
	} else {
	    utf_printf("%s: cannot handle message vector\n", __func__);
	    abort();
	}
    }
    DEBUG(DLEVEL_PROTO_RENDEZOUS) {
	utf_printf("%s: Receiving Rendezous reqsize(0x%lx) "
		   "rvcqid(0x%lx) lcl_stadd(0x%lx) rmt_stadd(0x%lx) "
		   "sidx(%d)\n",
		   __func__, req->rcvexpsz,
		   req->rgetsender.vcqid[0], req->bufinfo.stadd[0],
		   req->rgetsender.stadd[0]);
    }
    req->rsize =
	(req->rcvexpsz > TOFU_RMA_MAXSZ) ? TOFU_RMA_MAXSZ : req->rcvexpsz;
    remote_get(utf_info.vcqh, req->rgetsender.vcqid[0], req->bufinfo.stadd[0],
	       req->rgetsender.stadd[0], req->rsize, sidx, 0, 0);
    req->state = REQ_DO_RNDZ;
    utfslist_append(&utf_rget_proglst, &req->rget_slst);
}

static inline void
rget_continue(struct utf_msgreq *req)
{
    int	sidx = req->hdr.sidx;
    size_t	restsz, transsz;

    /* req->hdr.size is defined by the sender
     * req->expsize is defined by the receiver */
    DEBUG(DLEVEL_PROTO_RENDEZOUS) {
	utf_printf("%s: Continueing Rendezous reqsize(0x%lx) "
		   "rvcqid(0x%lx) lcl_stadd(0x%lx) rmt_stadd(0x%lx) "
		   "sidx(%d)\n",
		   __func__, req->rcvexpsz,
		   req->rgetsender.vcqid[0], req->bufinfo.stadd[0],
		   req->rgetsender.stadd[0], sidx);
    }
    restsz = req->rcvexpsz - req->rsize;
    transsz = (restsz > TOFU_RMA_MAXSZ) ? TOFU_RMA_MAXSZ : restsz;
    remote_get(utf_info.vcqh, req->rgetsender.vcqid[0], req->bufinfo.stadd[0] + req->rsize,
	       req->rgetsender.stadd[0] + req->rsize, transsz, sidx, 0, 0);
    req->rsize += transsz;
    utfslist_append(&utf_rget_proglst, &req->rget_slst);
}

static inline int
utfgen_explst_match(uint8_t flgs, uint32_t src, uint64_t tag)
{
    int	idx;
    if (flgs == 0) {
	idx = utf_explst_match(src, tag, 0);
    } else {
	utfslist_t *explst
	    = flgs&MSGHDR_FLGS_FI_TAGGED ? &tfi_tag_explst : &tfi_msg_explst;
	idx = tfi_utf_explst_match(explst, src, tag, 0);
    }
    return idx;
}

static inline void
utfgen_uexplst_enqueue(uint8_t flgs, struct utf_msgreq *req)
{
    if (flgs == 0) {
	utf_msglst_append(&utf_uexplst, req);
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("%s: register it(SRC(%d)) to unexpected UTF queue\n", __func__, req->hdr.src);
	}
    } else {
	utfslist_t *uexplst
	    = flgs&MSGHDR_FLGS_FI_TAGGED ? &tfi_tag_uexplst : &tfi_msg_uexplst;
	utf_msglst_append(uexplst, req);
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("%s: register it(SRC(%d)) to unexpected FI %s queue\n", __func__,
		       req->hdr.src, flgs&MSGHDR_FLGS_FI_TAGGED ? "TAGGED" : "MSG");
	}
    }
}

extern int	utf_progress();
#if 0
static inline int
utf_progress()
{
    int	j;
    int	arvd = 0;

    if (utf_tcq_count) utf_tcqprogress();

    if ((int64_t) utf_egr_rbuf.head.cntr < 0) {
	utf_printf("%s: No more Eager Receiver Buffer in my rank\n", __func__);
	utf_egrrbuf_show();
	abort();
    }
    /* here is peeking memory */
    for (j = utf_egr_rbuf.head.cntr + 1; j <= RCV_CNTRL_INIT/*RCV_CNTRL_MAX*/ ; j++) {
	struct utf_packet	*msgbase = utf_recvbuf_get(j);
	struct utf_recv_cntr	*urp = &utf_rcntr[j];
	volatile struct utf_packet	*pktp;
	int	marker, sidx, rc;
    try_again:
	pktp = msgbase + urp->recvidx;
	if (pktp->hdr.marker == MSG_MARKER) {
	    /* message arrives */
	    utf_tmr_end(TMR_UTF_RCVPROGRESS);
	    sidx = pktp->hdr.sidx;
	    if ((sidx & 0xff) == 0xff) {
		utf_printf("%s: j(%d) PROTOCOL ERROR urp(%p)->state(%d:%s) sidx(%d) pkt(%p) MSG(%s)\n",
			   __func__, j, urp, urp->state, rstate_symbol[urp->state],
			   sidx, pktp, pkt2string((struct utf_packet*) pktp, NULL, 0));
		utf_printf("%s: sidx(%d)\n", __func__, pktp->hdr.sidx);
	    }
	    /* for debugging */
	    urp->dbg_rsize[urp->dbg_idx] = PKT_PYLDSZ(pktp);
	    urp->dbg_idx = (urp->dbg_idx + 1) % COM_RBUF_SIZE;
	    rc = utf_recvengine(urp, (struct utf_packet*) pktp, sidx);
	    if (rc == 1) { /* Cannot handle at this time */
		/* Do we need to handle this situation at the sender side ? */
		continue;
	    } else if (rc < 0) {
		utf_printf("%s: j(%d) protocol error urp(%p)->state(%d:%s) sidx(%d) pkt(%p) MSG(%s)\n",
			   __func__, j, urp, urp->state, rstate_symbol[urp->state],
			   sidx, pktp, pkt2string((struct utf_packet*) pktp, NULL, 0));
		abort();
	    }
	    pktp->hdr.hall = -1UL;
	    urp->recvidx++;
	    arvd = 1;
	    if (IS_COMRBUF_FULL(urp)) {
		utofu_stadd_t	stadd = SCNTR_ADDR_CNTR_FIELD(sidx);
		urp->recvidx = 0;
		if (urp->svcqid == 0) {
		    urp->svcqid = utf_info.vname[pktp->hdr.src].vcqid;
		}
		utf_remote_armw4(utf_info.vcqh, urp->svcqid, urp->flags,
				 UTOFU_ARMW_OP_OR, SCNTR_OK,
				 stadd + SCNTR_RST_RECVRESET_OFFST, sidx, 0);
	    } else {
		goto try_again;
	    }
	}
    }
    if (!arvd) utf_mrqprogress();

    return 0;
}
#endif
