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
#define INFO(mask) if (utf_iflag&(mask))
#define DLEVEL_PMIX		0x1
#define DLEVEL_UTOFU		0x2
#define DLEVEL_PROTOCOL		0x4
#define DLEVEL_PROTO_EAGER	0x8
#define DLEVEL_PROTO_RENDEZOUS	0x000010
#define DLEVEL_PROTO_RMA	0x000020	/* 32 */
#define DLEVEL_PROTO_VBG	0x000040	/* 64 */
#define DLEVEL_PROTO_AM		0x000080	/* 128 */
#define DLEVEL_ADHOC		0x000100	/* 128 */
#define DLEVEL_ERR		0x000200
#define DLEVEL_INIFIN		0x000400
#define DLEVEL_MEMORY		0x000800
#define DLEVEL_COMM		0x001000
#define DLEVEL_WARN		0x002000
#define DLEVEL_STATISTICS	0x004000
#define DLEVEL_FI		0x008000
#define DLEVEL_INI		0x010000
#define DLEVEL_CHND		0x020000
#define DLEVEL_ALL		0x0fffff
#define ILEVEL_MSG		0x01
#define ILEVEL_STAG		0x02

#define SHMEM_KEY_VAL_FMT	"/tmp/MPICH-shm"
#define PMIX_TOFU_SHMEM	"UTF_TOFU_SHM"
#define PMIX_USR_SHMEM	"UTF_USR_SHM"

#define TOFU_NIC_SIZE	6	/* number of NIC */
#define TOFU_INJECTSIZE	228
#define TOFU_INJECTCNT	16	/* asynchronous message injection size */
#define NODE_MAX	158976
#define PROC_MAX	663552	/* 24*23*24*12 * 4(ppn) */
#define SND_CNTRL_MAX	128	/* shorter than 2^7 (sidx) */
#define RCV_CNTRL_MAX	128	/* shorter than 2^7 (sidx) */
#define RCV_CNTRL_INIT	(RCV_CNTRL_MAX - 2) /* reserved for the chain mode */
#define EGRCHAIN_RECVPOS (RCV_CNTRL_MAX - 1)
#define COM_PEERS	128	/* sidx, must be smaller than 2^8 (edata) */
#define COM_RBUF_SIZE	1024	/* # of packet for receive buffer per process */
//#define COM_RBUF_SIZE	8*1024	/* # of packet for receive buffer per process */
#define COM_RBUF_TOTSZ	(COM_RBUF_SIZE*COM_PEERS) /* 50 MiB: 512*1000*100 */
#define COM_SCNTR_MINF_SZ	4
#define COM_SBUF_SIZE	(COM_SCNTR_MINF_SZ*COM_PEERS)
#define IS_COMRBUF_FULL(p) ((p)->recvidx == COM_RBUF_SIZE)
#define COM_RMACQ_SIZE	128
#define COM_EGR_PKTSZ	128	/* 256*128 (236*128) 29KiB payload 2020/12/20 */
//#define COM_EGR_PKTSZ	512	/* 256*512 (236*512) 118KiB payload 2020/12/20 */
//#define COM_EGR_PKTSZ	300	/* 256*300 (236*300) 69KiB payload 2020/12/20 */
//#define COM_EGR_PKTSZ	3	/* 256*3 (236*3) 708 B payload 2020/12/20 */

//#define MSG_EAGERONLY	0
//#define MSG_RENDEZOUS	1
#define TRANSMODE_CHND	0
#define TRANSMODE_AGGR	1
#define TRANSMODE_THR	10	/* max is 255 (1B) */

/* request */
#define DEBUG_20201231
#ifdef DEBUG_20201231
#define MSGREQ_SEND_SZ	1024 /* MUST BE CHANGED to handle maximum */
#define MSGREQ_RECV_SZ	1024 /* MUST BE CHANGED to handle maximum, fi_rx_attr.size specifies this value */
#else
#define MSGREQ_SEND_SZ	256	/* MUST BE CHANGED to handle maximum */
#define MSGREQ_RECV_SZ	256	/* MUST BE CHANGED to handle maximum, COM_PEERS * MSGREQ_SEND_SZ */
#endif
#define MSGREQ_SIZE	(MSGREQ_SEND_SZ + MSGREQ_RECV_SZ)
#define MSGREQ_SENDMAX_INFLIGHT	200 /* pending rendezvous messages */
/* exp/uexp message list */
#ifdef DEBUG_20201231
#define MSGLST_SIZE	(1024*2)
#else
#define MSGLST_SIZE	512
#endif
/* network related */
#define TOFU_NTNI	6
#define TOFU_ALIGN	256
#define TOFU_RMA_MAXSZ	(8*1024*1024)	/* 8 MiB (8388608 B) */

#define MSG_MTU		1920	/* Tofu MTU */
//#define MSG_PKTSZ	(256*8)	/* must be cache align (256B) and shorter than MTU */
//#define MSG_PKTSZ	(256*3)	/* must be cache align (256B) and shorter than MTU */
//#define MSG_PKTSZ	(256*2)	/* must be cache align (256B) and shorter than MTU */
#define MSG_PKTSZ	(256)	/* must be cache align (256B) and shorter than MTU */
#define MSG_NTNI	3	/* see struct utf_vcqid_stadd in utf_queue.h */
#define MSG_MARKER	0x0 	/* marker is now zero */
//#define MSG_MARKER	0x9 	/* 4 bit */
//#define MSG_MARKER	(0x4B414E00L)	/* 4B */
#define MSG_RCNTRSZ	sizeof(struct utf_vcqid_stadd)
#define MSG_EAGER_SIZE	MSG_PYLDSZ	/* 236 B (256 - 20) */
#define MSG_EGRCNTG_SZ	(MSG_PYLDSZ*COM_EGR_PKTSZ)	/* may send contiguous packets */
#define UTOFU_PIGBACKSZ	32 /* See below */
#define MSG_EAGER_PIGBACK_SZ	(UTOFU_PIGBACKSZ - sizeof(struct utf_msghdr))	/* 12 B */
#define MSG_EGR		0
#define MSG_RENDEZOUS	1

//#define MSG_FI_PYLDSZ		(MSG_PYLDSZ - sizeof(uint64_t))	/* 228 B */ defined in utf_queue.h
#define MSG_FI_EAGER_PIGBACK_SZ	(MSG_EAGER_PIGBACK_SZ - sizeof(uint64_t))	/* 4 B */
#define MSG_FI_EAGER_SIZE	(MSG_EAGER_SIZE - sizeof(uint64_t))		/* 228 B */
//#define MSG_FI_EAGER_INPLACE_SZ (1024 - sizeof(uint64_t))
#define MSG_FI_EAGER_INPLACE_SZ	(MSG_EGRCNTG_SZ - sizeof(uint64_t))	/* 30208 B 2020/12/20 */

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
