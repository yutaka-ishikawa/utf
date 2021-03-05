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

//#define UTF_POLLING_WAIT
#ifdef UTF_POLLING_WAIT
int utf_progcount;
#define UTF_POLLING_COUNT 1000
#endif

/* Checking memory */
uint64_t	_utf_fi_src, _utf_fi_data;

int
utf_progress()
{
    int	j;
    int nrcv = 0;

    /* at least one shot progress */
    do {
	utf_tcqprogress();
    } while (utf_tcq_count > 0);

    if ((int64_t) utf_egr_rbuf.head.cntr < -1) {
	utf_printf("%s: No more Eager Receiver Buffer in my rank. cntr(%ld)\n", __func__, (int64_t) utf_egr_rbuf.head.cntr);
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
    for (j = utf_egr_rbuf.head.cntr + 1; j <= EGRCHAIN_RECVPOS; j++) {
	struct utf_packet	*msgbase = utf_recvbuf_get(j);
	struct utf_recv_cntr	*urp = &utf_rcntr[j];
	volatile struct utf_packet	*pktp;
	int	sidx;
    try_again:
	pktp = msgbase + urp->recvidx;
	if (pktp->hdr.marker == MSG_MARKER) {
	    /* message arrives */
	    sidx = pktp->hdr.sidx;
	    if ((sidx & 0xff) == 0xff) {
		utf_printf("%s: j(%d) PROTOCOL ERROR urp(%p)->state(%d:%s) sidx(%d) pkt(%p) MSG(%s)\n",
			   __func__, j, urp, urp->state, rstate_symbol[urp->state],
			   sidx, pktp, pkt2string((struct utf_packet*) pktp, NULL, 0));
		utf_printf("%s: sidx(%d)\n", __func__, pktp->hdr.sidx);
	    }
	    urp->src = pktp->hdr.src;
	    _utf_fi_src = PKT_MSGSRC(pktp);
	    _utf_fi_data = PKT_FI_DATA(pktp);
	    if (utf_recvengine(urp, (struct utf_packet*) pktp, sidx) < 0) {
		utf_printf("%s: j(%d) protocol error urp(%p)->state(%d:%s) sidx(%d) pkt(%p) MSG(%s)\n",
			   __func__, j, urp, urp->state, rstate_symbol[urp->state],
			   sidx, pktp, pkt2string((struct utf_packet*) pktp, NULL, 0));
		abort();
	    }
	    if (_utf_fi_data != PKT_FI_DATA(pktp)) {
		utf_printf("%s: TOFU NOTICE prev-src(%d)data(%ld) now-src(%d)data(%ld)\n", __func__, _utf_fi_src, _utf_fi_data, PKT_MSGSRC(pktp), PKT_FI_DATA(pktp));
	    }
	    pktp->hdr.hall = -1UL;
	    urp->recvidx++;
	    if (IS_COMRBUF_FULL(urp)) {
		utofu_stadd_t	stadd = SCNTR_ADDR_CNTR_FIELD(sidx);
		struct utf_send_cntr *newusp;
		urp->recvidx = 0;
		if (j == EGRCHAIN_RECVPOS || urp->svcqid == 0) {
		    urp->svcqid = utf_info.vname[pktp->hdr.src].vcqid;
		    urp->src = pktp->hdr.src;
		}
		newusp = utf_remote_armw4(utf_info.vcqh, urp->svcqid, urp->flags,
					  UTOFU_ARMW_OP_OR, SCNTR_OK,
					  stadd + SCNTR_RST_RECVRESET_OFFST, sidx, 0);
		if (newusp != NULL) { /* 2021/02/06 */
		    utf_printf("%s: TOFU internal error\n", __func__);
		    abort();
		}
	    } else {
		nrcv++;
		if (nrcv < utf_rcv_count) {
		    goto try_again;
		}
	    }
	}
    }
    utf_rcv_max = utf_rcv_count > utf_rcv_max ? utf_rcv_count : utf_rcv_max;
    {
	int	uc;
	do {
	    uc = utf_mrqprogress();
	} while (uc == UTOFU_SUCCESS);
    }
    //if (!arvd) utf_mrqprogress();

    return 0;
}
