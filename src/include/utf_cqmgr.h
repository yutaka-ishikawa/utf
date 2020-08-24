#include <stdatomic.h>

/*
 * 128 B (8 + 3 + 6 + 6 + 9 + 8*6 * 2)
 */
#pragma pack(1)
struct tni_info {
    void		*vnamp;		/* struct tofu_vname */
    uint8_t		ppn;		/* process per node */
    uint8_t		nrnk;		/* rank within node */
    uint8_t		ntni;
    uint8_t		idx[TOFU_NIC_SIZE]; /* index of snd_len/rcv_len of cqsel_tab */
    uint8_t		usd[TOFU_NIC_SIZE];
    uint8_t		rsv[9];
    utofu_vcq_hdl_t	vcqhdl[TOFU_NIC_SIZE];
    utofu_vcq_id_t	vcqid[TOFU_NIC_SIZE];
};
#pragma pack()

#pragma pack(1)
struct cqsel_table { /* 6240 B */
    struct tni_info	node[48];	/* 6144 B = 128 * 48 */
    atomic_ulong	snd_len[TOFU_NIC_SIZE]; /* 48 B */
    atomic_ulong	rcv_len[TOFU_NIC_SIZE]; /* 48 B */
};
#pragma pack()
