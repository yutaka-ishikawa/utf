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
#define UTF_RSTATESYM_NEEDED
#include "utf_msgmacros.h"

#define INTERNAL_ERROR do { \
    utf_printf("%s: UTF internal error\n", __func__);	\
    abort();						\
} while(0);

extern int utf_tcq_count, utf_injct_count;
extern utf_sndmgt_t		utf_egrmgt[PROC_MAX];
extern struct utf_send_cntr	utf_scntr[SND_CNTRL_MAX];
extern struct utf_rma_cq	utf_rmacq_pool[COM_RMACQ_SIZE];
extern utofu_stadd_t	utf_sndctr_stadd;	/* stadd of utf_scntr */
extern utofu_stadd_t	utf_sndctr_stadd_end;	/* stadd of utf_scntr */
extern utofu_stadd_t	utf_egr_rbuf_stadd;	/* stadd of utf_egr_rbuf: packet buffer */
extern utofu_stadd_t	utf_rcntr_stadd;	/* stadd of utf_rcntr */
extern utofu_stadd_t	utf_rndz_stadd;		/* stadd of utf_rndz_freelst */
extern utofu_stadd_t	utf_rndz_stadd_end;	/* stadd of utf_rndz_freelst */

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
	"S_DO_EGR", "S_DO_EGR_WAITCMPL", "S_DONE_EGR", "S_REQ_RDVR",
	"S_DO_RDVR", "S_RDVDONE", "S_DONE", "S_WAIT_BUFREADY",
	"S_WAIT_EGRSEND", "S_WAIT_RESETCHN", "S_WAIT_NEXTUPDT", "S_WAIT_BUFREADY_CHND" };

static char *str_evnt[EVT_END] = {
    "EVT_START", "EVT_CONT", "EVT_LCL", "EVT_LCL_CHNNXT", "EVT_LCL_EVT_ARM",
    "EVT_RMT_RGETDON", "EVT_RMT_RECVRST", "EVT_GET", "EVT_RMT_CHNRDY", "EVT_RMT_CHNUPDT"
};


static struct utf_msgreq	*
find_rget_msgreq(utofu_vcq_id_t vcqid)
{
    utfslist_entry_t	*cur, *prev;
    struct utf_msgreq *req;
    int	i;
    utfslist_foreach2(&utf_rget_proglst, cur, prev) {
	struct utf_vcqid_stadd  *vsp;
	req = container_of(cur, struct utf_msgreq, rget_slst);
	vsp = &req->rgetsender;
	for (i = 0; i < vsp->nent; i++) {
	    if (vsp->vcqid[i] == vcqid) {
		utfslist_remove2(&utf_rget_proglst, cur, prev);
		req->rget_slst.next = NULL;
		goto find;
	    }
	}
    }
    req = 0;
find:
    return req;
}


static int
is_scntr(utofu_stadd_t val, int *evtype)
{
    uint64_t	entry, off;
    if (val >= utf_sndctr_stadd && val <= utf_sndctr_stadd_end) {
	entry = (val - utf_sndctr_stadd) / sizeof(struct utf_send_cntr);
	off = val - utf_sndctr_stadd - entry*sizeof(struct utf_send_cntr);
//	if (SCNTR_IS_RGETDONE_OFFST(off)) {
//	    *evtype = EVT_RMT_RGETDON;
//	} else 
	if (SCNTR_IS_RECVRESET_OFFST(off)) {
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

/*
 * Chain mode
 */
static inline uint64_t
make_chain_addr(int rank, int mypos, int idx)
{
    union utf_chain_addr	addr;
    addr.rank = rank; addr.sidx = mypos;
    addr.recvidx = idx;
    return addr.rank_sidx;
}


/*
 *	EVT_LCL_XX is generated
 */
static inline void
chain_reset_recv_chntail(struct utf_send_cntr *usp)
{
    uint64_t	flgs = 0;
    uint64_t	oval = make_chain_addr(utf_info.myrank, usp->mypos, 0);
    uint64_t	nval = make_chain_addr(-1, -1, usp->recvidx);
    /* new value has the last recidx */
    /* This is the Compare and Swap */
    utf_remote_cswap(utf_info.vcqh, usp->rvcqid, flgs,
		     oval, nval, EGRCHAIN_RECV_CHNTAIL, usp->mypos, NULL);
}

/*
 * Ready to send flag in chain mode is set to the remote process
 * As a result of this operation, 
 *	EVT_LCL is generated.	NOT GENERATED 2020/05/25
 *	EVT_RMT_CHNRDY is generated in the remote side sendengine.
 */
static inline void
chain_inform_ready(struct utf_send_cntr *usp)
{
    utofu_vcq_id_t	rvcqid;
    uint64_t	flgs = 0;
    union utf_chain_rdy	ready;
    ready.rdy = 1;
    ready.recvidx = usp->recvidx;
    rvcqid = utf_info.vname[usp->chn_next.rank].vcqid;
    usp->chn_informed = 1;
    /* sindex is remote one */
    remote_piggysend2(utf_info.vcqh, rvcqid, &ready,
		      SCNTR_CHAIN_READY(usp->chn_next.sidx),
		      sizeof(uint64_t),  usp->chn_next.sidx, flgs, NULL);
    DEBUG(DLEVEL_CHND) {
	utf_printf("%s:[CHND] INFORM-TO rank(%d)\n", __func__, usp->chn_next.rank);
    }
}

void
utf_settransmode(int mode)
{
    int	dst;
    utf_mode_trans = mode;
    for (dst = 0; dst < PROC_MAX;  dst++) {
	utf_sndmgt_set_smode(dst, utf_egrmgt, mode);
    }
}

static inline void
chain_set_recvinfo(struct utf_send_cntr *usp, int recvidx)
{
    // utf_sndmgt_set_sndok(usp->dst, utf_egrmgt);
    utf_sndmgt_set_index(usp->dst, utf_egrmgt, EGRCHAIN_RECVPOS);
    usp->recvidx = recvidx;
}

#define PROG_CHAIN_NONE		0
#define PROG_CHAIN_SENDSTART	1
#define PROG_CHAIN_SENDENGINE	2
#define PROG_CHAIN_INFORM	3
#define PROG_CHAIN_WAIT_NEXTUPDT 4

static inline int
chain_prog_room(struct utf_send_cntr *usp, uint64_t rslt)
{
    int	rc;

    if (IS_CHAIN_EMPTY((union utf_chain_addr)rslt)) { /* success */
	chain_set_recvinfo(usp, ((union utf_chain_addr)rslt).recvidx);
	usp->state = S_HAS_ROOM;
	rc = PROG_CHAIN_SENDENGINE;
    } else {
	union utf_chain_addr last_rank;
	utofu_vcq_id_t	rvcqid;
	uint64_t	data;
	uint64_t	flags = 0;
	/*
	 * Set myinfo into the last rank in chain mode
	 * As a result of this operation,
	 *	EVT_RMT_CHNUPDT is generated in the remote side sendengine
	 *	TCQ_NOTICE is not generated in local side
	 * Keeping the S_ERQ_ROOM state. The state will be changed
	 * by EVT_RMT_CHNUPDT
	 */
	last_rank.rank_sidx = rslt;
	rvcqid = utf_info.vname[last_rank.rank].vcqid;
	data = make_chain_addr(utf_info.myrank, usp->mypos, 0);
	/* sidx is remote one */
	remote_piggysend2(utf_info.vcqh, rvcqid, &data, SCNTR_CHAIN_NXT(last_rank.sidx),
			  sizeof(uint64_t), last_rank.sidx, flags, NULL);
	rc = PROG_CHAIN_NONE;
	DEBUG(DLEVEL_CHND) {
	    utf_printf("%s:[CHND] WAIT-FOR rank(%d)\n", __func__, last_rank.rank);
	}
	usp->waitfor = last_rank.rank;
    }
    return rc;
}

/*
 * chain_prog_resetchain()
 *	-- checking a result of resetting receiver's chain-tail,
 *	    chain_reset_recv_chntail()
 *	S_WAIT_RESETCHN
 */
static inline int
chain_check_resetchain(struct utf_send_cntr *usp, uint64_t rslt)
{
    int	rc;
    if (((union utf_chain_addr)rslt).rank == utf_info.myrank) {
	/* receiver's chain-tail field is the same of my rank */
	rc = PROG_CHAIN_NONE;
    } else {
	/* This happens when this rank, the last rank of this chain,
	 * is going to reset the chain-tail of the receiver, but some process
	 * has already obtained the chain-tail field. */
	/* The chn_next field has already or will be filled in */
	if (usp->evtupdt) { /* already arrived */
	    if (IS_CHAIN_EMPTY(usp->chn_next)) {/* never happens */
		INTERNAL_ERROR;
	    }
	    rc = PROG_CHAIN_INFORM;
	} else {
	    rc = PROG_CHAIN_WAIT_NEXTUPDT;
	}
    }
    return rc;
}

/*
 * chain_progress(): all states changes related to the chain mode
 *	return PROG_CHAIN_NONE | PROG_CHAIN_SENDSTART | PROG_CHAIN_SENDENGINE
 */
static int
chain_progress(struct utf_send_cntr *usp, uint64_t rslt, int evtype)
{
    int	rc = PROG_CHAIN_NONE;
    switch (evtype) {
    case EVT_LCL_CHNNXT: /* called from sendengine at the end of send (S_NODE state)
			  * or mrqprogress (in case of S_WAIT_BUFREADY_CHND state) */
	/* (usp->state == S_NONE) and (usp->inflight == 0) */
	usp->ostate = usp->state;
	if (usp->inflight > 0) {
	    usp->state = S_HAS_ROOM;
	    rc = PROG_CHAIN_SENDENGINE;
	} else {
	    if (IS_CHAIN_EMPTY(usp->chn_next)) {
		/* I'm the last entry at this moment, reset the remote entry */
		chain_reset_recv_chntail(usp);
		usp->state = S_WAIT_RESETCHN;
		/* EVT_LCL_ARM will be generated in the sendengine */
	    } else { /* usp->chn_next has been filled in */
		if (usp->evtupdt) {
		    /* inform ready to the next rank */
		    chain_inform_ready(usp);
		    /* no event will be generated */
		    usp->state = S_NONE;
		} else {
		    usp->state = S_WAIT_NEXTUPDT;
		    /* wait for EVT_RMT_CHNUPDT */
		}
	    }
	}
	/* return to the sendengine */
	break;
    case EVT_LCL_EVT_ARM: /* result of REQ_ROOM or RESET RECV-CHAIN */
	if (usp->state == S_REQ_ROOM) {
	    /* result of utf_send_start() */
	    rc = chain_prog_room(usp, rslt);
	    /* state has been changed to S_HAS_ROOM or keeping S_REQ_ROOM */
	} else if (usp->state == S_WAIT_RESETCHN) {
	    /* result of utf_reset_recv_chntail() */
	    rc = chain_check_resetchain(usp, rslt);
	    // utf_printf("%s: rc(%d) from chain_check_resetchain\n", __func__, rc);
	    switch (rc) {
	    case PROG_CHAIN_INFORM:
		/* inform ready to the next rank */
		chain_inform_ready(usp);
		/* Falls through. */
	    case PROG_CHAIN_NONE:
		usp->ostate = usp->state;
		usp->state = S_NONE;
		if (usp->inflight > 0) {
		    /* the right of send has been released by
		     * resetchain, so send_start again  */
		    rc = PROG_CHAIN_SENDSTART;
		}
		break;
	    case PROG_CHAIN_WAIT_NEXTUPDT:
		usp->ostate = usp->state;
		usp->state = S_WAIT_NEXTUPDT;
		rc = PROG_CHAIN_NONE;
		break;
	    default: /* never comes */
		utf_printf("%s: EVT_LCL_EVT_ARM: rc %d\n", __func__, rc);
		INTERNAL_ERROR;
	    }
	} else {
	    /* 2021/01/26 state == DONE_EGR (6) */
	    utf_printf("%s: EVT_LCL_EVT_ARM: state = %d\n", __func__, usp->state);
	    utf_sendctr_show();
	    INTERNAL_ERROR;
	}
	break;
    case EVT_RMT_CHNUPDT: /* Chain updated */
	/* Two cases: At the end of send,
	 *	1) the chain field has been filled, but not yet processing this event.
	 *		usp->state: S_NONE -> S_WAIT_NEXTUPDT
	 *	2) the chain field has not been filled, but it is changed during
	 *	   resetting the receiver's chain-tail field.
	 *		usp->state: S_NONE -> S_WAIT_RESETCHN -> S_WAIT_NEXTUPDT
	 */
	usp->evtupdt = 1;
	if (usp->state == S_WAIT_NEXTUPDT) {
	    /* inform ready to the next rank */
	    chain_inform_ready(usp);
	    usp->ostate = usp->state;
	    usp->state = S_NONE;
	    if (usp->inflight > 0) {
		rc = PROG_CHAIN_SENDSTART;
	    }
	}
	break;
    case EVT_RMT_CHNRDY:
	chain_set_recvinfo(usp, usp->chn_ready.recvidx);
	usp->state = S_HAS_ROOM;
	rc = PROG_CHAIN_SENDENGINE;
	break;
    default:
	INTERNAL_ERROR;
	break;
    }
    return rc;
}


static int
is_msginfo(utofu_stadd_t val)
{
    if (val >= utf_rndz_stadd && val <= utf_rndz_stadd_end) {
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
	DEBUG(DLEVEL_PROTOCOL|DLEVEL_UTOFU|DLEVEL_PROTO_AM) {
	    utf_printf("%s: BUSYWAIT usp(%p) DST(%d)\n", __func__, usp, usp->dst);
	}
	usp->ostate = usp->state;
	usp->state = S_WAIT_BUFREADY;
    } else {
	recvstadd  = utf_egr_rbuf_stadd
	    + (uint64_t)&((struct utf_egr_rbuf*)0)->rbuf[COM_RBUF_SIZE*ridx]
	    + usp->recvidx*MSG_PKTSZ;
    }
    return recvstadd;
}

static inline int
can_sendcontig(struct utf_send_cntr *usp, uint64_t ridx, int ssize, int pyldsz)
{
    int reqpckt = (ssize+pyldsz-1)/pyldsz;
    int npckt;

    npckt = (reqpckt > COM_EGR_PKTSZ) ? COM_EGR_PKTSZ : reqpckt;
    npckt = (npckt > utf_injct_count) ? utf_injct_count : npckt;	/* forcing inject count before polling tcq completion */
    if (usp->recvidx + npckt < COM_RBUF_SIZE) {
	return npckt;
    } else {
	int rest = COM_RBUF_SIZE - usp->recvidx;
	// utf_printf("%s: YI!!! REST(%d)\n", __func__, rest);
	return rest;
    }
}

extern void
utf_sendengine_prep(utofu_vcq_hdl_t vcqh, struct utf_send_cntr *usp);

/*
 * sender engine
 *		
 */
static void
utf_rndz_done(int pos)
{
    struct utf_send_msginfo *minfo = find_rndzprog(pos);
    struct utf_msgreq	*req;

    if (minfo == NULL) {
	utf_printf("%s: YI###### something wrong pos(%d)\n", __func__, pos);
	assert(minfo != NULL);
    }
    req = minfo->mreq;
    DEBUG(DLEVEL_PROTO_RENDEZOUS) {
	utf_printf("%s: RNDZ done pos(%d) req(%p)->vcqid(%lx)\n",
		   __func__, pos, req, req->rgetsender.vcqid[0]);
    }
    if (req->bufinfo.stadd[0]) {
	utf_mem_dereg(utf_info.vcqh, req->bufinfo.stadd[0]);
	req->bufinfo.stadd[0] = 0;
    }
    --minfo->scntr->inflight;
    if (req->notify) req->notify(req, 1);
    if (req->reclaim) {
	req->reclaim = 0;
	utf_sendreq_free(req); /* req->state is reset to REQ_NONE */
    } else {
	req->state = REQ_DONE;
    }
    utfslist_insert(&utf_rndz_freelst, &minfo->slst);
}

#ifdef ENGINE_INLINE
static inline struct utf_send_cntr *
#else
/* Non-inline is a little bit faster than inline one, just 20 nsec */
struct utf_send_cntr *
#endif
sendcontig(struct utf_send_cntr *usp, int ridx, struct utf_packet *pkt,
	    struct utf_send_msginfo *minfo, utofu_vcq_id_t rvcqid, utofu_stadd_t recvstadd,
	    unsigned long flgs)
{
    struct utf_send_cntr *newusp;
    int npckt, pyldsz, fi;
    int	rest = minfo->msghdr.size - usp->usize;
    fi = PKT_MSGFLG(pkt)&MSGHDR_FLGS_FI; 
    if (fi) {
	pyldsz = MSG_FI_PYLDSZ;
    } else {
	pyldsz = MSG_PYLDSZ;
    }
    if ((npckt = can_sendcontig(usp, ridx, rest, pyldsz)) > 0) {
	int	off_mem, off_rest, off_pkt, ssize;
	off_mem = usp->usize;
	off_rest = rest; ssize = 0;
	for (off_pkt = 0; off_pkt < npckt; off_pkt++) {
	    int plsize;
	    if (off_pkt != 0) {
		/* header copy */
		memcpy(&PKT_HDR(pkt + off_pkt), &PKT_HDR(pkt), sizeof(struct utf_msghdr));
	    }
	    plsize = (off_rest >= pyldsz) ? pyldsz : off_rest;
	    /* payload copy */
	    if (fi) {
		pkt[off_pkt].pyld.fi_msg.data = pkt[0].pyld.fi_msg.data; /* fi data */	
		memcpy(&PKT_FI_MSGDATA(pkt + off_pkt), minfo->usrbuf + off_mem, plsize);
	    } else {
		memcpy(&PKT_DATA(pkt + off_pkt), minfo->usrbuf + off_mem, plsize);
	    }
	    PKT_PYLDSZ(pkt + off_pkt) = plsize;
	    ssize += plsize;
	    //utf_printf("%s: off_rest(%d) payload=%d usize(%d)\n", __func__, off_rest, PKT_PYLDSZ(pkt+off_pkt), usp->usize);
	    off_mem += pyldsz; off_rest -= pyldsz;
	}
	newusp = remote_put(utf_info.vcqh, rvcqid, minfo->sndstadd,
			    recvstadd, MSG_PKTSZ*npckt, usp->mypos, flgs, usp);
	usp->recvidx += npckt;
	usp->usize += ssize;
	if (usp->usize == minfo->msghdr.size) {
	    //utf_printf("%s: STATE--> S_DONE_EGR\n", __func__);
	    usp->state = S_DONE_EGR;
	} else {
	    if (usp->usize > minfo->msghdr.size) {
		utf_printf("%s: internal error: usp->usize(%d) npckt(%d) ssize(%d) message size(%d)\n", __func__, usp->usize, npckt, ssize, minfo->msghdr.size);
		abort();
	    }
	    //utf_printf("%s: STATE--> S_DO_EGR usize(%d)\n", __func__, usp->usize);
	    usp->state = S_DO_EGR;
	}
	//usp->npckt = npckt;
	//usp->state = S_WAIT_EGRSEND;
    } else {
	utf_printf("%s: Shoud never come here\n", __func__);
	abort();
	newusp = remote_put(utf_info.vcqh, rvcqid, minfo->sndstadd,
			    recvstadd, MSG_PKTSZ, usp->mypos, flgs, usp);
	usp->usize = PKT_PYLDSZ(pkt);
	usp->state = S_DO_EGR;
	usp->recvidx++;
    }
    if (usp->recvidx > COM_RBUF_SIZE) {
	utf_printf("%s: recvidx(%d) exceeds COM_RBUF_SIZE(%d)\n", __func__, usp->recvidx, COM_RBUF_SIZE);
	abort();
    }
    return newusp;
}

#ifdef SENDENGINE_INLINE
static inline struct utf_send_cntr *
#else
struct utf_send_cntr *
#endif
utf_sendengine(struct utf_send_cntr *usp, struct utf_send_msginfo *minfo, uint64_t rslt, int evt)
{
    struct utf_send_cntr *newusp = NULL;

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
	    utf_printf("No room in remote rank %d\n", usp->dst);
	    abort();
	}
	break;
    case S_HAS_ROOM: /* 3 */
    s_has_room:
    {
	struct utf_packet	*pkt = &minfo->sndbuf->pkt[0];
	utofu_vcq_id_t	rvcqid = usp->rvcqid;
	unsigned long	flgs = 0; // usp->flags;
	uint64_t	ridx = utf_sndmgt_get_index(usp->dst, utf_egrmgt);
	utofu_stadd_t	recvstadd;

	recvstadd = calc_recvstadd(usp, ridx);
	DEBUG(DLEVEL_PROTOCOL|DLEVEL_CHND) {
	    utf_printf("%s: [CHND] Going to send rvcqid(%lx) recvidx(%ld) recvstadd(%lx) "
		     "type(%d) sidx(%d) ridx(%d) MSGHDR(%s)\n",
		       __func__, rvcqid, usp->recvidx, recvstadd, minfo->cntrtype, usp->mypos, ridx,
		       pkt2string(pkt, NULL, 0));
	}
	if (recvstadd == 0) {
	    /* state is S_WAIT_BUFREADY */
	    break;
	}
	switch (minfo->cntrtype) {
	case SNDCNTR_BUFFERED_EAGER_PIGBACK:  /* max_piggyback_size == 32B */
	    /* packet data is already filled */
	    newusp = remote_piggysend(utf_info.vcqh, rvcqid, &minfo->sndbuf->pkt,
				      recvstadd, 32, usp->mypos, flgs, usp);
	    usp->recvidx++;
	    usp->usize = PKT_PYLDSZ(pkt);
	    usp->state = S_DONE_EGR;
	    break;
	case SNDCNTR_BUFFERED_EAGER: /* just one packet */
	    /* packet data is already filled */
	    if (PKT_MSGFLG(pkt)&MSGHDR_FLGS_FI) {
		newusp = remote_put(utf_info.vcqh, rvcqid, minfo->sndstadd,
				    recvstadd, PKT_FI_SENDSZ(pkt), usp->mypos, flgs, usp);
	    } else {
		newusp = remote_put(utf_info.vcqh, rvcqid, minfo->sndstadd,
				    recvstadd, PKT_SENDSZ(pkt), usp->mypos, flgs, usp);
	    }
	    usp->recvidx++;
	    usp->usize = PKT_PYLDSZ(pkt);
	    usp->state = S_DONE_EGR;
	    break;
	case SNDCNTR_INPLACE_EAGER1:  /* packet data is already filled in the first packet */
	case SNDCNTR_INPLACE_EAGER2:  /* packet data is already filled in the first packet  */
	    /* usp->usize and usp->state are set by sendcontig */
	    usp->usize = 0;
	    newusp = sendcontig(usp, ridx, pkt, minfo, rvcqid, recvstadd, flgs);
	    break;
	case SNDCNTR_RENDEZOUS:
	    {
		struct utf_send_msginfo *cpyinfo = utf_rndzprog_alloc();
		int	mypos;
		mypos = cpyinfo->mypos;
		/* rendezvous point is set now */
		if (PKT_MSGFLG(&minfo->sndbuf->pkt[0]) == 0) { /* utf message */
		    minfo->sndbuf->pkt[0].pyld.rndzdata.rndzpos = mypos;
		} else {
		    minfo->sndbuf->pkt[0].pyld.fi_msg.rndzdata.rndzpos = mypos;
		}
		newusp = remote_put(utf_info.vcqh, rvcqid, minfo->sndstadd,
				    recvstadd, MSG_PKTSZ, usp->mypos, flgs, usp);
		/* copy minfo except mypos, and register it now */
		memcpy(cpyinfo, minfo, sizeof(struct utf_send_msginfo));
		cpyinfo->mypos = mypos;
		utfslist_append(&utf_rndz_proglst, &cpyinfo->slst);
	    }
	    /* update send cntr */
	    usp->recvidx++;
	    usp->state = S_REQ_RDVR;
	    // usp->usize = minfo->msghdr.size; // expected all data
	    // usp->rgetwait = 0; usp->rgetdone = 0;
	    DEBUG(DLEVEL_PROTO_RENDEZOUS) {
		utf_printf("%s: rendezous send size(%ld) minfo->sndstadd(%lx) "
			   "src(%d) tag(%d) rndz(%d)\n", __func__,
			   usp->usize, minfo->sndstadd,
			   minfo->sndbuf->pkt[0].hdr.src,
			   minfo->sndbuf->pkt[0].hdr.tag,
			   minfo->sndbuf->pkt[0].hdr.rndz);
	    }
	    break;
	}
	break;
    }
    case S_DO_EGR: /* eager message */
    {
	struct utf_packet	*pkt = &minfo->sndbuf->pkt[0];
	unsigned long	flgs = 0; // usp->flags;
	utofu_vcq_id_t	rvcqid = usp->rvcqid;
	uint64_t	ridx = utf_sndmgt_get_index(usp->dst, utf_egrmgt);
	utofu_stadd_t	recvstadd;

	recvstadd = calc_recvstadd(usp, ridx);
	//utf_printf("%s: MSG DST(%d) S_DO_EGR, msgsize(%ld) sentsize(%ld) recvidx(%d)\n",
	//	   __func__, usp->dst, minfo->msghdr.size, usp->usize, usp->recvidx);
	if (recvstadd == 0) {
	    break;
	}
	//utf_printf("%s: DO EGER\n", __func__);
	newusp = sendcontig(usp, ridx, pkt, minfo, rvcqid, recvstadd, flgs);
#if 0
	size_t		rest = minfo->msghdr.size - usp->usize;
	size_t		ssize;
	/*
	 * copy to eager send buffer. minfo->sndbuf is minfo->sndstadd
	 * in stearing address.
	 */
	if (PKT_MSGFLG(pkt)&MSGHDR_FLGS_FI) {
	    ssize = rest > MSG_FI_PYLDSZ ? MSG_FI_PYLDSZ : rest;
	    memcpy(pkt->pyld.fi_msg.msgdata, minfo->usrbuf + usp->usize, ssize);
	} else {
	    ssize = rest > MSG_PYLDSZ ? MSG_PYLDSZ : rest;
	    memcpy(pkt->pyld.msgdata, minfo->usrbuf + usp->usize, ssize);
	}
	pkt->hdr.pyldsz = ssize;
	DEBUG(DLEVEL_PROTO_EAGER) {
	    utf_printf("%s: S_DO_EGR, mreq(%p) msgsize(%ld) sentsize(%ld) "
		       "rest(%ld) sending sent_size(%ld) send_size(%ld) "
		       "recvstadd(0x%lx) usp->recvidx(%d)\n",
		       __func__, minfo->mreq, minfo->msghdr.size, usp->usize,
		       rest, usp->usize, ssize, recvstadd, usp->recvidx);
	}
	newusp = remote_put(utf_info.vcqh, rvcqid, minfo->sndstadd, recvstadd, MSG_PKTSZ, usp->mypos, flgs, usp);
	usp->usize += ssize;
	usp->recvidx++;
	// utf_printf("%s: DO_EGR: DST(%d) usp(%p) minfo(%p) recvidx(%d) ssize(0x%ld) usize(%ld) micur(%d) cntrtype(%d)\n",
	// __func__, usp->dst, usp, minfo, usp->recvidx, ssize, usp->usize, usp->micur, minfo->cntrtype);
	if (minfo->msghdr.size == usp->usize) {
	    usp->state = S_DONE_EGR;
	}
#endif
	break;
    }
    case S_REQ_RDVR: /* Request has been received by the receiver */
	DEBUG(DLEVEL_PROTO_RENDEZOUS) {
	    utf_printf("%s: Request has been received by the receiver\n", __func__);
	}
	goto next_entry;
    case S_DO_RDVR:
    case S_RDVDONE:
	utf_printf("%s: protocol error. never come in S_DO_RDVR, S_RDVDONE.\n", __func__);
	abort();
	break;
    case S_DO_EGR_WAITCMPL:
	utf_printf("%s: S_DO_EGR_WAITCMPL rcvreset(%d)\n", __func__, usp->rcvreset);
	break;
    case S_WAIT_EGRSEND:
	--usp->npckt;
	utf_printf("%s: usp->npckt(%d)\n", __func__, usp->npckt);
	if (usp->npckt == 0) {
	    usp->state = S_DONE_EGR;
	    goto done_egr;
	}
	break;
    case S_DONE_EGR: done_egr:
    {
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("%s: DONE_EGR: DST(%d) usp(%p) minfo(%p) recvidx(%d) usize(%ld) micur(%d) cntrtype(%d)\n", __func__, usp->dst, usp, minfo, usp->recvidx, usp->usize, usp->micur, minfo->cntrtype);
	}
	{
	    struct utf_msgreq	*req = minfo->mreq;
	    --usp->inflight;
	    if (minfo->cntrtype == SNDCNTR_INPLACE_EAGER2
		&& minfo->usrbuf) {
		utf_free(minfo->usrbuf);
		minfo->usrbuf = NULL;
	    }
	    if (req->notify) req->notify(req, 1);
	    if (req->reclaim) {
		/* This is used by utf */
		req->reclaim = 0;
		utf_sendreq_free(req); /* req->state is reset to REQ_NONE */
	    } else {
		req->state = REQ_DONE;
	    }
	    /*
	     * reset minfo entry, keeping the send buffer
	     *  i.e., no call utf_egr_sbuf_free(minfo->sndbuf);
	     */
	}
    next_entry:
	minfo->mreq = NULL;
	minfo->cntrtype = SNDCNTR_NONE;
	usp->ostate = usp->state;
	usp->state = S_NONE;
	{ /* checking the next entry */
	    int	idx = (usp->micur + 1)%COM_SCNTR_MINF_SZ;
	    if (idx == usp->mient) { /* all entries are sent */
		usp->micur = usp->mient = 0;
		usp->state = S_NONE;
	    } else {
		minfo = &usp->msginfo[idx];
		usp->micur = idx;
		if (minfo->cntrtype != SNDCNTR_NONE) {
		    usp->state = S_HAS_ROOM;
		    evt = EVT_CONT;
		    goto s_has_room;
		} else {
		    utf_printf("%s: usp->micur(%d) usp->mient(%d) minfo->cntrtype(%d)\n",
			       __func__, usp->micur, usp->mient, minfo->cntrtype);
		    assert(minfo->cntrtype != SNDCNTR_NONE);
		}
	    }
	}
	if (usp->state == S_NONE && usp->smode == TRANSMODE_CHND) {
	    int	rc;
	    if (IS_COMRBUF_FULL(usp)) {
		/* MUST WAIT for reseting by the receiver */
		/* In case of TRANSMODE_AGGRESSIVE, utf_scntr_free takes care of
		 * this condition */
		usp->ostate = S_NONE;
		usp->state = S_WAIT_BUFREADY_CHND;
	    } else {
		/* state will be changed to S_WAIT_RESETCHN, S_WAIT_NEXTUPDT, or not */
		rc = chain_progress(usp, 0, EVT_LCL_CHNNXT);
		if (rc != PROG_CHAIN_NONE) {
		    INTERNAL_ERROR;
		}
		if (usp->inflight > 0) {
		    utf_printf("%s: usp->state(%d) usp->inflight(%d)\n", __func__, usp->state, usp->inflight);
		    INTERNAL_ERROR;
		}
	    }
	}
	break;
    }
    case S_DONE:
	break;
    case S_WAIT_BUFREADY:
	utf_printf("%s: protocol error in S_WAIT_BUFREADY\n", __func__);
	abort();
	break;
    }
    return newusp;
}

void
utf_sendengine_prep(utofu_vcq_hdl_t vcqh, struct utf_send_cntr *usp)
{
    do {
	struct utf_send_msginfo	*minfo = &usp->msginfo[usp->micur];
	usp = utf_sendengine(usp, minfo, 0, EVT_LCL);
	--utf_tcq_count;
    } while (usp != NULL);
    assert(utf_tcq_count >= 0);
}

int
utf_send_start(struct utf_send_cntr *usp, struct utf_send_msginfo *minfo)
{
    struct utf_send_cntr *newusp;
    int	dst = usp->dst;
    DEBUG(DLEVEL_CHND) {
	utf_printf("%s: [CHND] DST(%d)\n", __func__, usp->dst);
    }
    if (utf_sndmgt_isset_sndok(dst, utf_egrmgt) != 0) {
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("%s: Has a received room in rank %d: send control(%d)\n",
		     __func__, dst, usp->mypos);
	}
	usp->state = S_HAS_ROOM;
	newusp = utf_sendengine(usp, minfo, 0, EVT_START);
    } else if (utf_sndmgt_isset_examed(dst, utf_egrmgt) == 0) {
	uint64_t	flgs = 0;
	/*
	 * checking availability
	 *   edata: usp->mypos for mrqprogress
	 */
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("%s: Request a room to rank %d: send control(%d)\n"
		     "\tusp(%p)->mypos(%d) rvcqid(%lx) remote stadd(%lx)\n",
		       __func__, dst, usp->mypos, usp, usp->mypos, usp->rvcqid, utf_egr_rbuf_stadd);
	}
	usp->smode = utf_sndmgt_get_smode(usp->dst, utf_egrmgt);
	if (usp->smode == TRANSMODE_CHND) {
	    uint64_t	val = make_chain_addr(utf_info.myrank, usp->mypos, 0);
	    /* initialize */
	    usp->chn_next.rank_sidx = make_chain_addr(-1, usp->mypos, 0);
	    usp->chn_ready.recvidx = 0xffffffff;
	    usp->evtupdt = 0;
	    usp->chn_informed = 0;
	    utf_remote_swap(utf_info.vcqh, usp->rvcqid, flgs,
			    val, EGRCHAIN_RECV_CHNTAIL, usp->mypos, usp);
	} else {
	    newusp = utf_remote_add(utf_info.vcqh, usp->rvcqid,
				    UTOFU_ONESIDED_FLAG_LOCAL_MRQ_NOTICE,
				    (uint64_t) -1, utf_egr_rbuf_stadd, usp->mypos, 0);
	    utf_sndmgt_set_examed(dst, utf_egrmgt);
	}
	usp->state = S_REQ_ROOM;
	return 0;
    } else {
	utf_printf("%s: UTF Internal ERROR!!!!\n", __func__);
	abort();
	return -1;
    }
    if (newusp) {
	utf_sendengine_prep(utf_info.vcqh, newusp);
    }
    return 0;
}


void
utf_tcqprogress()
{
    struct utf_send_cntr	 *usp;
    int	rc;

    do {
	rc = utofu_poll_tcq(utf_info.vcqh, 0, (void*) &usp);
	if (rc != UTOFU_SUCCESS) break;
	if (usp == NULL) continue;
	utf_sendengine_prep(utf_info.vcqh, usp);
    } while (utf_tcq_count);
    if (rc != UTOFU_ERR_NOT_FOUND && rc != UTOFU_SUCCESS) {
	char msg[1024];
	utofu_get_last_error(msg);
	utf_printf("%s: error rc(%d) %s\n", __func__, rc, msg);
	abort();
    }
}

/*
 * receiver engine
 */

void
utf_rget_progress(struct utf_msgreq *req)
{
    utofu_stadd_t	stadd;

    DEBUG(DLEVEL_PROTO_RENDEZOUS) {
	utf_printf("%s: R_DO_RNDZ req(%p)->vcqid(%lx) rsize(%ld) expsize(%ld) rndzpos(%d)\n",
		   __func__, req, req->rgetsender.vcqid[0], req->rsize, req->rcvexpsz, req->rgetsender.rndzpos);
    }
    if (req->rsize != req->rcvexpsz) {
	/* continue to issue */
	rget_continue(req);
	return;
    }
    if (--req->rgetsender.inflight > 0) {
	/* waiting for completion */
	utfslist_append(&utf_rget_proglst, &req->rget_slst);
	return;
    }
    /*
     * notification to the sender: local mrq notification is supressed
     * currently local notitication is enabled
     */
    stadd = MSGINFO_STADDR(req->rgetsender.rndzpos);
    utf_remote_armw4(utf_info.vcqh, req->rgetsender.vcqid[0], 0,
		     UTOFU_ARMW_OP_OR, SCNTR_OK,
		     stadd + MSGINFO_RGETDONE_OFFST, req->rgetsender.rndzpos, 0);
    if (req->bufinfo.stadd[0]) {
	utf_mem_dereg(utf_info.vcqh, req->bufinfo.stadd[0]);
	req->bufinfo.stadd[0] = 0;
    }
    req->state = REQ_DONE;
    if (req->notify) req->notify(req, 1);
}

/* 20201229 */
int
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

int
utf_recvengine(struct utf_recv_cntr *urp, struct utf_packet *pkt, int sidx)
{
    struct utf_msgreq	*req;

#if 0
    if (sidx < 0) {
	sidx = pkt->hdr.sidx;
	utf_printf("%s: CORRECT sidx(%d)\n", __func__, sidx);
	abort();
    }
#endif
    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("%s: urp(%p), ridx(%d) recvidx(%d) state(%s) MSG(%s)\n",
		   __func__, urp, urp->mypos, urp->recvidx, rstate_symbol[urp->state],
		   pkt != NULL ? pkt2string(pkt, NULL, 0) : "NULL");
    }
    switch (urp->state) {
    case R_NONE: /* Begin receiving message */
    {
	int	idx;
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("%s: NEW MESSAGE SRC(%d) size(%ld)\n", __func__, pkt->hdr.src, (uint64_t)pkt->hdr.size);
	}
	if ((idx = utfgen_explst_match(PKT_MSGFLG(pkt), PKT_MSGSRC(pkt), PKT_MSGTAG(pkt))) != -1) {
	found:
	    req = utf_idx2msgreq(idx);
	    req->rndz = pkt->hdr.rndz;
	    req->fi_data = PKT_FI_DATA(pkt);
	    if (PKT_MSGFLG(pkt)&~MSGHDR_FLGS_FI_TAGGED && req->fi_mrecv) {
		/* expected queue is created */
		req->fi_mrecv(req, PKT_MSGSZ(pkt));
	    }
	    if (req->rndz) {
		if (PKT_MSGFLG(pkt) == 0) { /* utf message */
		    memcpy(&req->rgetsender, &pkt->pyld.rndzdata, sizeof(struct utf_vcqid_stadd));
		} else { /* fi msg */
		    memcpy(&req->rgetsender, &pkt->pyld.fi_msg.rndzdata, sizeof(struct utf_vcqid_stadd));
		}
		req->hdr = pkt->hdr;
		req->rcntr = urp;
		rget_start(req);
		urp->req = NULL;	/* disconnected */
		urp->state = R_NONE;
	    } else { /* expected eager */
		req->hdr = pkt->hdr; /* size and tag are needed */
		/* req->rcvexpsz is set by the user */
		if (eager_copy_and_check(urp, req, pkt) == R_DONE) goto done;
		urp->req = req;
	    }
	} else { /* New Unexpected message */
	    if (tfi_tag_explst.head) {
		/* try again to search explected list.  20201229 */
		if ((idx = utfgen_explst_match(PKT_MSGFLG(pkt), PKT_MSGSRC(pkt), PKT_MSGTAG(pkt))) != -1) {
		    goto found;
		}
	    }
	    req = utf_recvreq_alloc();
	    if (req == NULL) {
		/* Cannot handle this message at this time */
		DEBUG(DLEVEL_PROTOCOL) {
		    utf_printf("%s: Unexepected Lack of free utf_msgreq\n", __func__);
		}
		return 1;
	    }
	    req->allflgs = 0;
	    req->fi_data = PKT_FI_DATA(pkt);
	    req->fi_ucontext = 0;
	    req->hdr = pkt->hdr;
	    req->rsize = 0; req->ustatus = 0; req->type = REQ_RECV_UNEXPECTED;
	    req->rndz = pkt->hdr.rndz;
	    /* registering unexpected queue now */
	    utfgen_uexplst_enqueue(pkt->hdr.flgs, req);
	    if (req->rndz) {
		if (PKT_MSGFLG(pkt) == 0) { /* utf message */
		    memcpy(&req->rgetsender, &pkt->pyld.rndzdata, sizeof(struct utf_vcqid_stadd));
		} else { /* fi msg */
		    memcpy(&req->rgetsender, &pkt->pyld.fi_msg.rndzdata, sizeof(struct utf_vcqid_stadd));
		}
		/* req->rcvexpsz will be set at the remote get operation time */
		req->state = REQ_WAIT_RNDZ;
		req->rcntr = urp;
		// utfslist_append(&utf_rget_proglst, &req->rget_slst); NO!
		urp->req = NULL;
		urp->state = R_NONE;
	    } else {/* eager */
		req->buf = utf_malloc(PKT_MSGSZ(pkt));
		req->alloc = 1;
		req->rcvexpsz = PKT_MSGSZ(pkt); /* need to receive all message */
		DEBUG(DLEVEL_PROTOCOL) {
		    utf_printf("%s: EAGER BUF src(%d)alloc(%p) size(%ld)\n", __func__, req->hdr.src, req->buf, req->rcvexpsz);
		}
		/* we have to receive all messages from sender at this time */
		req->state = REQ_NONE;
		if (eager_copy_and_check(urp, req, pkt) == R_DONE) goto done;
		urp->req = req;
		req->rcntr = urp; /* needed to handle MULTI_RECV */
	    }
	}
	goto exiting;
    }
	break;
    case R_BODY:
	req = urp->req;
	utf_nullfunc();
	//getpid();
	//utf_printf("%s: SRC(%d) req->rsize(%ld)\n", __func__, req->hdr.src, PKT_PYLDSZ(pkt));
	//utf_printf("%s: SRC(%d) expsize(%d) rsize(%ld)\n", __func__, req->hdr.src, req->expsize, req->rsize);
	if (eager_copy_and_check(urp, req, pkt) == R_DONE) goto done;
	if (req->overrun) {
	    utf_printf("%s: OVERRUN SRC(%d) rcvexpsize(%d) rsize(%d) recvidx(%d) MSG(%s)\n", __func__,
		       req->hdr.src,  req->rcvexpsz, req->rsize, urp->recvidx, pkt2string(pkt, NULL, 0));
	}
	/* otherwise, continue to receive message */
	break;
    done:
	req->state = REQ_DONE;
	if (req->type == REQ_RECV_UNEXPECTED) {
	    DEBUG(DLEVEL_PROTOCOL) {
		utf_printf("%s: recv_post SRC(%d) UEXP DONE(req=%p,idx=%d,flgs(%x))\n", __func__, req->hdr.src,
			   req, utf_msgreq2idx(req), req->hdr.flgs);
	    }
	} else {
	    DEBUG(DLEVEL_PROTOCOL) {
		utf_printf("%s: recv_post SRC(%d) EXP DONE(req=%p,idx=%d,flgs(%x)), req->fi_flgs(0x%lx) notify(%p)\n", __func__, req->hdr.src,
			   req, utf_msgreq2idx(req), req->hdr.flgs, req->fi_flgs, req->notify);
	    }
	    if (req->notify) req->notify(req, 1);
	}
	/* reset the state */
	urp->state = R_NONE;
	urp->req = NULL;
	break;
    case R_HEAD:
    case R_DONE:
    case R_DO_RNDZ:
    default:
	utf_printf("%s: PROTOCOL ERROR SRC(%d), urp(%p), ridx(%d) recvidx(%d) state(%s) MSG(%s)\n",
		   __func__, pkt->hdr.src, urp, urp->mypos, urp->recvidx, rstate_symbol[urp->state],
		   pkt != NULL ? pkt2string(pkt, NULL, 0) : "NOPKT");
	return -1;
    }
    exiting:
	return 0;
}

int
utf_mrqprogress()
{
    int	ridx = -1;
    int	uc = 0;
    struct utofu_mrq_notice mrq_notice;

    uc = utofu_poll_mrq(utf_info.vcqh, 0, &mrq_notice);
    if (uc == UTOFU_ERR_NOT_FOUND) return ridx;
    if (uc != UTOFU_SUCCESS) {
	char msg[1024];
	utofu_get_last_error(msg);
	utf_printf("%s: utofu_poll_mrq ERROR rc(%d) %s\n", __func__, uc, msg);
	utf_printf("%s: type(%s) vcq_id(0x%lx) edata(0x%x) rmt_value(0x%lx) lcl_stadd(0x%lx) rmt_stadd(0x%lx)\n",
		   __func__, notice_symbol[mrq_notice.notice_type],
		   mrq_notice.vcq_id, mrq_notice.edata, mrq_notice.rmt_value,
		   mrq_notice.lcl_stadd, mrq_notice.rmt_stadd);
	if (mrq_notice.notice_type == UTOFU_MRQ_TYPE_LCL_GET) {
	    struct utf_msgreq	*req;
	    req = find_rget_msgreq(mrq_notice.vcq_id);
	    utf_printf("%s: sidx(%d) src(%d) flgs(0x%x) "
		       "size(%ld) rvcqid(0x%lx) lcl_stadd(0x%lx) rmt_stadd(0x%lx) "
		       "buf(%p) iov_count(%d) iov_base(%p)\n",
		       __func__, req->hdr.src, req->hdr.sidx, req->hdr.flgs,
		       req->rcvexpsz, req->rgetsender.vcqid[0], req->bufinfo.stadd[0],
		       req->rgetsender.stadd[0], req->buf, req->fi_iov_count,
		       req->fi_iov_count == 1 ? req->fi_msg[0].iov_base : NULL);
	    utf_rget_progress(req);
	}
	abort();
	return ridx;
    }
    DEBUG(DLEVEL_UTOFU) {
	utf_printf("%s: type(%s)\n", __func__,
		 notice_symbol[mrq_notice.notice_type]);
    }
    switch(mrq_notice.notice_type) {
    case UTOFU_MRQ_TYPE_LCL_PUT: /* 0 */
    {	/* Sender side: edata is index of sender engine */
	struct utf_send_cntr *usp;
	int	sidx = mrq_notice.edata;
	struct utf_send_msginfo	*minfo;

	DEBUG(DLEVEL_PROTO_RMA|DLEVEL_PROTOCOL) {
	    utf_printf("%s: MRQ_TYPE_LCL_PUT: edata(%d) rmt_val(%ld/0x%lx) "
		       "vcq_id(%lx) sidx(0x%x)\n",  __func__, mrq_notice.edata,
		       mrq_notice.rmt_value, mrq_notice.rmt_value,
		       mrq_notice.vcq_id, sidx);
	}
	if (sidx & EDAT_RMA) {
	    /* RMA operation */
	    int	rma_idx = sidx & ~EDAT_RMA;
	    struct utf_rma_cq	*rma_cq;
	    rma_cq = &utf_rmacq_pool[rma_idx];
	    if (rma_cq->notify) {
		rma_cq->notify(rma_cq);
	    } else {
		utf_rmacq_free(rma_cq);
	    }
	    break;
	}
	usp = utf_idx2scntr(sidx);
	minfo = &usp->msginfo[usp->micur];
	utf_sendengine(usp, minfo, 0, EVT_LCL);
	break;
    }
    case UTOFU_MRQ_TYPE_RMT_PUT: /* 1 */
    {
	int	sidx = mrq_notice.edata;
	struct utf_send_cntr *usp;
	struct utf_send_msginfo	*minfo;
	int	rc, evtype;

	usp = utf_idx2scntr(sidx);
	minfo = &usp->msginfo[usp->micur];
	if (usp->smode == TRANSMODE_CHND) {
	    /* EVT_RMT_CHUNUPD or EVT_RMT_CHNRDY */
	    int	ostate = usp->state;
	    if (is_scntr(mrq_notice.rmt_value, &evtype) == 0) {
		INTERNAL_ERROR;
	    }
	    rc = chain_progress(usp, 0, evtype);
	    if (rc == PROG_CHAIN_SENDSTART) {
		/*
		 * S_WAIT_RESETCHN and (usp->inflight > 0) --> S_NONE
		 * This happens when a send operation is posted
		 * during wait for completion of reset the receiver's chain-tail
		 * field.
		 */
		utf_send_start(usp, minfo);
	    } else if (rc == PROG_CHAIN_SENDENGINE) {
		/* S_REQ_ROOM --> S_HAS_ROOM */
		utf_sendengine(usp, minfo, 0, EVT_LCL);
	    } else {
		/*
		 * S_WAIT_NEXTUPDT --> S_NONE triggered by EVT_RMT_CHNUPDT(9)
		 * 
		 */
		/* must be removed 2021/01/27 */
		if (!(usp->state == S_REQ_ROOM && usp->inflight > 0)
		    && !(usp->state == S_WAIT_RESETCHN && evtype == EVT_RMT_CHNUPDT)
		    && !(usp->state == S_DONE_EGR && evtype == EVT_RMT_CHNUPDT)
		    && !(ostate == S_WAIT_NEXTUPDT && evtype == EVT_RMT_CHNUPDT)) {
		    utf_printf("%s: RMT_PUT evtype(%d) rc(%d) usp->state(%d) usp->inflight(%d)\n", __func__, evtype, rc, usp->state, usp->inflight);
		    INTERNAL_ERROR;
		}
	    }
	} else { /* never comes */
	    utf_printf("%s: RMT_PUT usp->state(%d) usp->inflight(%d)\n", __func__, usp->state, usp->inflight);
	    INTERNAL_ERROR;
	}
#if 0
	/* rmt_vlue is the last stadd address of put data */
	ridx = (mrq_notice.rmt_stadd
		- utf_egr_rbuf_stadd
		- sizeof(union utf_erv_head))/(sizeof(struct utf_packet)*COM_RBUF_SIZE);
	{
	    struct utf_packet	 *msgbase = utf_recvbuf_get(ridx);
	    struct utf_recv_cntr *urp = &utf_rcntr[ridx];
	    struct utf_packet	*pkt = msgbase + urp->recvidx;
	    static int i = 0;
	    //utf_printf("%s: ridx(%d) urp(%p) SRC(%d) recvidx(%d)\n", 
	    //__func__, ridx, urp, urp->req == NULL ? -1 : urp->req->hdr.src, urp->recvidx);
	}
#endif
	break;
    }
    case UTOFU_MRQ_TYPE_LCL_GET: /* 2 */
    {
	int	sidx = mrq_notice.edata;
	struct utf_msgreq	*req;

	if (sidx & EDAT_RMA) {
	    int	rma_idx = sidx & ~EDAT_RMA;
	    struct utf_rma_cq	*rma_cq;
	    rma_cq = &utf_rmacq_pool[rma_idx];
	    if (rma_cq->notify) {
		rma_cq->notify(rma_cq);
	    } else {
		utf_rmacq_free(rma_cq);
	    }
	    break;
	} else {
	    req = find_rget_msgreq(mrq_notice.vcq_id);
	    assert(req != 0);
	    DEBUG(DLEVEL_UTOFU) {
		utf_printf("%s: MRQ_TYPE_LCL_GET: edata(%d) "
			   "lcl_stadd(%lx) rmt_stadd(%lx) req(%p) sidx(%d)\n",
			   __func__, mrq_notice.edata,
			   mrq_notice.lcl_stadd, mrq_notice.rmt_stadd,
			   req, req->hdr.sidx);
	    }
	    utf_rget_progress(req);
	}
	break;
    }
    case UTOFU_MRQ_TYPE_LCL_ARMW:/* 4 */
    {	/* Sender side: result of
	 *		   utf_send_start() or
	 *		   utf_reset_recv_chntail()
	 * rmt_value is a result of this atomic operation.
	 * edata is index of sender engine */
	uint32_t	sidx = mrq_notice.edata;
	struct utf_send_cntr *usp;
	struct utf_send_msginfo	*minfo;
	//int	evtype;
	usp = utf_idx2scntr(sidx);
	minfo = &usp->msginfo[usp->micur];
	DEBUG(DLEVEL_UTOFU) {
	    utf_printf("%s: MRQ_LCL_ARM: edata(%d) rmt_val(%ld) usp(%p)\n",
		     __func__, mrq_notice.edata, mrq_notice.rmt_value, usp);
	}
	if (usp->smode == TRANSMODE_CHND) {
	    int rc, ostate = usp->state;
	    /* utf_send_start() or utf_reset_recv_chntail() */
	    rc = chain_progress(usp, mrq_notice.rmt_value, EVT_LCL_EVT_ARM);
	    if (rc == PROG_CHAIN_SENDENGINE) {
		/* S_REQ_ROOM --> S_HAS_ROOM */
		utf_sendengine(usp, minfo, 0, EVT_LCL);
	    } else if (rc == PROG_CHAIN_SENDSTART) {
		utf_send_start(usp, minfo);
	    } else {
		/* keeping the state S_REQ_ROOM */
		if (!(usp->state == S_REQ_ROOM && usp->inflight > 0)
		    && !(usp->state == S_WAIT_NEXTUPDT && ostate == S_WAIT_RESETCHN)
		    && !(usp->state == S_NONE && ostate == S_WAIT_RESETCHN)) {
		    /* S_WAIT_NEXTUPDT */
		    utf_printf("%s: LCL_ARMW usp->state(%d) ostate(%d) usp->inflight(%d)\n", __func__, usp->state, ostate, usp->inflight);
		    INTERNAL_ERROR;
		}
	    }
	} else {
	    /* utf_send_start() */
	    utf_sendengine(usp, minfo, mrq_notice.rmt_value, EVT_LCL);
	}
	break;
    }
    case UTOFU_MRQ_TYPE_RMT_ARMW:/* 5 */
    {
	int	evtype = -1;
	/* Sender side: rmt_value is the stadd address changed by the remote
	 * edata represents the sender's index of sender engine.
	 *  event type is only EVT_RMT_RECVRST
	 */
	if (is_scntr(mrq_notice.rmt_value, &evtype)) {
	    /* access my send control structure */
	    int			sidx = mrq_notice.edata;
	    struct utf_send_cntr *usp;
	    struct utf_send_msginfo	*minfo;
	    usp = utf_idx2scntr(sidx);
	    minfo = &usp->msginfo[usp->micur];
	    if (evtype != EVT_RMT_RECVRST) {
		utf_printf("%s: TOFU ???? evtype(%d)\n", __func__, evtype);
		INTERNAL_ERROR;
	    }
	    if (usp->smode == TRANSMODE_AGGR) {
		usp->rcvreset = 0; usp->recvidx = 0;
		if (usp->state == S_WAIT_BUFREADY) {
		    usp->state = usp->ostate;
		    utf_sendengine(usp, minfo, mrq_notice.rmt_value, evtype);
		}
	    } else if (usp->smode == TRANSMODE_CHND) {
		/* recvidx is reset */
		usp->rcvreset = 0; usp->recvidx = 0;
		if (usp->state == S_WAIT_BUFREADY) {
		    usp->state = usp->ostate;
		    utf_sendengine(usp, minfo, mrq_notice.rmt_value, evtype);
		} else if (usp->state == S_WAIT_BUFREADY_CHND) {
		    int rc;
		    rc = chain_progress(usp, 0, EVT_LCL_CHNNXT);
		    if (rc == PROG_CHAIN_SENDENGINE) {
			/* during waiting for BUFREADY, a new send post
			 * has been posted 2021/01/26 */
			utf_sendengine(usp, minfo, 0, EVT_LCL);
		    } else if (rc != PROG_CHAIN_NONE) {
			utf_printf("%s: RMT_ARMW rc(%d) usp->state(%d) usp->inflight(%d)\n", __func__, rc, usp->state, usp->inflight);
			INTERNAL_ERROR;
		    }
		}
	    }
	} else if (is_msginfo(mrq_notice.rmt_value)) {
	    int	pos = mrq_notice.edata;
	    utf_rndz_done(pos);
	} else {
	    /* receive buffer is accessedd by the sender */
	    utf_printf("%s: MRQ_RMT_ARMW where ? rmt_value(%lx) rmt_stadd(%lx) utf_sndctr_stadd(%lx) evtype(%d)\n",
		       __func__, mrq_notice.rmt_value, mrq_notice.rmt_stadd, utf_sndctr_stadd, evtype);
	}
	break;
    }
    case UTOFU_MRQ_TYPE_RMT_GET: /* 3 */
    default:
	utf_printf("%s: ERROR Unused notice type(%d): edata(%d) rmt_value(%ld)\n",
		   __func__, mrq_notice.notice_type, mrq_notice.edata, mrq_notice.rmt_value);
	break;
    }
    return ridx;
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
    //extern struct erecv_buf	*erbuf;
    uint64_t		cntr = utf_egr_rbuf.head.cntr;
    int	i;
    utf_printf("***** RECV_CNTR (PEERS: %d) *****\n", RCV_CNTRL_INIT - cntr);
    for (i = COM_PEERS - 1; i > cntr; --i) {
	struct utf_recv_cntr	*urp = &utf_rcntr[i]; 
	if (urp->req) {
	    utf_printf(" r-ridx(%d) r-src(%d) r-recvidx(%d) svcqid(0x%lx) state(%s:%d) req(%p) "
		       " r-msghdr(size(%d) tag(0x%lx) src(%d)\n",
		       i, urp->src, urp->recvidx, urp->svcqid, rstate_symbol[urp->state], urp->state,
		       urp->req, urp->req->hdr.size, urp->req->hdr.tag, urp->req->hdr.src);
	} else {
	    utf_printf(" r-ridx(%d) r-src(%d) r-recvidx(%d) svcqid(0x%lx) state(%s:%d) req(NULL)\n",
		       i, urp->src, urp->recvidx, urp->svcqid, rstate_symbol[urp->state], urp->state);
	}
    }
}

extern uint8_t		utf_rank2scntridx[PROC_MAX]; /* dest. rank to sender control index (sidx) */

void
utf_sendctr_show()
{
    struct utf_send_cntr *usp;
    struct utf_send_msginfo	*minfo;
    int	dst;
    int	ridx;

    utf_printf("*****  SND_CNTRL *****\n");
    for (dst = 0; dst < utf_info.nprocs; dst++) {
	uint8_t	headpos = utf_rank2scntridx[dst];
	int i;
	if (headpos == 0xff) continue;
	usp = &utf_scntr[headpos];
	ridx = utf_sndmgt_get_index(usp->dst, utf_egrmgt);
	utf_printf(" snd-dst(%d) s-ridx(%d) s-recvidx(%d) usp(%p)(state(%s:%d) ostate(%s:%d)"
		   " usize(%d) inflight(%d) micur(%d) mient(%d)"
		   " evtupdt(%d) chn_informed(%d) chn_next(%d:%d:%d) chn_ready(%d) wait-for(%d))\n",
		   dst, ridx, usp->recvidx, usp, sstate_symbol[usp->state], usp->state,
		   sstate_symbol[usp->ostate], usp->ostate,
		   usp->usize, usp->inflight, usp->micur, usp->mient,
		   usp->evtupdt, usp->chn_informed, usp->chn_next.rank, usp->chn_next.sidx, usp->chn_next.recvidx,
		   usp->chn_ready.recvidx, usp->waitfor);
	for (i = 0; i < COM_SCNTR_MINF_SZ; i++) {
	    minfo = &usp->msginfo[i];
	    utf_printf("\tminfo[%d] msghdr(size(%d) tag(0x%lx) req(%p))\n",
		       i, minfo->msghdr.size, minfo->msghdr.tag, minfo->mreq);
	}
    }
}

void
utf_injcnt_show()
{
    utf_printf("***** TOFU INJECTION *****\n");
    utf_printf("\tinjection count : %d\n", utf_injct_count);
    utf_printf("\ttcq count       : %d\n", utf_tcq_count);
    utf_printf("\tmrq count       : %d\n", utf_mrq_count);
}
