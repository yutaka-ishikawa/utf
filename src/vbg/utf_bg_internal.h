/*
 * Copyright (C) 2020 RIKEN, Japan. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

/*
 * Includes header files
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/mman.h>
#include <assert.h>
#if (!defined(TOFUGIJI))
#include <stdatomic.h>
#endif
#include "utf_conf.h"
#include "utf_tofu.h"
#include "utf_bg.h"

/*
 * Macro definitions
 */
#define UTF_BG_BGINFO_INVALID_BGID      ((utofu_bg_id_t)(0x00ff))
#define UTF_BG_BGINFO_INVALID_INDEX     ((size_t)-1)

#define UTF_BG_BITSFT_BGINFO_TNI             61
#define UTF_BG_BITSFT_BGINFO_DISABLE         60
#define UTF_BG_BITSFT_BGINFO_COORD_C         59
#define UTF_BG_BITSFT_BGINFO_COORD_B         57
#define UTF_BG_BITSFT_BGINFO_COORD_A         56
#define UTF_BG_BITSFT_BGINFO_COORD_Z         48
#define UTF_BG_BITSFT_BGINFO_COORD_Y         40
#define UTF_BG_BITSFT_BGINFO_COORD_X         32
#define UTF_BG_BITSFT_BGINFO_ERRCASE         16
#define UTF_BG_BITSFT_BGINFO_SRCBGID          8

#define UTF_BG_INTRA_NODE_MANAGER           (0UL)
#define UTF_BG_INTRA_NODE_WORKER            ((size_t)-1)

#define UTF_BG_CACHE_LINE_SIZE              256
#define UTF_BG_MMAP_FILE_NAME_LEN           128
#define UTF_BG_MMAP_NUM_SEQ                   1
#define UTF_BG_MMAP_NUM_BUF                   6

#define UTF_BG_MIN_TNI_NUMBER                 0
#define UTF_BG_MAX_TNI_NUMBER                 5
#define UTF_BG_MIN_START_BG_NUMBER            0
#define UTF_BG_MAX_START_BG_NUMBER           15

/* The maximum number of processes in the node */
#define UTF_BG_MAX_PROC_IN_NODE              48

#define UTF_BG_NUM_TNI       (UTF_BG_MAX_TNI_NUMBER - UTF_BG_MIN_TNI_NUMBER + 1)
#define UTF_BG_NUM_START_BG  (UTF_BG_MAX_START_BG_NUMBER - UTF_BG_MIN_START_BG_NUMBER + 1)

/* mmap header buffer size */
#define UTF_BG_MMAP_HEADER_SIZE                                                \
    ((sizeof(utf_bg_mmap_header_info_t) + UTF_BG_CACHE_LINE_SIZE - 1) /        \
     UTF_BG_CACHE_LINE_SIZE * UTF_BG_CACHE_LINE_SIZE)

/* mmap information buffer size for each process */
#define UTF_BG_MMAP_PROC_INF_SIZE                                              \
    ((sizeof(uint64_t) * (UTF_BG_MMAP_NUM_SEQ + UTF_BG_MMAP_NUM_BUF) +         \
           UTF_BG_CACHE_LINE_SIZE - 1) /                                       \
     UTF_BG_CACHE_LINE_SIZE * UTF_BG_CACHE_LINE_SIZE)

/* mmap information buffer size for each gate */
#define UTF_BG_MMAP_GATE_INF_SIZE                                              \
    (UTF_BG_MMAP_PROC_INF_SIZE * utf_bg_mmap_file_info.width)

/* mmap information buffer size of each TNI */
#define UTF_BG_MMAP_TNI_INF_SIZE                                               \
    (UTF_BG_MMAP_GATE_INF_SIZE * UTF_BG_NUM_START_BG)

/*
 * Structure definitions
 */

/*
 * The arguments for utf_bg_alloc function
 */
typedef struct {
    /* Pointer to an array of ranks numbered within MPI_COMM_WORLD
     * for all processes in this barrier network (=communicator)   */
    uint32_t       *rankset;
    /* The total number of processes in this barrier network       */
    size_t         len;
    /* Rank number of own process in this barrier network          */
    size_t         my_index;
} utf_bg_alloc_argument_info_t;

/*
 * Information about processes in the own node
 */
typedef struct {
    /* Whether own process is the leader in the node or not. */
    size_t   state;
    /* Number of processes in the node */
    size_t   size;
    /* Index of own process in the node */
    size_t   intra_index;
    /* list of vpids in the node */
    size_t *vpids;
    /* Number of processing sequence */
    uint64_t curr_seq;
    /* Pointer to memory shared by own node */
    volatile uint64_t *mmap_buf;
} utf_bg_intra_node_detail_info_t;

/*
 * Manager's tni/bg info.
 */
typedef struct {
    /* TNI to which BGs acquired by the leader process in the node belong */
    volatile utofu_tni_id_t tni;
    /* Start BG acquired by the leader process in the node */
    volatile utofu_bg_id_t  start_bg;
} utf_bg_manager_tnibg_info;

/*
 * Information in the first portion of a mmap buffer
 */
typedef struct {
    /*
     * Control area for deleting a shared memory segment (atomic_ulong)
     */
#if (!defined(TOFUGIJI))
    atomic_ulong  ul;
#else
    unsigned long ul;
#endif
    /* TNI to start using when the next barrier network is created */
    utofu_tni_id_t next_tni_id;
    /* Manager's TNI/BG infos */
    utf_bg_manager_tnibg_info manager_info[UTF_BG_MAX_PROC_IN_NODE];
} utf_bg_mmap_header_info_t;

/*
 * Information about a mmap buffer
 */
typedef struct {
    /*
     * Checks whether the function calling utf barrier communication
     * has already acquired the shared memory.
     *   true  : utf barrier communication allocates, uses, and
     *           releases mmap shared memory by itself.
     *   false : Allocates shared memory in utf_shm_init function and
     *           releases it in utf_shm_finalize function.
     */
    bool disable_utf_shm;
    /* Name of the shared memory file */
    char    *file_name;
    /* Size of the shared memory file */
    size_t  size;
    /* Maximum number of processes in the node */
    size_t  width;
    /*
     * File descriptor
     * The leader process in the node sets it in utf_bg_alloc function
     * called in the first time.
     * The other processes set it in utf_bg_init function
     * called in the first time.
     */
    int     fd;
    /*
     * Pointer to mmap shared memory
     * The leader process in the node sets it in utf_bg_alloc function
     * called in the first time.
     * The other processes set it in utf_bg_init function
     * called in the first time.
     */
    utf_bg_mmap_header_info_t *mmaparea;
    /*
     * Checks whether the result of the first utf_bg_alloc function succeeds
     * about all processes.
     *   false : There were enough VBGs to use for all processes.
     *   true  : There was one or more processes where the number
     *           of VBGs was not enough to use.
     */
    bool first_alloc_is_failed;
} utf_bg_mmap_file_info_t;

typedef struct{
    /* Source to receive */
    size_t src_rmt_index;
    /* Destination to send */
    size_t dst_rmt_index;
} utf_rmt_src_dst_index_t;

/** Information for one barrier network handing between functions */
typedef struct{
    /*
     * The arguments for utf_bg_alloc function
     */
    utf_bg_alloc_argument_info_t arg_info;

    /*
     * Information about processes in the own node
     */
    utf_bg_intra_node_detail_info_t *intra_node_info;

    /*
     * Total number of VBGs used to build the barrier network
     * - Set the value in utf_bg_alloc function.
     */
    size_t num_vbg;

    /*
     * An array of VBGs (Number of elements is num_bgs.)
     * The first element is the start/end VBG.
     * - Allocates memory and sets values in utf_bg_alloc function.
     * - Frees memory in utf_bg_free function.
     */
    utofu_vbg_id_t *vbg_ids;

    /*
     * An array of arguments to utofu_set_vbgs function (Number of elements is num_bgs.)
     * The sequence of destination or source VCQs is set there.
     * - Allocates memory in utf_bg_alloc function.
     * - Frees memory after calling utofu_set_vbg () function
     *   with this as an argument in utf_bg_init function.
     */
    struct utofu_vbg_setting *vbg_settings;

    /*
     * An array of indexes of communication peer processes (Number of elements is num_bgs.)
     * - Allocates memory and sets values in the utf_bg_alloc function.
     * - Frees memory in utf_bg_init function.
     */
    utf_rmt_src_dst_index_t  *rmt_src_dst_seqs;

} utf_coll_group_detail_t;

/*
 * Declarations of external variables
 */
/* Whether the barrier in the node is Tofu barrier or not
 * true :  Tofu barrier
 * false : Software barrier using shared memory
 */
extern bool utf_bg_intra_node_barrier_is_tofu;

/** Information for one barrier network handing between functions */
extern utf_coll_group_detail_t *utf_bg_detail_info;
/*
 * Information about the barrier network created by the first utf_bg_alloc function call
 */
extern utf_coll_group_detail_t *utf_bg_detail_info_first_alloc;
/* Information about a mmap buffer */
extern utf_bg_mmap_file_info_t utf_bg_mmap_file_info;
/* TNI which should be used in the allocation of the next barrier network */
extern utofu_tni_id_t utf_bg_next_tni_id; 
/* Counter for utf_bg_alloc function call */
extern uint64_t utf_bg_alloc_count;
/* Vpid list in my node */
extern size_t *utf_bg_alloc_intra_world_indexes;

typedef struct {
    utofu_vbg_id_t utf_bg_poll_ids;
    void          *utf_bg_poll_odata;
    void          *utf_bg_poll_idata;
    void          *utf_bg_poll_result;
    int            utf_bg_poll_op;
    size_t         utf_bg_poll_count;
    uint64_t       utf_bg_poll_datatype;
    size_t         utf_bg_poll_size;
    int            utf_bg_poll_numcount;
    bool           utf_bg_poll_root;
    // Shared memory communication uses the following variables.
    bool           sm_flg_comm_start;/* Flag to check the start of barrier communication */
    int            sm_utofu_op;     /* Operation type(utofu) */
    int            sm_loop_counter; /* Loop counter */
    size_t         sm_loop_rank;    /* Loop rank */
    size_t         sm_numproc;      // == intra_node_info->size
    size_t         sm_intra_index;  // == intra_node_info->intra_index
    uint64_t       sm_seq_val;      // == intra_node_info->curr_seq
    volatile uint64_t *sm_mmap_buf; // == intra_node_info->mmap_buf
} utf_bg_poll_info_t;
extern utf_bg_poll_info_t poll_info;

#if defined(UTF_BG_UNIT_TEST)
/*
 * Temporary declarations of macros for the debugging
 * Uses some source codes of MPICH-tofu for unit tests of the functions
 * of utf barrier communication.
 */
extern void utofu_get_last_error(const char*);
extern int mypid;      /* Temporarily declared in utf_bg_alloc.c
                        * The function sets my_index when called for the first time. */
extern int utf_dflag;  /* Temporarily declared in utf_bg_alloc.c */

#define DLEVEL_UTOFU       0x2
#define DLEVEL_PROTO_VBG  0x40 /* 64 */

#ifdef UTF_DEBUG
#define DEBUG(mask) if (utf_dflag&(mask))
#else
#define DEBUG(mask) if (0)
#endif

#define  utf_printf printf

#define UTOFU_ERRCHECK_EXIT(rc) do {					\
    if (rc != UTOFU_SUCCESS) {						\
	char msg[1024];							\
	utofu_get_last_error(msg);					\
	printf("[%d] error: %s (code:%d) @ %d in %s\n",			\
	       mypid, msg, rc, __LINE__, __FILE__);			\
	fprintf(stderr, "[%d] error: %s (code:%d) @ %d in %s\n",	\
	        mypid, msg, rc, __LINE__, __FILE__);			\
	fflush(stdout); fflush(stderr);					\
	abort();							\
    }									\
} while(0);

#define UTOFU_ERRCHECK_EXIT_IF(abrt, rc) do {				\
    if (rc != UTOFU_SUCCESS) {						\
	char msg[1024];							\
	utofu_get_last_error(msg);					\
	printf("[%d] error: %s (code:%d) @ %d in %s\n",			\
	       mypid, msg, rc, __LINE__, __FILE__);			\
	fprintf(stderr, "[%d] error: %s (code:%d) @ %d in %s\n",	\
	        mypid, msg, rc, __LINE__, __FILE__);			\
	fflush(stdout); fflush(stderr);					\
	if (abrt) abort();						\
    }									\
} while(0);

#define UTOFU_CALL(abrt, func, ...) do {				\
    char msg[256];							\
    int rc;								\
    DEBUG(DLEVEL_UTOFU) {						\
	snprintf(msg, 256, "%s: calling %s(%s)\n", __func__, #func, #__VA_ARGS__); \
	utf_printf("%s", msg);						\
    }									\
    rc = func(__VA_ARGS__);						\
    UTOFU_ERRCHECK_EXIT_IF(abrt, rc);					\
} while (0);

#define UTOFU_CALL_RC(rc, func, ...) do {				\
    char msg[256];							\
    DEBUG(DLEVEL_UTOFU) {						\
	snprintf(msg, 256, "%s: calling %s(%s)\n", __func__, #func, #__VA_ARGS__); \
	utf_printf("%s", msg);						\
    }									\
    rc = func(__VA_ARGS__);						\
    return rc;								\
} while (0);

#define JTOFU_ERRCHECK_EXIT_IF(abrt, rc) do {				\
    if (rc != JTOFU_SUCCESS) {						\
	char msg[1024];							\
	utofu_get_last_error(msg);					\
	printf("[%d] error: %s (code:%d) @ %d in %s\n",			\
	       mypid, msg, rc, __LINE__, __FILE__);			\
	fprintf(stderr, "[%d] error: %s (code:%d) @ %d in %s\n",	\
	        mypid, msg, rc, __LINE__, __FILE__);			\
	fflush(stdout); fflush(stderr);					\
	if (abrt) abort();						\
    }									\
} while(0);

#define JTOFU_CALL(abrt, func, ...) do {				\
    char msg[256];							\
    int rc;								\
    DEBUG(DLEVEL_UTOFU) {						\
	snprintf(msg, 256, "%s: calling %s(%s)\n", __func__, #func, #__VA_ARGS__); \
	utf_printf("%s", msg);						\
    }									\
    rc = func(__VA_ARGS__);						\
    JTOFU_ERRCHECK_EXIT_IF(abrt, rc);					\
} while (0);

extern int utf_shm_finalize(void);
extern int utf_shm_init(size_t, void **);
extern struct utf_info utf_info;
#endif
