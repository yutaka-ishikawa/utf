#include <sys/uio.h>
#include "utf_list.h"
/*
 * Though the edata field of utofu put/get/rma operations is uint64_t,
 * actual length is only 1 byte. If -1 value was passed, the utofu_poll_tcq would
 * returns the error: UTOFU_ERR_TCQ_LENGTH.
 *
 */
#define EDAT_RMA	0x80	/* MSB is used to identify RMA operations */
#define EDAT_RGET	0xff

#pragma pack(1)
struct utf_msghdr { /* 20 Byte (4 + 8 + 8) */
    uint32_t	src;	  /* 32 bit is enoguth for Fugaku */
    uint64_t	tag;	  /* libfabric requirement */
    union {
	struct {
	    uint64_t size:35,  /* 35 bit, 32GiB is enough for Fugaku */
		pyldsz:13, /* size of payload: up to 8KiB */
		rndz: 1,
		flgs: 3,   /* utf(0) or libfabric(1) TAGGED(2) or MSG(0) */
		marker: 4,
		sidx: 8;
	};
	uint64_t hall;
    };
};
#pragma pack()
/* Fabric flags */
#define MSGHDR_FLGS_UTF		0x00
#define MSGHDR_FLGS_FI		0x01
#define MSGHDR_FLGS_FI_TAGGED	0x02

struct utf_fabmsghdr { /* 28 Byte */
    uint32_t	src;	  /* 32 bit is enoguth for Fugaku */
    uint64_t	tag;	  /* libfabric requirement */
    union {
	struct {
	    uint64_t size:35,  /* 35 bit, 32GiB is enough for Fugaku */
		pyldsz:10, /* size of payload */
		rndz: 1,
		type: 2,	/* libfabric or utf message */
		flgs: 4,
		marker: 4,
		sidx: 8;
	};
	uint64_t all;
    };
    uint64_t	data;	/* used for libfabric */
};

struct utf_vcqhdl_stadd {		/* 80B (8 +8*3*3) */
    size_t	nent;			/* */
    uint64_t	recvlen[MSG_NTNI];	/* length */
    uint64_t	vcqhdl[MSG_NTNI];	/* utofu_vcq_hdl_t */
    uint64_t	stadd[MSG_NTNI];	/* utofu_stadd_t */
};

struct utf_vcqid_stadd {		/* 80B (8+8*3+8*3+8*3) */
    uint32_t	rndzpos;		/* position of rendezvous progress structure */
    uint16_t	inflight;		/* used in protocol handling */
    uint16_t	nent;			/* */
    uint64_t	len[MSG_NTNI];		/* length */
    uint64_t	vcqid[MSG_NTNI];	/* utofu_vcq_hdl_t */
    uint64_t	stadd[MSG_NTNI];	/* utofu_stadd_t */
};

#define UTF_MARKER_TAIL
#ifdef UTF_MARKER_TAIL
/* 256 - (20 + 8 + 1) */
#define MSG_FI_PYLDSZ	(MSG_PKTSZ - sizeof(struct utf_msghdr) - sizeof(uint64_t) - sizeof(uint8_t))
#else
/* 256 - (20 + 8) */
#define MSG_FI_PYLDSZ	(MSG_PKTSZ - sizeof(struct utf_msghdr) - sizeof(uint64_t))
#endif /* UTF_MARKER_TAIL */
#pragma pack(1)
struct fi_1stpacket {
    uint64_t	data;
#ifdef UTF_MARKER_TAIL
    uint8_t	mrk_tail;
#endif /* UTF_MARKER_TAIL */
    union {
	uint8_t	msgdata[MSG_FI_PYLDSZ];		/* 228B */
	struct utf_vcqid_stadd	rndzdata;	/*  80B */
    };
};
#pragma pack()

#define MSG_PYLDSZ	(MSG_PKTSZ - sizeof(struct utf_msghdr))
#pragma pack(1)
struct utf_packet {
    struct utf_msghdr	hdr;
    union {
	union {
	    uint8_t		msgdata[MSG_PYLDSZ];
	    struct utf_vcqid_stadd	rndzdata;
	};
	struct fi_1stpacket	fi_msg;
    } pyld;
};
#pragma pack()

#define PKT_HDR(pkt) ((pkt)->hdr)
#define PKT_MSGSRC(pkt) ((pkt)->hdr.src)
#define PKT_MSGTAG(pkt) ((pkt)->hdr.tag)
#define PKT_MSGFLG(pkt) ((pkt)->hdr.flgs)
#define PKT_MSGSZ(pkt) ((pkt)->hdr.size)
#define PKT_DATA(pkt) ((pkt)->pyld.msgdata)
#define PKT_PYLDSZ(pkt) ((pkt)->hdr.pyldsz)
#define PKT_RADDR(pkt)  ((pkt)->pyld.rndzdata)
#define PKT_SENDSZ(pkt) ((pkt)->hdr.pyldsz + sizeof(struct utf_msghdr))


#define PKT_FI_DATA(pkt) ((pkt)->pyld.fi_msg.data)
#define PKT_FI_MSGDATA(pkt) ((pkt)->pyld.fi_msg.msgdata)
#define PKT_FI_RADDR(pkt)  ((pkt)->pyld.fi_msg.rndzdata)
#ifdef UTF_MARKER_TAIL
#define PKT_FI_SENDSZ(pkt) ((pkt)->hdr.pyldsz + sizeof(struct utf_msghdr) + sizeof(uint64_t) + sizeof(uint8_t))
#else
#define PKT_FI_SENDSZ(pkt) ((pkt)->hdr.pyldsz + sizeof(struct utf_msghdr) + sizeof(uint64_t))
#endif /* UTF_MARKER_TAIL */

struct utf_egr_sbuf {
    union {
	utfslist_entry_t	slst;
	struct utf_packet	pkt[COM_EGR_PKTSZ];
    };
};

#define SCNTR_NOROOM	-1
#define SCNTR_HEAD	0
#define SCNTR_TAIL	1
#define SCNTR_OK	1

union utf_chain_addr {
    struct {
	uint64_t rank:24,	/* = 16,777,216 > 7,962,624 > 663,552 */
		 sidx:8,	/* */
		 recvidx:32;	/* */
    };
    uint64_t	      rank_sidx;
};
#define RANK_ALL1	0xffffff /* 24 bit */
#define IS_CHAIN_EMPTY(val)    ((val).rank == RANK_ALL1)

union utf_chain_rdy {
    struct {
	uint64_t	rdy:1,
		recvidx:63;
    };
    uint64_t	data;
};

/* receive management */
union utf_erv_head {
    struct {
	uint64_t cntr;
	union utf_chain_addr chntail;	/* tail address of request chain */
    };
    char	 pad[256];
};
struct utf_egr_rbuf {
    union utf_erv_head	head;
    struct utf_packet	rbuf[COM_RBUF_TOTSZ];	/* 50 MiB: COM_RBUF_SIZE*COM_PEERS */
};

/*
 * req->status
 */
enum utq_reqstatus {
    REQ_NONE		= 0,
    REQ_PRG_NORMAL	= 1,
    REQ_DONE		= 3,
    REQ_WAIT_RNDZ	= 4,		/* waiting rendzvous */
    REQ_DO_RNDZ		= 5
};
//    REQ_PRG_RECLAIM	= 2,

/*
 * request type (XXXX recv_ctr state)
 */
enum {
    REQ_RECV_EXPECTED		= 1,
    REQ_RECV_UNEXPECTED		= 2,
    REQ_RECV_EXPECTED2		= 3, /* this value is used to distingush recv/send request */
    REQ_SND_BUFFERED_EAGER	= 4,
    REQ_SND_INPLACE_EAGER	= 5,
    REQ_SND_RENDEZOUS		= 6
};

enum rstate {
    R_FREE		= 0,
    R_NONE		= 1,
    R_HEAD		= 2,
    R_BODY		= 3,
    R_WAIT_RNDZ		= 4,
    R_DO_RNDZ		= 5,
    R_DO_READ		= 6,
    R_DO_WRITE		= 7,
    R_DONE		= 8
};

/*                 +--------------+----------------------+----------+-----------------+
 *		   | usrreqsz     |  hdr.size            | rcvexpsz |rsize(progress)  |
 *                 +--------------+----------------------+----------+-----------------+
 * eager expected  | defined      |set at matching.      | defined  | During transfer,|
 *                 |              |receiving this size   |          | rsize is a real |
 *                 +--------------+----------------------+----------+ data trans size.|
 * eager unexpected| undef and set|set at msg arrival.   | tempolary| Comparing it    |
 *                 | at matching  |receiving this size   | set.     | with hdr.size.  |
 *                 |              |                      | hdr.size | copy size is    |
 *                 |              |                      | is copied| rcvexpsz.       |
 *                 +--------------+----------------------+----------+-----------------+
 * rendz expected  | defined      |set at matching.      |          | During transfer,|
 *                 |              |No-use during transfer| Use for  | rsize is a real |
 *                 +--------------+--------------+-------+ rmt-get  | data trans size.|
 * rendz unexpected| undef and set|set at msg arrival.   | opertion | Comparing it    |
 *                 | at matching  |No-use during transfer|          | with rcvexpsz   |
 *                 +--------------+----------------------+----------+-----------------+
 */
struct utf_msgreq {
    struct utf_msghdr hdr;	/* 28: message header, size field is sender's one */
    uint8_t	*buf;		/* 32: buffer address */
    union {
	struct {
	    uint64_t	rsize:35,	/* 40: received size */
			state: 8,	/* 40: utf-level  status */
			ustatus: 6,	/* 40: user-level status */
			overrun:1,
			reclaim:1,
			rndz:1,		/* 40: set if rendezvous mode */
			type:3,		/* 40: EXPECTED or UNEXPECTED or SENDREQ */
			ptype:4,	/* 40: PKT_EAGER | PKT_RENDZ | PKT_WRITE | PKT_READ */
			alloc:1,	/* 40: dynamically memory is allocated */
			fireqhead:1,	/* 40: fabric-level head of multi-recv req */
			fimrec:1,	/* 40: fabric-level multi-recv state */
			fistate:2;	/* 40: fabric-level status */
	};
	uint64_t	allflgs;
    };
    size_t	rcvexpsz;	/* 48: expected receive size used in rendezvous */
    size_t	usrreqsz;	/* 48: user request size */
    utfslist_entry_t slst;	/* 56: list */
    utfslist_entry_t busyslst;	/* XX: list */
    int		(*notify)(struct utf_msgreq*, int aux);	/* 64: notifier */
    struct utf_recv_cntr *rcntr;	/* 72: point to utf_recv_cntr */
    struct utf_vcqid_stadd rgetsender; /* 112: rendezous: sender's stadd's and vcqid's */
    struct utf_vcqhdl_stadd bufinfo; /* 152: rendezous: receiver's stadd's and vcqid's  */
    utfslist_entry_t	rget_slst;/* rendezous: list of rget progress */
    /* for Fabric */
    int		(*fi_mrecv)(struct utf_msgreq*, size_t sz);
    struct utf_msgreq	*fi_nxtrq;	/* used in multi-rec implementation */
    size_t	fi_min_mrecv;
    uint64_t	fi_data;
    void	*fi_ctx;
    void	*fi_ucontext;
    uint64_t	fi_ignore;
    uint64_t	fi_flgs;
    size_t	fi_iov_count;
    struct iovec fi_msg[4];	/* TOFU_IOV_LIMIT */
    size_t	fi_recvd;	/* size of received data */
    struct utf_msghdr fi_svdhdr;
};

struct utf_msglst {
    utfslist_entry_t	slst;	/*  8 B */
    struct utf_msghdr	hdr;	/* 24 B */
    uint32_t		reqidx;	/* 28 B: index of utf_msgreq */
    /* for Fabric */
    uint64_t	fi_ignore;	/* ignore */
//    uint64_t	fi_flgs;
};

struct utf_recv_cntr {
    uint32_t	recvidx;
    struct utf_msgreq	*req;
    utofu_vcq_id_t	svcqid;
    uint64_t		flags;
    uint8_t	state;
    uint8_t	mypos;
    uint32_t	src;
    /* for debugging */
    uint32_t	dbg_idx;
    uint8_t	dbg_rsize[COM_RBUF_SIZE];
};

/*****************************************************
 *	Sender Control
 *****************************************************/
enum {
    S_FREE		= 0,
    S_NONE		= 1,
    S_REQ_ROOM		= 2,
    S_HAS_ROOM		= 3,
    S_DO_EGR		= 4,
    S_DO_EGR_WAITCMPL	= 5,
    S_DONE_EGR		= 6,
    S_REQ_RDVR		= 7,
    S_DO_RDVR		= 8,
    S_RDVDONE		= 9,
    S_DONE		= 10,
    S_WAIT_BUFREADY	= 11,
    S_WAIT_EGRSEND	= 12,
    S_WAIT_RESETCHN	= 13,
    S_WAIT_NEXTUPDT	= 14,
    S_WAIT_BUFREADY_CHND= 15,
};

enum {
    SNDCNTR_NONE		   = 0,
    SNDCNTR_BUFFERED_EAGER_PIGBACK = 1,
    SNDCNTR_BUFFERED_EAGER	   = 2,
    SNDCNTR_INPLACE_EAGER1	   = 3,
    SNDCNTR_INPLACE_EAGER2	   = 4,
    SNDCNTR_RENDEZOUS		   = 5
};

/*
 * Event types for handling messages
 */
#define EVT_START	0
#define EVT_CONT	1
#define EVT_LCL		2
#define EVT_LCL_CHNNXT	3
#define EVT_LCL_EVT_ARM	4
#define EVT_RMT_RGETDON	5	/* remote armw operation */
#define EVT_RMT_RECVRST	6	/* remote armw operation */
#define EVT_RMT_GET	7	/* remote get operation */
#define EVT_RMT_CHNRDY	8	/* ready in request chain mode */
#define EVT_RMT_CHNUPDT	9	/* next chain is set by remote */
#define EVT_END		10

struct utf_send_msginfo { /* msg info */
    uint32_t		rgetdone;
    uint16_t		cntrtype;
    uint16_t		mypos;
    struct utf_msghdr	msghdr;		/* message header     +28 = 28 Byte */
    struct utf_egr_sbuf	*sndbuf;	/* send data for eger  +8 = 40 Byte */
    utofu_stadd_t	sndstadd;	/* stadd of sndbuf     +8 = 48 Byte */
    void		*usrbuf;	/* stadd of user buf   +8 = 56 Byte */
    struct utf_vcqid_stadd rgetaddr;	/* for rendezvous     +40 = 96 Byte
					 * stadd and vcqid for rget by dest, expose it to dest */
    struct utf_msgreq	*mreq;		/* request struct      +8 =104 Byte */
    struct utf_send_cntr *scntr;
//QWE    void		*fi_context;	/* For fabric */
    utfslist_entry_t	slst;
};

/* used for remote operation */
#define MSGINFO_RGETDONE_OFFST		0x0
//#define SCNTR_RGETDONE_OFFST		0x0
#define SCNTR_RST_RECVRESET_OFFST	0x4
#define SCNTR_RST_RMARESET_OFFST	0x8
#define SCNTR_CHN_NEXT_OFFST		0x10
#define SCNTR_CHN_READY_OFFST		0x18

#define MSGINFO_STADDR(pos)	\
    (utf_rndz_stadd + sizeof(struct utf_send_msginfo)*(pos))

#define SCNTR_ADDR_CNTR_FIELD(sidx)				\
    (utf_sndctr_stadd + sizeof(struct utf_send_cntr)*(sidx))
#define SCNTR_IS_RGETDONE_OFFST(off)	((off) == SCNTR_RGETDONE_OFFST)
#define SCNTR_IS_RECVRESET_OFFST(off)	((off) == SCNTR_RST_RECVRESET_OFFST)
#define SCNTR_IS_CHN_NEXT_OFFST(off)	((off) == SCNTR_CHN_NEXT_OFFST + sizeof(uint64_t))
#define SCNTR_IS_CHN_RDY_OFFST(off)	((off) == SCNTR_CHN_READY_OFFST + sizeof(uint64_t))

struct utf_sndctr_svd {
    uint32_t	valid:1,	/* valid or not this entry */
		recvidx: 31;	/* saved recvidx */
};

#pragma pack(1)
struct utf_send_cntr {	/* 500 Byte */
    uint32_t		rgetdone:1,	/* 0: ready for resetting recv offset */
			ineager: 1,	/* position depends */
			rgetwait:2,	/* */
			state: 5,	/* upto 31 states */
			ostate: 5,	/* upto 31 states */
			smode: 1,       /* MSGMODE_CHND or MSGMODE_AGGR 2021/01/23 */
			evtupdt: 1,     /* receive event update  2021/01/23 */
			expevtupdt: 1,  /* expect receiving RMT_CHNUPDT event  2021/01/23 */
			chn_informed:1, /* chain_inform_ready issued  2021/01/23 */
			mypos: 14;	/* */
    uint32_t		rcvreset: 1,	/* position depends */
			recvidx: 31;	/*  +4 =  8 Byte */
    uint32_t		rmareset: 1,	/* position depends. ready for reseting */
			rmawait: 1,	/* Wait for reseting */
			rmaoff: 30;	/*  */
    uint32_t		dst;		/* 12  = +4  Byte */
    union utf_chain_addr chn_next;	/* position depends. 16 = +8 24 Byte */
    union utf_chain_rdy	chn_ready;	/* position depends. 24 = +8 = 32 Byte */
    uint64_t		flags;		/*  32 = +8 Byte */
    utofu_vcq_id_t	svcqid;		/*  40 = +8 Byte */
    utofu_vcq_id_t	rvcqid;		/*  48 = +8 Byte */
    size_t		usize;		/*  56 = +8 user-level sent size */
    utfslist_t		smsginfo;	/*  64 = +8 Byte */
    uint8_t		micur;		/*  72 = +1 Byte */
    uint8_t		mient;		/*  73 = +1 Byte */
    uint8_t		inflight;	/*  74 = +1 Byte */
    uint8_t		npckt;		/*  75 = +1 Byte for eager */
    struct utf_send_msginfo msginfo[COM_SCNTR_MINF_SZ];	/*  76 = +408 the first entry */
    utfslist_entry_t	slst;		/* 482 = + 8 Byte for free list */
					/* 490  */
    /* for debugging */
    uint32_t		waitfor;	/* wait-for chn_ready from rank */
    //uint32_t	dbg_idx;
    //uint8_t	dbg_ssize[COM_RBUF_SIZE];
};
#pragma pack()

/*****************************************************
 * CHAIN MODE
 *****************************************************/
#define EGRCHAIN_RECV_CHNTAIL (utf_egr_rbuf_stadd + (uint64_t) &((struct utf_egr_rbuf*)0)->head.chntail)
#define EGRCHAIN_RECV_CNTR (utf_egr_rbuf_stadd + (uint64_t) &((struct utf_egr_rbuf*)0)->head.cntr)
#define SCNTR_CHAIN_CNTR(pos)					\
    (utf_sndctr_stadd + sizeof(struct utf_send_cntr)*(pos))
#define SCNTR_CHN_NEXT_OFFST		0x10	/* 16B */
#define SCNTR_CHN_READY_OFFST		0x18	/* 24B */
#define SCNTR_CHAIN_NXT(pos)					\
    (SCNTR_CHAIN_CNTR(pos) + SCNTR_CHN_NEXT_OFFST)
#define SCNTR_CHAIN_READY(pos)					\
    (SCNTR_CHAIN_CNTR(pos) + SCNTR_CHN_READY_OFFST)

/*****************************************************
 *	RMA Completion
 *****************************************************/
#define UTF_RMA_READ	1
#define UTF_RMA_WRITE	2
#define UTF_RMA_WRITE_INJECT 3
struct utf_rma_cq {
    utfslist_entry_t	slst;
    struct tofu_ctx	*ctx;
    utofu_vcq_hdl_t	vcqh;
    utofu_vcq_id_t	rvcqid;
    utofu_stadd_t	lstadd;
    utofu_stadd_t	rstadd;
    void		*lmemaddr;
    uint64_t		addr;
    ssize_t		len;
    uint64_t		data;
    void		(*notify)(struct utf_rma_cq *);
    int			type;
    int			mypos;
    uint64_t		utf_flgs;
    uint64_t		fi_flags;
    void		*fi_ctx;
    void		*fi_ucontext;
    uint8_t		inject[TOFU_INJECTSIZE];
};

extern utfslist_t	utf_explst;	/* expected message list */
extern utfslist_t	utf_uexplst;	/* unexpected message list */
extern utfslist_t	tfi_tag_explst;	/* fi: expected tagged message list */
extern utfslist_t	tfi_tag_uexplst;/* fi: unexpected tagged message list */
extern utfslist_t	tfi_msg_explst;	/* fi: expected message list */
extern utfslist_t	tfi_msg_uexplst;/* fi: unexpected message list */
extern utfslist_t	utf_msglst_freelst;


/* utf_msglst_free is defined here because static inline functions in this header
 * use this function */
static inline void
utf_msglst_free(struct utf_msglst *msl)
{
    utfslist_insert(&utf_msglst_freelst, &msl->slst);
}

static inline int
utf_uexplst_match(uint32_t src, uint32_t tag, int peek)
{
    struct utf_msglst	*msl;
    uint32_t		idx;
    utfslist_entry_t	*cur, *prev;

    if (utfslist_isnull(&utf_uexplst)) {
	return -1;
    }
    if ((src == -1) && (tag == -1)) {
	cur = utf_uexplst.head; prev = 0;
	msl = container_of(cur, struct utf_msglst, slst);
	goto find;
    } else if ((src == -1) && (tag != -1)) {
	utfslist_foreach2(&utf_uexplst, cur, prev) {
	    msl = container_of(cur, struct utf_msglst, slst);
	    if (tag == msl->hdr.tag) {
		goto find;
	    } 
	}
    } else if (tag == -1) {
	utfslist_foreach2(&utf_uexplst, cur, prev) {
	    msl = container_of(cur, struct utf_msglst, slst);
	    if (src == msl->hdr.tag) {
		goto find;
	    } 
	}
    } else {
	utfslist_foreach2(&utf_uexplst, cur, prev) {
	    msl = container_of(cur, struct utf_msglst, slst);
	    if (src == msl->hdr.src && tag == msl->hdr.tag) {
		goto find;
	    } 
	}
    }
    return -1;
find:
    idx = msl->reqidx;
    if (peek == 0) {
	utfslist_remove2(&utf_uexplst, cur, prev);
	utf_msglst_free(msl);
    }
    return idx;
}

static inline int
utf_explst_match(uint32_t src, uint32_t tag, int peek)
{
    struct utf_msglst	*msl;
    utfslist_entry_t	*cur, *prev;
    uint32_t		idx;

    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("explst_match: utf_explst(%p) src(%d) tag(%d)\n",
		 &utf_explst, src, tag);
    }
    if (utfslist_isnull(&utf_explst)) {
	return -1;
    }
    utfslist_foreach2(&utf_explst, cur, prev) {
	msl = container_of(cur, struct utf_msglst, slst);
	uint32_t exp_src = msl->hdr.src;
	uint32_t exp_tag = msl->hdr.tag;
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("\t exp_src(%d) exp_tag(%x)\n", exp_src, exp_tag);
	}
	if (exp_src == -1 && exp_tag != -1) {
	    goto find;
	} else if (exp_src == -1 && exp_tag == tag) {
	    goto find;
	} else if (exp_tag == -1 && exp_src == src) {
	    goto find;
	} else if (exp_src == src && exp_tag == tag) {
	    goto find;
	}
    }
    return -1;
find:
    idx = msl->reqidx;
    if (peek == 0) {
	utfslist_remove2(&utf_explst, cur, prev);
	utf_msglst_free(msl);
    }
    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("\t return idx(%d)\n", idx);
    }
    return idx;
}

static inline int
tfi_utf_uexplst_match(utfslist_t *uexplst, uint32_t src, uint64_t tag, uint64_t ignore, int peek)
{
    struct utf_msglst	*msl;
    utfslist_entry_t	*cur, *prev;
    uint32_t		idx;

    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("tfi_utf_uexplst_match: explst(%p) src(%d) tag(0x%lx) ignore(0x%lx)\n", uexplst, src, tag, ignore);
    }
    if (utfslist_isnull(uexplst)) {
	//utf_printf("\t: list is null\n");
	return -1;
    }
    if (src == -1) {
	utfslist_foreach2(uexplst, cur, prev) {
	    msl = container_of(cur, struct utf_msglst, slst);
	    if ((tag & ~ignore) == (msl->hdr.tag & ~ignore)) {
		goto find;
	    }
	}
    } else {
	utfslist_foreach2(uexplst, cur, prev) {
	    msl = container_of(cur, struct utf_msglst, slst);
	    DEBUG(DLEVEL_PROTOCOL) {
		utf_printf("\t msl(%p) src(%d) tag(%x) rvignr(%lx)\n", msl, msl->hdr.src, msl->hdr.tag, ~ignore);
	    }
	    if ((src == msl->hdr.src) &&
		((tag & ~ignore) == (msl->hdr.tag & ~ignore))) {
		goto find;
	    }
	}
    } /* not found */
    //utf_printf("\t: not found\n");
    return -1;
find:
    idx = msl->reqidx;
    //utf_printf("\t: found idx(%d)\n", idx);
    if (peek == 0) {
	utfslist_remove2(uexplst, cur, prev);
	utf_msglst_free(msl);
    }
    return idx;
}

static inline int
tfi_utf_explst_match(utfslist_t *explst, uint32_t src, uint64_t tag,  int peek)
{
    struct utf_msglst	*mlst;
    utfslist_entry_t	*cur, *prev;
    uint32_t		idx;

    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("tfi_utf_explst_match: explst(%p) src(%d) tag(0x%lx)\n", explst, src, tag);
    }
    if (utfslist_isnull(explst)) {
	return -1;
    }
    utfslist_foreach2(explst, cur, prev) {
	mlst = container_of(cur, struct utf_msglst, slst);
	uint32_t exp_src = mlst->hdr.src;
	uint64_t exp_tag = mlst->hdr.tag;
	uint64_t exp_rvignr = ~mlst->fi_ignore;
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("\t mlst(%p) exp_src(%d) exp_tag(%lx) exp_rvignr(%lx) exp1(%d) exp2(%d) match1(%d) match2(%d)\n",
		       mlst, exp_src, exp_tag, exp_rvignr,
		       exp_src == -1, (tag & exp_rvignr) == (exp_tag & exp_rvignr),
		       (exp_src == -1 && ((tag & exp_rvignr) == (exp_tag & exp_rvignr))),
		       (exp_src == src && ((tag & exp_rvignr) == (exp_tag & exp_rvignr))));
	}
	if ((exp_src == -1) && ((tag & exp_rvignr) == (exp_tag & exp_rvignr))) {
	    goto find;
	} else if ((exp_src == src)
		   && ((tag & exp_rvignr) == (exp_tag & exp_rvignr))) {
	    goto find;
	}
    }
    return -1;
find:
    idx = mlst->reqidx;
    //utf_printf("\t return idx(%d) mlst(%p) exp_src(%ld) exp_tag(0x%lx) exp_ignore(0x%lx)\n", idx, mlst, mlst->hdr.src, mlst->hdr.tag, mlst->fi_ignore);
    if (peek == 0) {
	DEBUG(DLEVEL_LOG2) {
	    extern int tofu_gatherv_cnt, tofu_gatherv_do;
	    if (tofu_gatherv_do) {
		utf_log("\tG(%d)A(%d:%d) ", tofu_gatherv_cnt, src, mlst->hdr.src);
	    }
	}
	utfslist_remove2(explst, cur, prev);
	utf_msglst_free(mlst);
    }
    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("\t return idx(%d)\n", idx);
    }
    return idx;
}
