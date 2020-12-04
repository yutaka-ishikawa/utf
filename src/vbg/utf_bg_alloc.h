/*
 * Copyright (c) 2020 XXXXXXXXXXXXXXXXXXXXXXXX.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

/*
 * @file   utf_bg_alloc.h
 * @briaf  Macro and structure definitions used in utf_bg_alloc()
 */


/*
 * Include of header files
 */
#include "utf_bg_internal.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <jtofu.h>

/*
 * Macro definitions
 */

/*
 * プロセス毎の軸種別
 */
#define UTF_BG_ALLOC_AXIS_OFF_AXES (-1)
#define UTF_BG_ALLOC_AXIS_IN_NODE    0
#define UTF_BG_ALLOC_AXIS_ON_X       1
#define UTF_BG_ALLOC_AXIS_ON_Y       2
#define UTF_BG_ALLOC_AXIS_ON_Z       3
#define UTF_BG_ALLOC_AXIS_NUM_AXES   4
#define UTF_BG_ALLOC_AXIS_MYSELF     5

/* TNI番号取り出し用マスク値 */
#define UTF_BG_ALLOC_MSK_TNI         0xE000000000000000
/* VBGからbgid取り出し用マスク値 */
#define UTF_BG_ALLOC_MSK_BGID        0x000000000000003F
/* bginfo初期値 */
#define UTF_BG_ALLOC_INIT_BGINFO     0x000000000000FFFF
/* ノードID作成 */
#define UTF_BG_ALLOC_MSK_6DADDR      0x0FFFFFFF00000000
/* 6次元アドレス生成用マスク値 */
#define UTF_BG_ALLOC_MSK_6DADDR_X            0x000000FF
#define UTF_BG_ALLOC_MSK_6DADDR_Y            0x0000FF00
#define UTF_BG_ALLOC_MSK_6DADDR_Z            0x00FF0000
#define UTF_BG_ALLOC_MSK_6DADDR_A            0x01000000
#define UTF_BG_ALLOC_MSK_6DADDR_B            0x06000000
#define UTF_BG_ALLOC_MSK_6DADDR_C            0x08000000
#define UTF_BG_ALLOC_BITSFT_COORD_X                   0
#define UTF_BG_ALLOC_BITSFT_COORD_Y                   8
#define UTF_BG_ALLOC_BITSFT_COORD_Z                  16
#define UTF_BG_ALLOC_BITSFT_COORD_A                  24
#define UTF_BG_ALLOC_BITSFT_COORD_B                  25
#define UTF_BG_ALLOC_BITSFT_COORD_C                  27

/* ノード内ソフトバリア使用時に必要なノード数 */
#define UTF_BG_ALLOC_REQUIRED_NODE           4
/* ノード内プロセス最大数 */
#define UTF_BG_ALLOC_MAX_PROC_IN_NODE        48
/* ノード内ハードバリア使用時のノード内プロセス数 */
#define UTF_BG_ALLOC_MAX_PROC_IN_NODE_FOR_HB UTF_BG_ALLOC_MAX_PROC_IN_NODE
/* 無効なインデックス値 */
#define UTF_BG_ALLOC_INVALID_INDEX           ((size_t)-1)
/* 無効なノードID */
#define UTF_BG_ALLOC_INVALID_NODEID          (-1)

/* バタフライネットワーク内の自プロセスの位置 */
#define UTF_BG_ALLOC_NODE_ZONES            3
#define UTF_BG_ALLOC_POWEROFTWO_EAR        0 /* 耳 */
#define UTF_BG_ALLOC_POWEROFTWO_CHEEK      1 /* 頬 */
#define UTF_BG_ALLOC_POWEROFTWO_FACE       2 /* 顔 */

/* ノード内プロセス数、ノード数が正しくない場合の内部エラーコード */
#define UTF_BG_ALLOC_TOO_MANY_PROC  ((uint8_t)0x01)
#define UTF_BG_ALLOC_UNMATCH_PROC   ((uint8_t)0x02)
#define UTF_BG_ALLOC_TOO_FEW_NODE   ((uint8_t)0x04)

/* VBG情報 */
#define UTF_BG_ALLOC_VBG_ID_UNNECESSARY ((utofu_vbg_id_t)-3) /* 不必要VBGID */
#define UTF_BG_ALLOC_VBG_ID_UNDECIDED   ((utofu_vbg_id_t)-2) /* 未確定VBGID */
#define UTF_BG_ALLOC_VBG_ID_NOOPARATION UTOFU_VBG_ID_NULL

/* utf_bg_alloc.cでのみの内部エラー */
#define UTF_BG_ALLOC_ERR_UNMATCH_PROC_PER_NODE   -2048
#define UTF_BG_ALLOC_ERR_TOO_MANY_PROC_IN_NODE   -2049
#define UTF_BG_ALLOC_ERR_TOO_FEW_NODE            -2050

/* utf_bg_internal.hに移すべきマクロ */

/*
 * Structure definitions
 */

/* バタフライネットワーク通信ノード情報 */
typedef struct {
    utofu_vbg_id_t vbg_id;                                      /* VBGID */
    size_t  nodeinfo_index;
                                /* nodeinfo内の自プロセスのインデックス  */
    size_t index;                        /* rankset,bginfoのインデックス */
    union jtofu_phys_coords *phy_6d_addr; /* 物理6次元アドレス  */
} utf_bf_alloc_butterfly_proc_info_t;

/* バタフライネットワーク情報 */
struct utf_bf_alloc_butterfly_sequence_info_t {
    utofu_vbg_id_t vbg_id;                           /* VBGID */
    utf_bf_alloc_butterfly_proc_info_t     recvinfo; /* 受信元情報 */
    utf_bf_alloc_butterfly_proc_info_t     sendinfo; /* 送信先情報 */
    struct utf_bf_alloc_butterfly_sequence_info_t *nextptr;
};
typedef struct utf_bf_alloc_butterfly_sequence_info_t
        utf_bf_alloc_butterfly_sequence_info_t;

/* each of nodes information which is belong to axes */
typedef struct {
    size_t   index;                     /* rankset,bginfoのインデックス */
    union jtofu_log_coords  log_3d_addr; /* 論理3次元アドレス  */
    union jtofu_phys_coords phy_6d_addr; /* 物理6次元アドレス  */
} utf_bg_alloc_axis_node_info_t;

/* each of axes information */
typedef struct {
    uint32_t biggest;        /* 論理アドレスの最大値 */
    uint32_t smallest;       /* 論理アドレスの最小値 */
    size_t size;             /* 軸の長さ */
    size_t number_gates_of_poweroftwo; /* 2冪レベルでの必要なゲート数*/
    bool flag_poweroftwo;    /* ノード数が2冪か？ */
    size_t number_gates;     /* バタフライネットワークを組むのに必要なゲート数*/
    size_t number_relay_gates[UTF_BG_ALLOC_NODE_ZONES];
                             /* ゾーン別に見た中継ゲートの数 */
    size_t on_poweroftwo;    /* バタフライネットワークにおける2冪開始位置 */
    size_t off_poweroftwo;   /* バタフライネットワークにおける2冪から外れた位置*/
    size_t nodeinfo_index;   /* nodeinfo内の自プロセスのインデックス */
    utf_bg_alloc_axis_node_info_t *nodeinfo;
                   /* each of nodes information which is belong to this axis */

} utf_bg_alloc_axis_detail_info_t;

/* each of nodes information */
typedef struct {
    /* ノードID 物理6次元アドレスを基に作成 */
    uint32_t nodeid;
    int8_t   size;           /* ノード内プロセス数 */
    int8_t   axis_kind;      /* 自プロセスのノードから見た軸種別 */
    size_t   manager_index;  /* ノード間代表プロセスのインデックス */
} utf_bf_alloc_node_info_t;

/* each of processes information */
typedef struct {
    uint32_t nodeid;
    int8_t axis_kind;       /* 自プロセスのノードから見た軸種別 */
    size_t index;           /* 通信相手プロセスのインデックス */
} utf_bf_alloc_proc_info_t;

