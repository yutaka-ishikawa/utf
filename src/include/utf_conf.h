#include <assert.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifdef UTF_DEBUG
#define DEBUG(mask) if (utf_dflag&(mask))
#else
#define DEBUG(mask) if (0)
#endif
#define DLEVEL_PMIX		0x1
#define DLEVEL_UTOFU		0x2
#define DLEVEL_PROTOCOL		0x4
#define DLEVEL_PROTO_EAGER	0x8
#define DLEVEL_PROTO_RENDEZOUS	0x10
#define DLEVEL_ALL		0xffff
#define DLEVEL_PROTO_RMA	0x20	/* 32 */
#define DLEVEL_PROTO_VBG	0x40	/* 64 */
#define DLEVEL_PROTO_AM		0x80	/* 128 */
#define DLEVEL_ADHOC		0x100	/* 128 */
#define DLEVEL_ERR		0x200
#define DLEVEL_INIFIN		0x400
#define DLEVEL_MEMORY		0x800
#define DLEVEL_COMM		0x1000
#define DLEVEL_WARN		0x2000
#define DLEVEL_ALL		0xffff

#define SHMEM_KEY_VAL_FMT	"/tmp/MPICH-shm"
#define PMIX_TOFU_SHMEM	"UTF_TOFU_SHM"
#define PMIX_USR_SHMEM	"UTF_USR_SHM"

#define TOFU_NIC_SIZE	6	/* number of NIC */
#define TOFU_INJECTSIZE	228
#define NODE_MAX	158976
#define PROC_MAX	663552	/* 24*23*24*12 * 4(ppn) */
#define SND_CNTRL_MAX	128	/* shorter than 2^7 (sidx) */
#define RCV_CNTRL_MAX	128	/* shorter than 2^7 (sidx) */
#define RCV_CNTRL_INIT	(RCV_CNTRL_MAX - 2) /* reserved for the chain mode */
#define COM_PEERS	128	/* sidx, must be smaller than 2^8 (edata) */
#define COM_RBUF_SIZE	1024	/* # of packet for receive buffer per process */
#define COM_RBUF_TOTSZ	(COM_RBUF_SIZE*COM_PEERS) /* 50 MiB: 512*1000*100 */
#define COM_SCNTR_MINF_SZ	4
#define COM_SBUF_SIZE	(COM_SCNTR_MINF_SZ*COM_PEERS)
#define IS_COMRBUF_FULL(p) ((p)->recvidx == COM_RBUF_SIZE)
#define COM_RMACQ_SIZE	128

//#define MSG_EAGERONLY	0
//#define MSG_RENDEZOUS	1
#define TRANSMODE_CHND	0
#define TRANSMODE_AGGR	1
#define TRANSMODE_THR	10	/* max is 255 (1B) */

/* request */
#define MSGREQ_SEND_SZ	256	/* MUST BE CHANGED to handle maximum */
#define MSGREQ_RECV_SZ	256	/* MUST BE CHANGED to handle maximum, COM_PEERS * MSGREQ_SEND_SZ */
#define MSGREQ_SIZE	(MSGREQ_SEND_SZ + MSGREQ_RECV_SZ)
#define MSGREQ_SENDMAX_INFLIGHT	200 /* pending rendezvous messages */
/* exp/uexp message list */
#define MSGLST_SIZE	512
/* network related */
#define TOFU_NTNI	6
#define TOFU_ALIGN	256
#define TOFU_RMA_MAXSZ	(8*1024*1024)	/* 8 MiB (8388608 B) */

#define MSG_MTU		1920	/* Tofu MTU */
//#define MSG_PKTSZ	(256*2)	/* must be cache align (256B) and shorter than MTU */
#define MSG_PKTSZ	(256)	/* must be cache align (256B) and shorter than MTU */
#define MSG_NTNI	3	/* see struct utf_vcqid_stadd in utf_queue.h */
#define MSG_MARKER	0x0 	/* marker is now zero */
//#define MSG_MARKER	0x9 	/* 4 bit */
//#define MSG_MARKER	(0x4B414E00L)	/* 4B */
#define MSG_RCNTRSZ	sizeof(struct utf_vcqid_stadd)
#define MSG_EAGER_SIZE	MSG_PYLDSZ	/* 236 B (256 - 20) */
#define UTOFU_PIGBACKSZ	32 /* See below */
#define MSG_EAGER_PIGBACK_SZ	(UTOFU_PIGBACKSZ - sizeof(struct utf_msghdr))	/* 12 B */
#define MSG_EGR		0
#define MSG_RENDEZOUS	1

//#define MSG_FI_PYLDSZ		(MSG_PYLDSZ - sizeof(uint64_t))	/* 228 B */ defined in utf_queue.h
#define MSG_FI_EAGER_PIGBACK_SZ	(MSG_EAGER_PIGBACK_SZ - sizeof(uint64_t))	/* 4 B */
#define MSG_FI_EAGER_SIZE	(MSG_EAGER_SIZE - sizeof(uint64_t))		/* 228 B */
#define MSG_FI_EAGER_INPLACE_SZ (1024 - sizeof(uint64_t))

/* stadd */
#define STAG_EGRMGT	10	/* utf_egrmgt */
#define STAG_RBUF	11	/* utf_egr_rbuf */
#define STAG_SBUF	12	/* utf_egr_sbuf */
#define STAG_SNDCTR	13	/* utf_scntr */
#define STAG_RCVCTR	14	/* utf_rcntr */
#define STAG_RNDV	15	/* utf_rndz_freelst */
#define STAG_RMACQ	16	/* utf_rmacq_pool */

/* 
 * MTU 1920 Byte, Piggyback size 32 Byte, Assume payload(1888 Byte)
 * 6.8 GB/s, 0.147 nsec/B
 * Rendezvous
 *	Time(B) = 2 + 0.147*B nsec, 
 *	e.g., T(1MiB) = 154.143 usec, 1MiB/154.143usec = 6.487 GiB/s
 *	    actual: 6.35 GiB/s/MB
 * Eager
 *	T(1888 B) = 1 + 0.147*1920 = 184.2 nsec, 1888B/184.2nsec = 10.2 MB/s
 */
/*
 *   158,976 node = 24 x 23 x 24 x 12
 *   635,904 process = 621 kiB, 20 bit (4 ppn)
 * 7,630,800 process = 7.277 Mi, 23 bit (48 ppn)
 */
/*
 *  Cache Line Size (https://www.7-cpu.com/)
 *	Skylake&KNL: 64 Byte
 *	A64FX: 512 Byte
 */
/*
 * Here is a result of utofu_query_onesided_caps()
 * flags:		 3
 * armw_ops:		 3f
 * num_cmp_ids:		 8
 * num_reserved_stags:	 256
 * cache_line_size:	 256 B
 * stag_address_alignment: 256 B
 * max_toq_desc_size:	 64 B
 * max_putget_size:	 15 MiB (16777215 B)
 * max_piggyback_size:	 32 B
 * max_edata_size:	 1 B
 * max_mtu:		 1920 B
 * max_gap:		 255 B
 */
