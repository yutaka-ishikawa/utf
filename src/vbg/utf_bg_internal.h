/*
 * Copyright (c) 2020 XXXXXXXXXXXXXXXXXXXXXXXX.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

/*
 * ヘッダファイルの読み込み
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
#include "utf_debug.h"
#include "utf.h"
extern int	utf_shm_init(size_t, void **);
extern int	utf_shm_finalize();
#include "utf_errmacros.h"
#include "utf_bg.h"
/*
 * マクロの定義
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
 * 構造体宣言
 */

/*
 * utf_bg_allocの引数情報
 */
typedef struct {
    /* utf_alloc_bgに渡されたranksetのアドレス    */
    uint32_t       *rankset;
    /* utf_alloc_bgに渡されたlenの値              */
    size_t         len;
   /* utf_alloc_bgに渡されたmy_indexの値          */
    size_t         my_index;
} utf_bg_alloc_argument_info_t;

/*
 * ノード内プロセス情報
 */
typedef struct {
    /* 自プロセスがノード内マネージャーかワーカーか */
    size_t   state;
    /* ノード内プロセス数*/
    size_t   size;
    /* 自プロセスのノード内インデックス*/
    size_t   intra_index;
    /* シーケンス番号 */
    uint64_t curr_seq;
    /* 自プロセスのノード内共有メモリ通信領域の先頭アドレス */
    volatile uint64_t *mmap_buf;
} utf_bg_intra_node_detail_info_t;

/*
 * mmapの先頭領域に格納される情報
 */
typedef struct {
    /*
     * 共有メモリセグメント削除制御用領域(atomic_ulong)  
     */
#if (!defined(TOFUGIJI))
    atomic_ulong  ul;
#else
    unsigned long ul;
#endif
    /*
     * 最初のutf_bg_alloc呼び出しで作成されたMPI_COMM_WORLDの始点ゲート
     * uint64_t
     *
     * utf_bg_intra_node_barrier_is_tofuが真の場合
     *   : すべてのプロセスがutf_bg_allocで設定する。
     *     utf_bg_init側で参照する必要はない。
     *
     * utf_bg_intra_node_barrier_is_tofuが偽の場合
     *   : ノード内マネージャーがutf_bg_allocで設定する。
     *     ノード内ワーカーがutf_bg_initで参照する必要がある。
     */
    utofu_vbg_id_t first_vbg_id;
    /* ノード内マネージャーが確保した始点BGID               uint16_t */
    utofu_bg_id_t  manager_start_bg_id;
    /* ノード内マネージャーがバリアゲートを確保したTNI番号  uint16_t */
    utofu_tni_id_t manager_tni_id;
    /* 次コミュニケータが使用する先頭のTNI番号              uint16_t */
    utofu_tni_id_t next_tni_id;
} utf_bg_mmap_header_info_t;

/*
 * mmapファイル情報
 */
typedef struct {
    /*
     * 共有メモリをUTF本体側で確保しないことを指示
     *   true  : UTFバリア通信が自身でmmapによる共有メモリ領域を作成、使用、解放
     *   false : utf_shm_initで共有メモリ領域を確保、utf_shm_finalizeで解放
     */
    bool disable_utf_shm;
    /* ファイル名 */
    char    *file_name;
    /* ファイル、mmap領域の大きさ */
    size_t  size;
    /* 最大ノード内プロセス数 */
    size_t  width;
    /*
     * ファイルディスクリプタ
     * UTF_BG_INTRA_NODE_MANAGERはutf_bg_allocの初回呼び出しで代入
     * UTF_BG_INTRA_NODE_WORKERはutf_bg_initの初回呼び出しで代入
     */
    int     fd;
    /*
     * mmap領域の先頭アドレス
     * UTF_BG_INTRA_NODE_MANAGERはutf_bg_allocの初回呼び出しで代入
     * UTF_BG_INTRA_NODE_WORKERはutf_bg_initの初回呼び出しで代入
     */
    utf_bg_mmap_header_info_t *mmaparea;
    /*
     * 最初のutf_bg_alloc呼び出しで作成されたMPI_COMM_WORLDの始点ゲート
     */
    utofu_vbg_id_t first_vbg_id;
    /*
     * 全プロセスにおける、最初のutf_bg_alloc呼び出し結果
     *   true  : 全てのプロセスがVBGを確保できた。
     *   false : VBGを確保できなかったプロセスが存在する。
     */
    bool first_alloc_is_failed;
} utf_bg_mmap_file_info_t;

typedef struct{
    /* 受信 */
    size_t src_rmt_index;
    /* 送信 */
    size_t dst_rmt_index;
} utf_rmt_src_dst_index_t;

/** 通信開始側と演算完了側で共有する情報 */
/** Information shared by communication start processiong and end processing */

typedef struct{
    /*
     * utf_bg_allocの引数情報
     */
    utf_bg_alloc_argument_info_t arg_info;

    /*
     * ノード内プロセス情報
     */
    utf_bg_intra_node_detail_info_t *intra_node_info;

    /*
     * バリア回路構築用に獲得したVBGの総数
     *   utf_bg_alloc(): 値を設定
     */
    size_t num_vbg;

    /*
     * 確保されたVBGのID(×num_vbg)
     * 先頭が開始終了VBG
     *   utf_bg_alloc(): 領域を確保、値を設定
     *   utf_bg_free():  領域を解放
     */
    utofu_vbg_id_t *vbg_ids;

    /*
     * 該当コミュニケータ―に対応する送受信シーケンス(×num_vbg)
     *   utf_bg_alloc(): 領域を確保
     *   utf_bg_init():  utofu_set_vbg()で使用、領域を解放
     */
    struct utofu_vbg_setting *vbg_settings;

    /*
     * 通信相手ノードのインデックス(×num_vbg)
     *   utf_bg_alloc(): 領域を確保、値を設定
     *   utf_bg_init():  領域を解放
     */
    utf_rmt_src_dst_index_t  *rmt_src_dst_seqs;

} utf_coll_group_detail_t;

/*
 * 外部変数の宣言
 */
/* 真:ノード内ハードバリア 偽:ノード内ソフトバリア */
extern bool utf_bg_intra_node_barrier_is_tofu;
/* utf_bg_init,utf_barrier,utf_(all)reduce,向けのバリア情報 */
extern utf_coll_group_detail_t *utf_bg_detail_info;
/*
 * utf_bg_init,utf_barrier,utf_(all)reduce,向けのMPI_COMM_WORLDバリア情報
 * (utf_bg_alloc初回呼び出し時に作成されたバリア情報)
 */
extern utf_coll_group_detail_t *utf_bg_detail_info_first_alloc;
/* mmapファイル情報 */
extern utf_bg_mmap_file_info_t utf_bg_mmap_file_info;
/* 次コミュニケータが使用する先頭のTNI番号 */
extern utofu_tni_id_t utf_bg_next_tni_id; 
/* utf_bg_alloc呼び出し回数 */
extern uint64_t utf_bg_alloc_count;

typedef struct {
    utofu_vbg_id_t utf_bg_poll_ids;
    void          *utf_bg_poll_odata;
    void          *utf_bg_poll_idata;
    void          *utf_bg_poll_result;
    int            utf_bg_poll_op;
    int            utf_bg_poll_count;
    uint64_t       utf_bg_poll_datatype;
    size_t         utf_bg_poll_size;
    int            utf_bg_poll_numcount;
    bool           utf_bg_poll_root;
    // 以降はsmで使用
    bool           sm_flg_comm_start;/* Flag to check the start of barrier communication */
    int            sm_utofu_op;     /* Operation type(utofu) */
    int            sm_loop_counter; /* Loop counter */
    size_t         sm_loop_rank;    /* Loop rank */
    size_t         sm_state;        // == intra_node_info->state
    size_t         sm_numproc;      // == intra_node_info->size
    size_t         sm_intra_index;  // == intra_node_info->intra_index
    uint64_t       sm_seq_val;      // == intra_node_info->curr_seq
    volatile uint64_t *sm_mmap_buf; // == intra_node_info->mmap_buf
} utf_bg_poll_info_t;
extern utf_bg_poll_info_t poll_info;

#if defined(UTF_BG_UNIT_TEST)
/*
 * 仮デバッグマクロ
 * UTF本体結合するまでのテスト用
 */
extern void utofu_get_last_error(const char*);
extern int mypid;      /* utf_bg_alloc.cに仮に作成 初回呼び出しのmy_indexを入れる */
extern int utf_dflag;  /* utf_bg_alloc.cに仮に作成 */

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
