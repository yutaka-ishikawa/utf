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

#define UTF_POLLING_WAIT

#ifdef UTF_POLLING_WAIT
int utf_progcount;
#define UTF_POLLING_COUNT 1000
#endif

int
utf_progress()
{
    int	j;
    int	arvd = 0;

    if (utf_tcq_count) utf_tcqprogress();

    if ((int64_t) utf_egr_rbuf.head.cntr < 0) {
	utf_printf("%s: No more Eager Receiver Buffer in my rank\n", __func__);
	utf_egrrbuf_show(stderr);
	abort();
    }
#ifdef UTF_POLLING_WAIT
    if (utf_progcount++ > UTF_POLLING_COUNT) {
	getpid();
	utf_progcount = 0;
    }
#endif /* UTF_POLLING_WAIT */
    /* here is peeking memory */
    for (j = utf_egr_rbuf.head.cntr + 1; j <= RCV_CNTRL_INIT/*RCV_CNTRL_MAX*/ ; j++) {
	struct utf_packet	*msgbase = utf_recvbuf_get(j);
	struct utf_recv_cntr	*urp = &utf_rcntr[j];
	volatile struct utf_packet	*pktp;
	int	sidx;
    try_again:
	pktp = msgbase + urp->recvidx;
	if (pktp->hdr.marker == MSG_MARKER) {
	    /* message arrives */
	    //utf_printf("%s: msgsize(%d) payloadsize(%d)\n", __func__, pktp->hdr.size, PKT_PYLDSZ(pktp));
	    //utf_tmr_end(TMR_UTF_RCVPROGRESS);
	    sidx = pktp->hdr.sidx;
	    if ((sidx & 0xff) == 0xff) {
		utf_printf("%s: j(%d) PROTOCOL ERROR urp(%p)->state(%d:%s) sidx(%d) pkt(%p) MSG(%s)\n",
			   __func__, j, urp, urp->state, rstate_symbol[urp->state],
			   sidx, pktp, pkt2string((struct utf_packet*) pktp, NULL, 0));
		utf_printf("%s: sidx(%d)\n", __func__, pktp->hdr.sidx);
	    }
	    /* for debugging */
	    //urp->dbg_rsize[urp->dbg_idx] = PKT_PYLDSZ(pktp);
	    //urp->dbg_idx = (urp->dbg_idx + 1) % COM_RBUF_SIZE;
	    if (utf_recvengine(urp, (struct utf_packet*) pktp, sidx) < 0) {
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
	// utf_printf("%s: waiting for recvidx(%d)\n", __func__, urp->recvidx);
    }
    if (!arvd) utf_mrqprogress();

    return 0;
}
