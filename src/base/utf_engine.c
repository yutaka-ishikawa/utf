#include <unistd.h>
#include <utofu.h>
#include "utf_conf.h"
#include "utf_tofu.h"
#include "utf_externs.h"
#include "utf_debug.h"
#include "utf_errmacros.h"
#include "utf_sndmgt.h"
#include "utf_queue.h"
#include "utf_timer.h"
#include "utf_msgmacros.h"

extern int utf_tcq_count;
extern utf_sndmgt_t		utf_egrmgt[PROC_MAX];
extern struct utf_send_cntr	utf_scntr[SND_CNTRL_MAX];
extern utofu_stadd_t	utf_sndctr_stadd;	/* stadd of utf_scntr */
extern utofu_stadd_t	utf_sndctr_stadd_end;	/* stadd of utf_scntr */
extern utofu_stadd_t	utf_egr_rbuf_stadd;	/* stadd of utf_egr_rbuf: packet buffer */

static inline struct utf_send_cntr *
utf_idx2scntr(uint32_t idx)
{
    if (idx > SND_CNTRL_MAX) {
	utf_printf("%s: Internal error: out of range on send control (%d)\n",
		 __func__, idx);
	abort();
    }
    return &utf_scntr[idx];
}

static char *notice_symbol[] =
{	"MRQ_LCL_PUT", "MRQ_RMT_PUT", "MRQ_LCL_GET",
	"MRQ_RMT_GET", "MRQ_LCL_ARMW","MRQ_RMT_ARMW"
};

static char *sstate_symbol[] =
{	"S_FREE", "S_NONE", "S_REQ_ROOM", "S_HAS_ROOM",
	"S_DO_EGR", "S_DO_EGR_WAITCMPL", "S_DONE_EGR",
	"S_REQ_RDVR", "S_RDVDON", "S_DONE", "S_WAIT_BUFREADY" };

static char *str_evnt[EVT_END] = {
    "EVT_START", "EVT_CONT", "EVT_LCL", "EVT_RMT_RGETDON",
    "EVT_RMT_RECVRST", "EVT_GET", "EVT_RMT_CHNRDY", "EVT_RMT_CHNUPDT"
};

static struct utf_recv_cntr	*
find_rget_recvcntr(utofu_vcq_id_t vcqid)
{
    utfslist_entry_t	*cur, *prev;
    struct utf_recv_cntr *urcp;
    int	i;
    utfslist_foreach2(&utf_rget_proglst, cur, prev) {
	struct utf_vcqid_stadd  *vsp;
	urcp = container_of(cur, struct utf_recv_cntr, rget_slst);
	vsp = &urcp->req->rgetsender;
	for (i = 0; i < vsp->nent; i++) {
	    if (vsp->vcqid[i] == vcqid) {
		utfslist_remove2(&utf_rget_proglst, cur, prev);
		urcp->rget_slst.next = NULL;
		goto find;
	    }
	}
    }
    urcp = 0;
find:
    return urcp;
}


static int
is_scntr(utofu_stadd_t val, int *evtype)
{
    uint64_t	entry, off;
    if (val >= utf_sndctr_stadd && val <= utf_sndctr_stadd_end) {
	entry = (val - utf_sndctr_stadd) / sizeof(struct utf_send_cntr);
	off = val - utf_sndctr_stadd - entry*sizeof(struct utf_send_cntr);
	if (SCNTR_IS_RGETDONE_OFFST(off)) {
	    *evtype = EVT_RMT_RGETDON;
	} else if (SCNTR_IS_RECVRESET_OFFST(off)) {
	    *evtype = EVT_RMT_RECVRST;
	} else if (SCNTR_IS_CHN_NEXT_OFFST(off)) {
	    /* This happens in a result of remote put operation.
	     * rmt_stadd is advanced by size of uint64_t in remote result */
	    *evtype = EVT_RMT_CHNUPDT;
	} else if (off == SCNTR_CHN_READY_OFFST + sizeof(uint64_t)) {
	    /* This happens in a result of remote put operation.
	     * rmt_stadd is advanced by size of uint64_t in remote result */
	    *evtype = EVT_RMT_CHNRDY;
	} else {
	    utf_printf("%s: val(0x%lx) utf_sndctr_stadd(0x%lx) off(0x%lx)\n",
		     __func__, val, utf_sndctr_stadd, off);
	    abort();
	}
	return 1;
    }
    return 0;
}


static inline utofu_stadd_t
calc_recvstadd(struct utf_send_cntr *usp, uint64_t ridx)
{
    utofu_stadd_t	recvstadd = 0;
    if (IS_COMRBUF_FULL(usp)) {
	/* buffer full */
	DEBUG(DLEVEL_PROTOCOL|DLEVEL_UTOFU) {
	    utf_printf("%s: BUSYWAIT\n", __func__);
	}
	usp->ostate = usp->state;
	usp->state = S_WAIT_BUFREADY;
//	utf_mrq_count++; /* receiving MRQ event */
    } else {
	recvstadd  = utf_egr_rbuf_stadd
	    + (uint64_t)&((struct utf_egr_rbuf*)0)->rbuf[COM_RBUF_SIZE*ridx]
	    + usp->recvidx*MSG_PKTSZ;
    }
    return recvstadd;
}

/*
 * sender engine
 *		
 */
static inline void
utf_sendengine(struct utf_send_cntr *usp, struct utf_send_msginfo *minfo, uint64_t rslt, int evt)
{
progress:
    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("%s: usp(%p)->state(%s), evt(%d) rcvreset(%d) recvidx(%d)\n",
		 __func__, usp, sstate_symbol[usp->state],
		 evt, usp->rcvreset, usp->recvidx);
    }
    switch(usp->state) {
    case S_NONE: /* never comes */
	break;
    case S_REQ_ROOM:
	if ((int64_t)rslt >= 0) {
	    utf_sndmgt_set_sndok(usp->dst, utf_egrmgt);
	    /* ridx is set */
	    utf_sndmgt_set_index(usp->dst, utf_egrmgt, rslt);
	    usp->state = S_HAS_ROOM;
	    usp->recvidx = 0;
	    DEBUG(DLEVEL_PROTOCOL) {
		utf_printf("\tRoom is available in remote rank\n");
	    }
	    goto s_has_room;
	} else {
	    /* No room in remote rank */
	    utf_printf("No room in remote rank\n");
	    abort();
	}
	break;
    case S_HAS_ROOM: /* 3 */
    s_has_room:
    {
	struct utf_packet	*pkt = &minfo->sndbuf->pkt;
	utofu_vcq_id_t	rvcqid = usp->rvcqid;
	unsigned long	flgs = 0; // usp->flags;
	uint64_t	ridx = utf_sndmgt_get_index(usp->dst, utf_egrmgt);
	utofu_stadd_t	recvstadd;

	recvstadd = calc_recvstadd(usp, ridx);
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("%s: Going to send rvcqid(%lx) recvidx(%ld) recvstadd(%lx) "
		     "type(%d) sidx(%d) ridx(%d) MSGHDR(%s)\n",
		       __func__, rvcqid, usp->recvidx, recvstadd, minfo->cntrtype, usp->mypos, ridx,
		       pkt2string(pkt, NULL, 0));
	}
	if (recvstadd == 0) {
	    /* state is S_WAIT_BUFREADY */
	    break;
	}
	switch (minfo->cntrtype) {
	case SNDCNTR_BUFFERED_EAGER_PIGBACK:  /* just one chache-line */
	    /* packet data is already filled */
	    remote_piggysend(utf_info.vcqh, rvcqid, &minfo->sndbuf->pkt,
			     recvstadd, 32, usp->mypos, flgs, usp);
	    usp->recvidx++;
	    usp->usize = PKT_PYLDSZ(pkt);
	    usp->state = S_DONE_EGR;
	    break;
	case SNDCNTR_BUFFERED_EAGER: /* just one packet */
	    /* packet data is already filled */
	    remote_put(utf_info.vcqh, rvcqid, minfo->sndstadd,
		       recvstadd, MSG_PKTSZ, usp->mypos, flgs, usp);
	    usp->recvidx++;
	    usp->usize = PKT_PYLDSZ(pkt);
	    usp->state = S_DONE_EGR;
	    break;
	case SNDCNTR_INPLACE_EAGER:
	    usp->usize = PKT_PYLDSZ(pkt);
	    usp->state = S_DO_EGR;
	    remote_put(utf_info.vcqh, rvcqid, minfo->sndstadd,
		       recvstadd, MSG_PKTSZ, usp->mypos, flgs, usp);
	    usp->recvidx++;
	    break;
	case SNDCNTR_RENDEZOUS:
	    usp->usize = minfo->msghdr.size; // expected all data
	    usp->state = S_REQ_RDVR;
	    remote_put(utf_info.vcqh, rvcqid, minfo->sndstadd,
		       recvstadd, MSG_PKTSZ, usp->mypos, flgs, usp);
	    usp->recvidx++;
	    usp->rgetwait = 0; usp->rgetdone = 0;
	    DEBUG(DLEVEL_PROTO_RENDEZOUS) {
		utf_printf("%s: rendezous send size(%ld) minfo->sndstadd(%lx) "
			   "src(%d) tag(%d) rndz(%d)\n", __func__,
			   usp->usize, minfo->sndstadd,
			   minfo->sndbuf->pkt.hdr.src,
			   minfo->sndbuf->pkt.hdr.tag,
			   minfo->sndbuf->pkt.hdr.rndz);
	    }
	    break;
	}
	break;
    }
    case S_DO_EGR: /* eager message */
    {
	struct utf_packet	*pkt = &minfo->sndbuf->pkt;
	unsigned long	flgs = 0; // usp->flags;
	utofu_vcq_id_t	rvcqid = usp->rvcqid;
	size_t		rest = minfo->msghdr.size - usp->usize;
	uint64_t	ridx = utf_sndmgt_get_index(usp->dst, utf_egrmgt);
	utofu_stadd_t	recvstadd;
	size_t		ssize;

	ssize = rest > MSG_PYLDSZ ? MSG_PYLDSZ : rest;
	recvstadd = calc_recvstadd(usp, ridx);
	if (recvstadd == 0) {
	    break;
	}
	/*
	 * copy to eager send buffer. minfo->sndbuf is minfo->sndstadd
	 * in stearing address.
	 */
	memcpy(pkt->pyld.msgdata, minfo->usrbuf + usp->usize, ssize);
	pkt->hdr.pyldsz = ssize;
	DEBUG(DLEVEL_PROTO_EAGER) {
	    utf_printf("%s: S_DO_EGR, mreq(%p) msgsize(%ld) sentsize(%ld) "
		       "rest(%ld) sending sent_size(%ld) send_size(%ld) "
		       "recvstadd(0x%lx) usp->recvidx(%d)\n",
		       __func__, minfo->mreq, minfo->msghdr.size, usp->usize,
		       rest, usp->usize, ssize, recvstadd, usp->recvidx);
	}
	remote_put(utf_info.vcqh, rvcqid, minfo->sndstadd, recvstadd, ssize, usp->mypos, flgs, usp);
	usp->usize += ssize;
	usp->recvidx++;
	if (minfo->msghdr.size == usp->usize) {
	    usp->state = S_DONE_EGR;
	}
	break;
    }
    case S_REQ_RDVR:
    {
	size_t	rest = minfo->msghdr.size - usp->usize;
	DEBUG(DLEVEL_PROTO_RENDEZOUS) {
	    utf_printf("%s: S_REQ_RDVR evt(%s)\n", __func__, str_evnt[evt]);
	}
	if (rest == 0) {
	    usp->state = S_RDVDONE;
	    goto rdvdone;
	}
	/* checking data size */
    }
	break;
    case S_DO_EGR_WAITCMPL:
	utf_printf("%s: S_DO_EGR_WAITCMPL rcvreset(%d)\n", __func__, usp->rcvreset);
	break;
    case S_RDVDONE:
    rdvdone:
	if (minfo->mreq->bufinfo.stadd[0]) {
	    utf_mem_dereg(utf_info.vcqh, minfo->mreq->bufinfo.stadd[0]);
	    minfo->mreq->bufinfo.stadd[0] = 0;
	}
	/* fall into ... */
    case S_DONE_EGR:
    done_egr:
    {
	int	idx;
	struct utf_msgreq	*req = minfo->mreq;
	// utf_printf("%s: DONE_EGER: req(%p)->state(%d) usp(%p)->state(%d) micur(%d) mient(%d) minfo(%p)\n", __func__, req, req->state, usp, usp->state, usp->micur, usp->mient, minfo);
	if (req->notify) req->notify(req);
	if (req->state == REQ_PRG_RECLAIM) {
	    utf_msgreq_free(req); /* req->state is reset to REQ_NONE */
	} else {
	    req->state = REQ_DONE;
	}
	minfo->mreq = NULL;
	/*
	 * reset minfo entry, keeping the send buffer
	 *  i.e., no call utf_egr_sbuf_free(minfo->sndbuf);
	 */
	minfo->cntrtype = SNDCNTR_NONE;
	/* checking the next entry */
	idx = (usp->micur + 1)%COM_SCNTR_MINF_SZ;
	if (idx == usp->mient) {
	    /* all entries are sent */
	    usp->micur =usp->mient = 0;
	    usp->state = S_NONE;
	    //utf_printf("%s: ALLDONE usp(%p)->micur(%d) mient(%d) top_cntrtype(%d)\n", __func__, usp, usp->micur, usp->mient, usp->msginfo[0].cntrtype);
	} else {
	    minfo = &usp->msginfo[idx];
	    if (minfo->cntrtype != SNDCNTR_NONE) {
		usp->micur = idx;
		usp->state = S_HAS_ROOM;
		evt = EVT_CONT;
		//utf_printf("%s: NEXT REQUEST usp(%p)->micur(%d) mient(%d) minfo(%p)->cntrtype=%d\n", __func__, usp, usp->micur, usp->mient, minfo, minfo->cntrtype);
		goto s_has_room;
	    }
	}
	break;
    }
    case S_DONE:
	break;
    case S_WAIT_BUFREADY:
	utf_printf("%s: protocol error in S_WAIT_BUFREADY\n", __func__);
	break;
    }
}

int
utf_send_start(struct utf_send_cntr *usp, struct utf_send_msginfo *minfo)
{
    int	dst = usp->dst;
    if (utf_sndmgt_isset_sndok(dst, utf_egrmgt) != 0) {
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("%s: Has a received room in rank %d: send control(%d)\n",
		     __func__, dst, usp->mypos);
	}
	usp->state = S_HAS_ROOM;
	utf_sendengine(usp, minfo, 0, EVT_START);
	return 0;
    } else if (utf_sndmgt_isset_examed(dst, utf_egrmgt) == 0) {
	/*
	 * checking availability
	 *   edata: usp->mypos for mrqprogress
	 */
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("%s: Request a room to rank %d: send control(%d)\n"
		     "\trvcqid(%lx) remote stadd(%lx)\n",
		     __func__, dst, usp->mypos, usp->rvcqid, utf_egr_rbuf_stadd);
	}
	utf_remote_add(utf_info.vcqh, usp->rvcqid,
		       UTOFU_ONESIDED_FLAG_LOCAL_MRQ_NOTICE,
		       (uint64_t) -1, utf_egr_rbuf_stadd, usp->mypos, 0);
	usp->state = S_REQ_ROOM;
	utf_sndmgt_set_examed(dst, utf_egrmgt);
	return 0;
    } else {
	utf_printf("%s: ERROR!!!!\n", __func__);
	return -1;
    }
}

void
utf_tcqprogress()
{
    struct utf_send_cntr	 *usp;
    struct utf_send_msginfo	*minfo;
    int	rc;

    do {
	rc = utofu_poll_tcq(utf_info.vcqh, 0, (void*) &usp);
	if (rc != UTOFU_SUCCESS) break;
	minfo = &usp->msginfo[usp->micur];
	utf_sendengine(usp, minfo, 0, EVT_LCL);
	--utf_tcq_count;
    } while (utf_tcq_count);
    if (rc != UTOFU_ERR_NOT_FOUND && rc != UTOFU_SUCCESS) {
	char msg[1024];
	utofu_get_last_error(msg);
	utf_printf("%s: error rc(%d) %s\n", __func__, rc, msg);
	abort();
    }
}


int
utf_mrqprogress()
{
    int	rc;
    struct utofu_mrq_notice mrq_notice;

    rc = utofu_poll_mrq(utf_info.vcqh, 0, &mrq_notice);
    if (rc == UTOFU_ERR_NOT_FOUND) return rc;
    if (rc != UTOFU_SUCCESS) {
	char msg[1024];
	utofu_get_last_error(msg);
	utf_printf("%s: utofu_poll_mrq ERROR rc(%d) %s\n", __func__, rc, msg);
	return rc;
    }
    DEBUG(DLEVEL_UTOFU) {
	utf_printf("%s: type(%s)\n", __func__,
		 notice_symbol[mrq_notice.notice_type]);
    }
    //--utf_mrq_count;
    switch(mrq_notice.notice_type) {
    case UTOFU_MRQ_TYPE_LCL_GET: /* 2 */
    {
	uint8_t	sidx = mrq_notice.edata;
	struct utf_recv_cntr	*urp;
	struct utf_msgreq	*req;

	urp = find_rget_recvcntr(mrq_notice.vcq_id);
	req = urp->req;
	DEBUG(DLEVEL_UTOFU) {
	    utf_printf("%s: MRQ_TYPE_LCL_GET: edata(%d) "
		       "lcl_stadd(%lx) rmt_stadd(%lx) state(%d:%s) sidx(%d)\n",
		       __func__, mrq_notice.edata,
		       mrq_notice.lcl_stadd, mrq_notice.rmt_stadd,
		       urp->state, rstate_symbol[urp->state],
		       req->hdr.sidx);
	}
	utf_recvengine(urp, NULL, req->hdr.sidx);
	break;
    }
    case UTOFU_MRQ_TYPE_RMT_GET: /* 3 */
    {
	struct utf_send_cntr *usp;
	struct utf_send_msginfo *minfo;
	int	sidx = mrq_notice.edata;
	DEBUG(DLEVEL_UTOFU) {
	    utf_printf("%s: MRQ_TYPE_RMT_GET: edata(%d) "
		     "lcl_stadd(%lx) rmt_stadd(%lx)\n",
		     __func__, mrq_notice.edata,
		     mrq_notice.lcl_stadd, mrq_notice.rmt_stadd);
	}
	utf_printf("%s: MRQ_TYPE_RMT_GET: edata(%d) "
		   "lcl_stadd(%lx) rmt_stadd(%lx)\n",
		   __func__, mrq_notice.edata,
		   mrq_notice.lcl_stadd, mrq_notice.rmt_stadd);
	usp = utf_idx2scntr(sidx);
	minfo = &usp->msginfo[usp->micur];
	utf_sendengine(usp, minfo, 0, EVT_RMT_GET);
    }
	break;
    case UTOFU_MRQ_TYPE_LCL_ARMW:/* 4 */
    {	/* Sender side:
	 * rmt_value is a result of this atomic operation.
	 * edata is index of sender engine */
	uint32_t	sidx = mrq_notice.edata;
	struct utf_send_cntr *usp;
	struct utf_send_msginfo	*minfo;
	int	evtype;
	usp = utf_idx2scntr(sidx);
	minfo = &usp->msginfo[usp->micur];
	DEBUG(DLEVEL_UTOFU) {
	    utf_printf("%s: MRQ_LCL_ARM: edata(%d) rmt_val(%ld) usp(%p)\n",
		     __func__, mrq_notice.edata, mrq_notice.rmt_value, usp);
	}
	utf_sendengine(usp, minfo, mrq_notice.rmt_value, EVT_LCL);
	break;
    }
    case UTOFU_MRQ_TYPE_RMT_ARMW:/* 5 */
    {
	int	evtype;
	/* Receiver side:
	 * rmt_value is the stadd address changed by the sender
	 * edata represents the sender's index of sender engine 
	 * The corresponding notification in the sender side is
	 * UTOFU_MRQ_TYPE_LCL_ARMW */
	if (is_scntr(mrq_notice.rmt_value, &evtype)) {
	    /* access my send control structure */
	    int			sidx = mrq_notice.edata;
	    struct utf_send_cntr *usp;
	    struct utf_send_msginfo	*minfo;
	    usp = utf_idx2scntr(sidx);
	    minfo = &usp->msginfo[usp->micur];
	    DEBUG(DLEVEL_UTOFU) {
		utf_printf("%s: MRQ_RMT_ARMW sidx rmt_value(%lx) sidx(%d) evtype(%d:%s) "
			   "usp(%p)->micur(%d) state(%d:%s) ostate(%d:%s) recvidx(%d) "
			   "rcvreset(%d) mient(%d)\n",
			   __func__, mrq_notice.rmt_value, sidx, evtype, str_evnt[evtype],
			   usp, usp->micur, usp->state, sstate_symbol[usp->state],
			   usp->ostate, sstate_symbol[usp->ostate], usp->recvidx,
			   usp->rcvreset, usp->mient);
	    }
	    if (evtype == EVT_RMT_RECVRST) {
		usp->rcvreset = 0; usp->recvidx = 0;
		if (usp->state != S_NONE) {
		    usp->state = usp->ostate;
		    utf_sendengine(usp, minfo, mrq_notice.rmt_value, evtype);
		}
	    } else {
		utf_sendengine(usp, minfo, mrq_notice.rmt_value, evtype);
	    }
	} else {
	    /* receive buffer is accessedd by the sender */
	    utf_printf("%s: MRQ_RMT_ARMW where ? rmt_value(%lx) rmt_stadd(%lx) evtype(%d)\n",
		       __func__, mrq_notice.rmt_value, mrq_notice.rmt_stadd, evtype);
	}
	DEBUG(DLEVEL_UTOFU) {
	    utf_printf("%s: MRQ_TYPE_RMT_ARMW: edata(%d) rmt_value(0x%lx)\n",
		     __func__, mrq_notice.edata, mrq_notice.rmt_value);
	}
	break;
    }
    case UTOFU_MRQ_TYPE_LCL_PUT: /* 0 */
    case UTOFU_MRQ_TYPE_RMT_PUT: /* 1 */
    default:
	utf_printf("%s: Unused notice type(%d): edata(%d) rmt_value(%ld)\n",
		 __func__, mrq_notice.notice_type, mrq_notice.edata, mrq_notice.rmt_value);
	break;
    }
    return rc;
}

int
utf_dbg_progress(int mode)
{
    int	rc;
    if (mode) {
	utf_printf("%s: progress\n", __func__);
    }
    rc = utf_progress();
    return rc;
}


void
utf_recvcntr_show(FILE *fp)
{
    extern struct erecv_buf	*erbuf;
    uint64_t		cntr = utf_egr_rbuf.head.cntr;
    int	i;
    utf_printf("# of PEERS: %d\n", RCV_CNTRL_INIT - cntr);
    for (i = COM_PEERS - 1; i > cntr; --i) {
	fprintf(fp, "\t0x%lx", utf_rcntr[i].svcqid);
	if (((i + 1) % 8) == 0) fprintf(fp, "\n");
    }
    fprintf(fp, "\n"); fflush(fp);
}
