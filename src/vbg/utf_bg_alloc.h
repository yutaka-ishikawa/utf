/*
 * Copyright (C) 2020 RIKEN, Japan. All rights reserved.
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
 * Includes of header files
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
 * The axis that the process is on.
 */
#define UTF_BG_ALLOC_AXIS_OFF_AXES (-1)
#define UTF_BG_ALLOC_AXIS_IN_NODE    0
#define UTF_BG_ALLOC_AXIS_ON_X       1
#define UTF_BG_ALLOC_AXIS_ON_Y       2
#define UTF_BG_ALLOC_AXIS_ON_Z       3
#define UTF_BG_ALLOC_AXIS_NUM_AXES   4
#define UTF_BG_ALLOC_AXIS_MYSELF     5

/* Bit mask for TNI */
#define UTF_BG_ALLOC_MSK_TNI         0xE000000000000000
/* Bit mask for BG ID */
#define UTF_BG_ALLOC_MSK_BGID        0x000000000000003F
/* Initial value of bginfo */
#define UTF_BG_ALLOC_INIT_BGINFO     0x000000000000FFFF
/* To get node ID */
#define UTF_BG_ALLOC_MSK_6DADDR      0x0FFFFFFF00000000
/* Bit mask for 6D coordinate */
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

/* The minimum number of nodes using utf barrier communication */
#define UTF_BG_ALLOC_REQUIRED_NODE           4
/* Using Tofu barrier, the maximum number of processes in the node */
#define UTF_BG_ALLOC_MAX_PROC_IN_NODE_FOR_HB UTF_BG_MAX_PROC_IN_NODE
/* Invalid index */
#define UTF_BG_ALLOC_INVALID_INDEX           ((size_t)-1)
/* Invalid node ID */
#define UTF_BG_ALLOC_INVALID_NODEID          (-1)

/* Location of own process in a butterfly network */
#define UTF_BG_ALLOC_NODE_ZONES            3
#define UTF_BG_ALLOC_POWEROFTWO_EAR        0                   /* Ears   */
#define UTF_BG_ALLOC_POWEROFTWO_CHEEK      1                   /* Cheeks */
#define UTF_BG_ALLOC_POWEROFTWO_FACE       2                   /* Face   */

/* Internal error code when the number of processes in the node or
 * the number of nodes is incorrect */
#define UTF_BG_ALLOC_TOO_MANY_PROC  ((uint8_t)0x01)
#define UTF_BG_ALLOC_UNMATCH_PROC   ((uint8_t)0x02)
#define UTF_BG_ALLOC_TOO_FEW_NODE   ((uint8_t)0x04)

/* Information about VBG */
#define UTF_BG_ALLOC_VBG_ID_UNNECESSARY ((utofu_vbg_id_t)-3)
                                                      /* Unnecessary VBG */
#define UTF_BG_ALLOC_VBG_ID_UNDECIDED   ((utofu_vbg_id_t)-2)
                                                      /* Undecided   VBG */
#define UTF_BG_ALLOC_VBG_ID_NOOPARATION UTOFU_VBG_ID_NULL

/* Internal error used just in utf_bg_alloc.c */
#define UTF_BG_ALLOC_ERR_UNMATCH_PROC_PER_NODE   -2048
#define UTF_BG_ALLOC_ERR_TOO_MANY_PROC_IN_NODE   -2049
#define UTF_BG_ALLOC_ERR_TOO_FEW_NODE            -2050

/*
 * Structure definitions
 */

/* Node information about butterfly network communication */
typedef struct {
    utofu_vbg_id_t vbg_id;                                     /* VBG ID */
    size_t  nodeinfo_index;  /* Index of own process in node information */
    size_t index;                         /* Index of rankset and bginfo */
    union jtofu_phys_coords *phy_6d_addr;     /* Physical 6D coordinates */
} utf_bf_alloc_butterfly_proc_info_t;

/* Information about butterfly network communication */
struct utf_bf_alloc_butterfly_sequence_info_t {
    utofu_vbg_id_t vbg_id;                                     /* VBG ID */
    utf_bf_alloc_butterfly_proc_info_t     recvinfo;           /* Source */
    utf_bf_alloc_butterfly_proc_info_t     sendinfo;      /* Destination */
    struct utf_bf_alloc_butterfly_sequence_info_t *nextptr;
};
typedef struct utf_bf_alloc_butterfly_sequence_info_t
        utf_bf_alloc_butterfly_sequence_info_t;

/* Node information belonging to each axis */
typedef struct {
    size_t   index;                       /* Index of rankset and bginfo */
    union jtofu_log_coords  log_3d_addr;       /* Logical 3D coordinates */
    union jtofu_phys_coords phy_6d_addr;      /* Physical 6D coordinates */
} utf_bg_alloc_axis_node_info_t;

/* Axis information */
typedef struct {
    uint32_t biggest;            /* Maximum value of Logical coordinates */
    uint32_t smallest;           /* Minimum value of Logical coordinates */
    size_t size;                                         /* Axial length */
    size_t number_gates_of_poweroftwo;
                     /* Number of gates used in conversion to power of 2 */
    bool flag_poweroftwo;
                        /* Whether the number of nodes is a power of 2? */
    size_t number_gates;
               /* Number of gates needed to complete a butterfly network */
    size_t number_relay_gates[UTF_BG_ALLOC_NODE_ZONES];
           /* Number of relay gates for each zone in a butterfly network */
    size_t on_poweroftwo; /* Starting position of a true power of 2      *
                                            range in a butterfly network */
    size_t off_poweroftwo; /* End position of a true power of 2 range    *
                                                  in a butterfly network */
    size_t nodeinfo_index;   /* Index of own process in node information */
    utf_bg_alloc_axis_node_info_t *nodeinfo;
                              /* Node information belonging to each axis */

} utf_bg_alloc_axis_detail_info_t;

/* Node information */
typedef struct {
    
    uint32_t nodeid;     /* Node ID derived from physical 6D coordinates */
    int8_t   size;                    /* Number of processes in the node */
    int8_t   axis_kind; /* The axis on which the target node is based on *
                                               the position of own node. */
    size_t   manager_index;   /* Index of the leader process in the node */
} utf_bf_alloc_node_info_t;

/* Process information */
typedef struct {
    uint32_t nodeid;
    int8_t axis_kind;   /* The axis on which the target node is based on */
    size_t index;               /* Index of communication peer processes */
} utf_bf_alloc_proc_info_t;

