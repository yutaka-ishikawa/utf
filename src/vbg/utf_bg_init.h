/*
 * Copyright (c) 2020 XXXXXXXXXXXXXXXXXXXXXXXX.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 */

/**
 * @file   utf_bg_init.h
 * @brief  Macro and structure definitions used in utf_bg_init()
 */

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

/*
 *            << structure of bginfo(utf_bg_info_t) >>
 * TNI ID(3)                                           dest BG ID(8)
 *  | Tofu barrier unavailable bit(1)            src BG ID(8)  |
 *  |  |                                               |       |
 * +--++---------------------------+---------------+-------+-------+
 * |  || compute node coords(28)   |  not used(16) |       |       |
 * +---------------+---------------+---------------+---------------+
 * MSB            48              32              16             LSB
 */

/**
 * Macro definitions
 */
/* Tofu barrier network unavailable bit check */
#define UTF_BIT_BGINFO_DISABLE ((utf_bg_info_t)1 << UTF_BG_BITSFT_BGINFO_DISABLE)
/* Get TNI ID and the compute node coordinates */
#define UTF_GET_BGINFO_TNI_CNC(x)  (utofu_vbg_id_t)((x) & 0xEFFFFFFF00000000)
/* Get the source BG ID */
#define UTF_GET_BGINFO_DSTBGID(x)  (utofu_vbg_id_t)((x) & 0xFF)
/* Get the destination BG ID */
#define UTF_GET_BGINFO_SRCBGID(x)  (utofu_vbg_id_t)((x) & 0xFF00) >> 8

#if defined(UTF_INTERNAL_DEBUG)

/* For internal debug */
#define VBG_BG                 0x000000000000003F
#define VBG_COORDS_X           0x000000FF00000000
#define VBG_COORDS_Y           0x0000FF0000000000
#define VBG_COORDS_Z           0x00FF000000000000
#define VBG_COORDS_A           0x0100000000000000
#define VBG_COORDS_B           0x0600000000000000
#define VBG_COORDS_C           0x0800000000000000
#define VBG_TNI                0xE000000000000000

#define BITSFT_VBG_COORDS_X                    32
#define BITSFT_VBG_COORDS_Y                    40
#define BITSFT_VBG_COORDS_Z                    48
#define BITSFT_VBG_COORDS_A                    56
#define BITSFT_VBG_COORDS_B                    57
#define BITSFT_VBG_COORDS_C                    59
#define BITSFT_VBG_TNI                         61

/* For internal debug: Get the BG ID from the VBG ID */
#define UTF_BG_VBG_BGID(v) (uint16_t)((v) & VBG_BG)
/* For internal debug: Get TNI ID from the VBG ID */
#define UTF_BG_VBG_TNI(v)  (uint16_t)(((v) & VBG_TNI) >> BITSFT_VBG_TNI)
/* For internal debug: Get the compute node coordinates from the VBG ID */
#define UTF_BG_VBG_COORDS(v,co)                                      \
{                                                                    \
    (co).x = (uint8_t)(((v) & VBG_COORDS_X) >> BITSFT_VBG_COORDS_X); \
    (co).y = (uint8_t)(((v) & VBG_COORDS_Y) >> BITSFT_VBG_COORDS_Y); \
    (co).z = (uint8_t)(((v) & VBG_COORDS_Z) >> BITSFT_VBG_COORDS_Z); \
    (co).a = (uint8_t)(((v) & VBG_COORDS_A) >> BITSFT_VBG_COORDS_A); \
    (co).b = (uint8_t)(((v) & VBG_COORDS_B) >> BITSFT_VBG_COORDS_B); \
    (co).c = (uint8_t)(((v) & VBG_COORDS_C) >> BITSFT_VBG_COORDS_C); \
}
/* For internal debug: The BG node identification information */
#define I_START        ":S"
#define I_END          ":E"
#define I_RELAY        ""
/* For internal debug: Attributes of arrows(DOT) */
/* SL: signal source,      SR: packet source, 
   DL: signal destination, DR: packet destination */
#define ATTR_E_SL      "color=skyblue"
#define ATTR_E_SR      "color=blue"
#define ATTR_E_DL      "color=tomato"
#define ATTR_E_DR      "color=red"
/* For internal debug: Draw the BG node(DOT) */
/*   "<BG node info>" [pos=<node drawing position>, <other attributes>];\n */
/* <BG node info> = <coords>\n<TNI ID>-<BG ID>:{[S|E] }[<rank>]            */
/* <coords>    = <X>-<Y>-<Z>-<A>-<B>-<C>                                   */
#define UTF_BG_DRAW_NODE(OFILE, SRC, SRC_INF, COLUMN, ROW, ATTR)           \
    fprintf((OFILE),                                                       \
            "  \"%02d-%02d-%02d-%d-%d-%d\\n%d-%d%s[%ld]"                   \
            "\" [pos=\"%ld,%ld!\", %s];\n",                                \
            (SRC).coords.x, (SRC).coords.y, (SRC).coords.z,                \
            (SRC).coords.a, (SRC).coords.b, (SRC).coords.c,                \
            (SRC).tni, (SRC).bgid, (SRC_INF), (SRC).rank,                  \
            (COLUMN), (ROW), (ATTR));                                      \
    fflush((OFILE));
/* For internal debug: Draw the arrow(DOT) */
/*   "<BG node info>" -> "<BG node info>" [<attributes>];\n */
#define UTF_BG_DRAW_EDGE(OFILE, SRC, SRC_INF, DST, DST_INF, ATTR)          \
    fprintf((OFILE),                                                       \
            "  \"%02d-%02d-%02d-%d-%d-%d\\n%d-%d%s[%ld]\" -> "             \
            "\"%02d-%02d-%02d-%d-%d-%d\\n%d-%d%s[%ld]\" [%s];\n",          \
            (SRC).coords.x, (SRC).coords.y, (SRC).coords.z,                \
            (SRC).coords.a, (SRC).coords.b, (SRC).coords.c,                \
            (SRC).tni, (SRC).bgid, (SRC_INF), (SRC).rank,                  \
            (DST).coords.x, (DST).coords.y, (DST).coords.z,                \
            (DST).coords.a, (DST).coords.b, (DST).coords.c,                \
            (DST).tni, (DST).bgid, (DST_INF), (DST).rank,                  \
            (ATTR));                                                       \
    fflush((OFILE));
#endif /* UTF_INTERNAL_DEBUG */


/**
 * Structure definitions
 */
#if defined(UTF_INTERNAL_DEBUG)
/* For internal debug: the compute node coordinates of the VBG ID */
typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t z;
    uint8_t a;
    uint8_t b;
    uint8_t c;
} utf_bg_vbg_coords;

/* For internal debug: VBG infomation for drawing */
typedef struct {
    size_t   rank;             /* rank */
    uint16_t bgid;             /* BG ID */
    uint16_t tni;              /* TNI ID */
    utf_bg_vbg_coords coords;  /* compute node coordinates */
} utf_bg_vbg_info_t;
#endif /* UTF_INTERNAL_DEBUG */


/**
 * Internal interface declaration
 */
#if defined(UTF_INTERNAL_DEBUG)
static void draw_barrier_network(struct utofu_vbg_setting *vbg_val,
                                 uint32_t rankset[],
                                 size_t my_index);
#endif /* UTF_INTERNAL_DEBUG */

