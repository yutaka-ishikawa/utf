#include <stdio.h>

#define MPIDI_OFI_CONTEXT_BITS              (20)
#define MPIDI_OFI_SOURCE_BITS               (0)
#define MPIDI_OFI_TAG_BITS                  (31)

#define MPIDI_OFI_PROTOCOL_BITS (3)     /* This is set to 3 even though we actually use 4. The ssend
                                         * ack bit needs to live outside the protocol bit space to avoid
                                         * accidentally matching unintended messages. Because of this,
                                         * we shift the PROTOCOL_MASK one extra bit to the left to take
                                         * the place of the empty SSEND_ACK bit. */
#define MPIDI_OFI_SYNC_SEND_ACK      (1ULL << (MPIDI_OFI_CONTEXT_BITS + MPIDI_OFI_SOURCE_BITS + MPIDI_OFI_TAG_BITS))
#define MPIDI_OFI_SYNC_SEND          (2ULL << (MPIDI_OFI_CONTEXT_BITS + MPIDI_OFI_SOURCE_BITS + MPIDI_OFI_TAG_BITS))
#define MPIDI_OFI_DYNPROC_SEND       (4ULL << (MPIDI_OFI_CONTEXT_BITS + MPIDI_OFI_SOURCE_BITS + MPIDI_OFI_TAG_BITS))
#define MPIDI_OFI_HUGE_SEND          (8ULL << (MPIDI_OFI_CONTEXT_BITS + MPIDI_OFI_SOURCE_BITS + MPIDI_OFI_TAG_BITS))
#define MPIDI_OFI_PROTOCOL_MASK      (((1ULL << MPIDI_OFI_PROTOCOL_BITS) - 1) << 1 << (MPIDI_OFI_CONTEXT_BITS + MPIDI_OFI_SOURCE_BITS + MPIDI_OFI_TAG_BITS))
#define MPIDI_OFI_CONTEXT_MASK       (((1ULL << MPIDI_OFI_CONTEXT_BITS) - 1) << (MPIDI_OFI_SOURCE_BITS + MPIDI_OFI_TAG_BITS))
#define MPIDI_OFI_SOURCE_MASK        (((1ULL << MPIDI_OFI_SOURCE_BITS) - 1) << MPIDI_OFI_TAG_BITS)
#define MPIDI_OFI_TAG_MASK           ((1ULL << MPIDI_OFI_TAG_BITS) - 1)

int
main()
{
    unsigned long long	ll;

    ll = MPIDI_OFI_PROTOCOL_MASK;
    printf("MPIDI_OFI_PROTOCOL_MASK = 0x%016lx\n", ll);
    ll = MPIDI_OFI_CONTEXT_MASK;
    printf("MPIDI_OFI_CONTEXT_MASK  = 0x%016lx\n", ll);
    ll = MPIDI_OFI_SOURCE_MASK;
    printf("MPIDI_OFI_SOURCE_MASK   = 0x%016lx\n", ll);
    ll = MPIDI_OFI_TAG_MASK;
    printf("MPIDI_OFI_TAG_MASK      = 0x%016lx\n", ll);

    ll = MPIDI_OFI_SYNC_SEND_ACK;
    printf("MPIDI_OFI_SYNC_SEND_ACK = 0x%016lx\n", ll);
    ll = MPIDI_OFI_SYNC_SEND;
    printf("MPIDI_OFI_SYNC_SEND     = 0x%016lx\n", ll);
    ll = MPIDI_OFI_DYNPROC_SEND;
    printf("MPIDI_OFI_DYNPROC_SEND  = 0x%016lx\n", ll);
    ll = MPIDI_OFI_HUGE_SEND;
    printf("MPIDI_OFI_HUGE_SEND     = 0x%016lx\n", ll);
    return 0;
}
