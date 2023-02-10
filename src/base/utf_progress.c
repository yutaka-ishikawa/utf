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
struct utf_packet	_pkt_;

int
_compare_(unsigned char *org, unsigned char *new, unsigned char *val)
{
    int		max = sizeof(struct utf_packet);
    int		i;
    for (i = 0; i < max; i++) {
	if (*org++ != *new++) {
	    *val = *(org - 1);
	    return i + 1;
	}
    }
    return 0;
}

void
pkt_dump(struct utf_packet *pktp, int pos)
{
    int	i;
    char	buf[MSG_FI_PYLDSZ*6];
    char	*cp = buf;
    unsigned char	*pp = (unsigned char*) pktp;
    snprintf(buf, MSG_FI_PYLDSZ,
	     "\tsrc(%d) tag(%ld) size(%ld) pyldsz(%d) rndz(%d) flgs(%d) marker(%x) sidx(%d)"
	     "hall(0x%lx) fi_msg.data(%ld)\n",
	     pktp->hdr.src, pktp->hdr.tag, (int64_t) pktp->hdr.size, pktp->hdr.pyldsz,
	     pktp->hdr.rndz, pktp->hdr.flgs, pktp->hdr.marker, pktp->hdr.sidx,
	     pktp->hdr.hall,
	     pktp->pyld.fi_msg.data);
    cp += strlen(buf);
    for (i = 0; i < MSG_FI_PYLDSZ; i++) {
	if (i == pos) *cp++ = '*';
	snprintf(cp, 4, "%02x:", pp[i]);
	cp += 3;
	if (((i+1) % 16) == 0) {
	    *cp++ = '\n';
	}
    }
    utf_printf("%s\n", buf);
}

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
	    /**/
	    //_utf_fi_src = PKT_MSGSRC(pktp);
	    //_utf_fi_data = PKT_FI_DATA(pktp);
	    {
		extern int utf_dummy(struct utf_packet *);
		unsigned char val;
		int	bad = 1;
		do {
		    _pkt_ = *pktp;
		    utf_dummy(&_pkt_);
		    if (_compare_((unsigned char*) pktp, (unsigned char*) &_pkt_, &val)) {
			/* different */
			utf_reload++;
			continue;
		    } else {
			/* _utf_fi_src and _utf_fi_daa are not updated for testing */
			bad = 0;
		    }
		} while (bad);
	    }
	    /**/
	    if (utf_recvengine(urp, (struct utf_packet*) pktp, sidx) < 0) {
		utf_printf("%s: j(%d) protocol error urp(%p)->state(%d:%s) sidx(%d) pkt(%p) MSG(%s)\n",
			   __func__, j, urp, urp->state, rstate_symbol[urp->state],
			   sidx, pktp, pkt2string((struct utf_packet*) pktp, NULL, 0));
		abort();
	    }
#if 0
	    if (_utf_fi_data != PKT_FI_DATA(pktp)
		|| _utf_fi_src != PKT_MSGSRC(pktp)) {
		utf_printf("%s: TOFU NOTICE prev-src(%d)data(%ld) now-src(%d)data(%ld) pktp(%p) recvidx(%d)\n", __func__, _utf_fi_src, _utf_fi_data, PKT_MSGSRC(pktp), PKT_FI_DATA(pktp), pktp, urp->recvidx);
		pkt_dump((struct utf_packet*) pktp, -1);
		pkt_dump(&_pkt_, -1);
	    } else
#endif /* 0 */
	    {
		int	rc;
		unsigned char	val;
		if ((rc = _compare_((unsigned char*) pktp, (unsigned char*) &_pkt_, &val))) {
		    utf_printf("%s: TOFU NOTICE !!! pos(%d) val(%02x)\n", __func__, rc - 1, val);
		    pkt_dump((struct utf_packet*) pktp, rc - 1);
		    pkt_dump(&_pkt_, rc - 1);
		}
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
