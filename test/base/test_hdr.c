#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <utofu.h>
#include "utf_conf.h"
#include "utf_list.h"
#include "utf_sndmgt.h"
#include "utf_debug.h"
#include "utf_queue.h"

#pragma pack(1)
struct test_msghdr { /* 20 Byte (4 + 8 + 8) */
    uint32_t	src;	  /* 32 bit is enoguth for Fugaku */
    uint64_t	tag;	  /* libfabric requirement */
    union {
	struct {
	    uint64_t size:35,  /* 35 bit, 32GiB is enough for Fugaku */
		pyldsz:13, /* size of payload: up to 8KiB */
		rndz: 1,
		flgs: 3,   /* utf(0) or libfabric(1) TAGGED(2) or MSG(0) */
		sidx: 8,
		marker: 4;
	};
	uint64_t hall;
    };
};
#pragma pack()

struct utf_packet	pkt;

int
bar(struct test_msghdr *pp)
{
    int		i = 0;
    if (pp->marker == MSG_MARKER) {
	printf("ARV\n");
    }
    if (pp->sidx == 100) {
	printf("SIDX\n");
    }
    return i;
}

int
foo(struct utf_packet *pp)
{
    int		i = 0;
    if (pp->hdr.marker == MSG_MARKER) {
	printf("ARV\n");
    }
    if (pp->hdr.flgs == 1) {
	printf("FI\n");
    }
    return i;
}

int
main()
{
    struct utf_packet	*pktp = &pkt;
    printf("pkt size(%ld)\n", sizeof(pkt));
    printf("pkt.hdr offset = %ld\n", (char*) &pkt.hdr - (char*) &pkt);
    printf("pkt.hdr.src offset = %ld\n", (char*) &pkt.hdr.src - (char*) &pkt);
    printf("pkt.hdr.tag offset = %ld\n", (char*) &pkt.hdr.tag - (char*) &pkt);
    printf("pkt.hdr.all offset = %ld\n", (char*) &pkt.hdr.hall - (char*) &pkt);
    if (pktp->hdr.marker == MSG_MARKER) {
	printf("ON\n");
    }
    return 0;
}
