/*
 * Copyright (c) 2020 XXXXXXXXXXXXXXXXXXXXXXXX.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

/*
 * @file   utf_bg_alloc.c
 * @brief  Implementation of utf_bg_alloc()
 */

/*
 * Include of head files
 */
#include "utf_bg_alloc.h"

/*
 * 外部変数の宣言
 */
static bool utf_bg_first_call=true;
            /* 最初の呼び出しかどうかの判断
             * 最初の呼び出しはMPI_COMM_WORLDに対してものと想定する。
             * 最初の呼び出し時には、以下を行う。
             * - mmap領域の作成
             * - MPI_COMM_WORLDの始点ゲートを保存
             */
bool utf_bg_intra_node_barrier_is_tofu=true;
            /* 真:ノード内ハードバリア 偽:ノード内ソフトバリア */
utf_coll_group_detail_t *utf_bg_detail_info;
            /* utf_bg_init,utf_barrier,utf_(all)reduce,向けのバリア情報 */
utf_coll_group_detail_t *utf_bg_detail_info_first_alloc;
            /* utf_bg_alloc初回呼び出し時に作成されたMPI_COMM_WORLDバリア情報*/
static int my_jobid=(-1);                      /* 自ジョブのジョブID */
static char my_hostname[64];
static int ppn_job;

utf_bg_mmap_file_info_t utf_bg_mmap_file_info; /* mmap用ファイルの情報 */

static unsigned long int utf_bg_alloc_flags = 0UL;
static int max_intra_node_procs_for_hb = UTF_BG_ALLOC_MAX_PROC_IN_NODE_FOR_HB;
utofu_tni_id_t utf_bg_next_tni_id;
uint64_t utf_bg_alloc_count;
            /* utf_bg_alloc呼び出し回数 == utf_bg_allocがUTF_SUCCESSで返った数 */

#if defined(DEBUGLOG)
static size_t my_rankset_index; /* Rank of the process */
static int xonlyflag = 0;
#endif
#if defined(UTF_BG_UNIT_TEST)
/*
 * 仮デバッグマクロ
 * UTF本体結合するまでのテスト用
 */
int mypid;
int utf_dflag;
#endif

/*
 * 内部関数
 * 内部マクロ処理
 */

/*
 * アクシステーブル
 * ノード内プロセス情報
 * の作成
 */

/* Get the nodeid from rankset */
#define GET_NODEID_FROM_RANKSET(nodeid,ranksetposition)               \
{                                                                     \
    nodeid=0U;                                                        \
    union jtofu_phys_coords *coords =                                 \
        &((utf_info.vname + ranksetposition)->xyzabc);                \
    nodeid = ((uint32_t)coords->s.c << UTF_BG_ALLOC_BITSFT_COORD_C) | \
             ((uint32_t)coords->s.b << UTF_BG_ALLOC_BITSFT_COORD_B) | \
             ((uint32_t)coords->s.a << UTF_BG_ALLOC_BITSFT_COORD_A) | \
             ((uint32_t)coords->s.z << UTF_BG_ALLOC_BITSFT_COORD_Z) | \
             ((uint32_t)coords->s.y << UTF_BG_ALLOC_BITSFT_COORD_Y) | \
             ((uint32_t)coords->s.x << UTF_BG_ALLOC_BITSFT_COORD_X);  \
}
/* Get the nodeid from VBG ID */
#define GET_NODEID_FROM_VCQ(nodeid,ranksetposition)                  \
{                                                                    \
    nodeid = (uint32_t)( (ranksetposition & UTF_BG_ALLOC_MSK_6DADDR) \
                                 >> UTF_BG_BITSFT_BGINFO_COORD_X );  \
}



/* Compare my 3D logical address to peer's 3D logical address */
#define COMPARE_LOGADDR(my,temp) \
{ \
    if (my.s.z == temp.s.z) {                                      \
        if (my.s.y == temp.s.y) {                                  \
            if (my.s.x == temp.s.x) {                              \
                /* 自プロセスと同じノードを検出。 */               \
                nodeinfo[i].axis_kind = UTF_BG_ALLOC_AXIS_IN_NODE; \
            }                                                      \
            else {                                                 \
                /* Xのみが異なる。 */                              \
                nodeinfo[i].axis_kind = UTF_BG_ALLOC_AXIS_ON_X;    \
            }                                                      \
        }                                                          \
        else if (my.s.x == temp.s.x) {                             \
            /* Yのみが異なる。 */                                  \
            nodeinfo[i].axis_kind = UTF_BG_ALLOC_AXIS_ON_Y;        \
        }                                                          \
        else {                                                     \
            /* 2つの軸が異なる。 */                                \
            nodeinfo[i].axis_kind = UTF_BG_ALLOC_AXIS_OFF_AXES;    \
        }                                                          \
    }                                                              \
    else if ( (my.s.y == temp.s.y) &&                              \
              (my.s.x == temp.s.x) ) {                             \
        /* Zのみが異なる。 */                                      \
        nodeinfo[i].axis_kind = UTF_BG_ALLOC_AXIS_ON_Z;            \
    }                                                              \
    else {                                                         \
        /* 2つ以上の軸が異なる。 */                                \
        nodeinfo[i].axis_kind = UTF_BG_ALLOC_AXIS_OFF_AXES;        \
    }                                                              \
    /* 論理3次元アドレスの最大値、最小値を記録する。*/             \
    /* iを再利用して良い。*/                                       \
    for (i=(size_t)UTF_BG_ALLOC_AXIS_ON_X;                         \
         i<=(size_t)UTF_BG_ALLOC_AXIS_ON_Z;i++) {                  \
        /* a[0->X,1->Y,2->Z] */                                    \
        if ((axisptr + i)->biggest  < temp.a[i-1]) {               \
            (axisptr + i)->biggest = temp.a[i-1];                  \
        }                                                          \
        else if (temp.a[i-1] < (axisptr + i)->smallest ) {         \
            (axisptr + i)->smallest = temp.a[i-1];                 \
        }                                                          \
    }                                                              \
}

/* Freeing temporary data buffers */
#define FREEAREA_AXES()                                     \
{                                                           \
    if (my_node_indexes != NULL) { free(my_node_indexes); } \
    if (nodeinfo != NULL)        { free(nodeinfo); }        \
    if (procinfo != NULL)        { free(procinfo); }        \
}

/*
 * 軸毎の軸に所属するノードの情報配列を物理6次元アドレス順に並び替える
 *
 * node_a (IN) ノード情報
 * node_b (IN) ノード情報
 */
static int sort_nodeinfo_by_phy_6d_addr(const void *node_a,const void *node_b)
{
    utf_bg_alloc_axis_node_info_t *na_ptr =
        (utf_bg_alloc_axis_node_info_t *)node_a;
    utf_bg_alloc_axis_node_info_t *nb_ptr =
        (utf_bg_alloc_axis_node_info_t *)node_b;

    /* acb -> xyz */

    if (na_ptr->phy_6d_addr.s.z != nb_ptr->phy_6d_addr.s.z){
        return (na_ptr->phy_6d_addr.s.z - nb_ptr->phy_6d_addr.s.z);
    }
    if (na_ptr->phy_6d_addr.s.y != nb_ptr->phy_6d_addr.s.y){
        return (na_ptr->phy_6d_addr.s.y - nb_ptr->phy_6d_addr.s.y);
    }
    if (na_ptr->phy_6d_addr.s.x != nb_ptr->phy_6d_addr.s.x){
        return (na_ptr->phy_6d_addr.s.x - nb_ptr->phy_6d_addr.s.x);
    }
    if (na_ptr->phy_6d_addr.s.b != nb_ptr->phy_6d_addr.s.b){
        return (na_ptr->phy_6d_addr.s.b - nb_ptr->phy_6d_addr.s.b);
    }
    if (na_ptr->phy_6d_addr.s.c != nb_ptr->phy_6d_addr.s.c){
        return (na_ptr->phy_6d_addr.s.c - nb_ptr->phy_6d_addr.s.c);
    }
    if (na_ptr->phy_6d_addr.s.a != nb_ptr->phy_6d_addr.s.a){
        return (na_ptr->phy_6d_addr.s.a - nb_ptr->phy_6d_addr.s.a);
    }

    return 0;
}

/*
 * ノード数、ノード内プロセス数情報、ノード内プロセスの中での自プロセスの位置
 * を計算する。
 *
 * rankset      (IN)  バリアに参加する全てのプロセスのVCQのIDの配列
 *   (IN)     len       Total number of the process.
 *   (IN)     my_index  Rank of the process.
 * my_nodeid    (IN)  自プロセスが存在するノードのID
 * oneppn       (OUT) ノード内プロセスが1か2以上かの情報
 * num_nodes_job(OUT) ノード数
 * target_index (OUT) ノード内プロセスの中での自プロセスの位置
 */
static int check_ppn_make_nodenum_make_targer_index(
                                                    uint32_t *rankset,
                                                    size_t len,
                                                    size_t my_index,
                                                    uint32_t my_nodeid,
                                                    bool   *oneppn,
                                                    size_t *num_nodes_job,
                                                    size_t *target_index)
{

    if (2 <= ppn_job) {
        /*
         * ノード内プロセスの中での自プロセスの位置を計算
         * "何番目"ではなくノード内プロセス配列の添え字になるように計算する。
         *
         * rankset
         * +-----------------------------------+
         * | a| a| b| b| c| c| d| d| e| e| f| f|
         * +-----------------------------------+
         *   0  1  2  3  4  5  6  7  8  9 10 11
         *   0  1  0  1  0  1  0  1  0  1  0  1 ->target_idex
         * <--------------- len --------------->
         *  my_index=5とすると、target_indexは1
         *  ransetにおけるノード間で通信するプロセスのindexは
         *  1,3,5(自分),7,9,11となる。
         *
         */
        /*
         * ノード数の計算
         * rankset
         * +-----------------------------------+
         * | a| a| b| b| b| c| c| c| a| d| d| b|
         * +-----------------------------------+
         * <--------------- len --------------->
         *
         * save_nodeids
         * +-----------------------------------+
         * | a| b| c| d|-1|-1|-1|-1|-1|-1|-1|-1|
         * +-----------------------------------+
         * <-ノード数-->
         * <--------------- len --------------->
         *
         * 160,000 x 4 x sizeof(uint32_t) = 2,560,000 -> 2.5MB
         */
        uint32_t *save_nodeids;
        size_t s,i;
        bool found_index;
        uint32_t temp_nodeid;

        save_nodeids = (uint32_t *)malloc(sizeof(uint32_t) * len);
        if (NULL == save_nodeids) {
            return UTF_ERR_OUT_OF_MEMORY;
        }
        memset((void *)save_nodeids,UTF_BG_ALLOC_INVALID_NODEID,
            sizeof(uint32_t) * len);
        *oneppn = false;
        *num_nodes_job = 0UL;
        *target_index = 0UL;
        for (found_index=false,s=0;s<len;s++) {
            GET_NODEID_FROM_RANKSET(temp_nodeid,rankset[s]);
            if ( (!found_index) && (temp_nodeid == my_nodeid) ) {
                if (my_index == s) {
                    found_index=true;
                }
                else {
                    (*target_index)++;
                }
            }
            for (i=0;i<*num_nodes_job;i++) {
                if (save_nodeids[i] == temp_nodeid) { break; }
            }
            if (i == *num_nodes_job) {
                /* 新しいノードを発見した */
                save_nodeids[*num_nodes_job] = temp_nodeid;
                (*num_nodes_job)++;
            }
        }
        free(save_nodeids);
    }
    else {
        *oneppn = true;
        *num_nodes_job = len;
        *target_index=0;
    }
    return UTF_SUCCESS;
}

/* Get the 6D physical coordinates from the nodeid */
#define GET_PHY_6D_ADDR_FROM_NODEID(nodeid,addr)                               \
{                                                                              \
    addr.s.x = (uint8_t)                                                       \
        ((nodeid & UTF_BG_ALLOC_MSK_6DADDR_X) >> UTF_BG_ALLOC_BITSFT_COORD_X); \
    addr.s.y = (uint8_t)                                                       \
        ((nodeid & UTF_BG_ALLOC_MSK_6DADDR_Y) >> UTF_BG_ALLOC_BITSFT_COORD_Y); \
    addr.s.z = (uint8_t)                                                       \
        ((nodeid & UTF_BG_ALLOC_MSK_6DADDR_Z) >> UTF_BG_ALLOC_BITSFT_COORD_Z); \
    addr.s.a = (uint8_t)                                                       \
        ((nodeid & UTF_BG_ALLOC_MSK_6DADDR_A) >> UTF_BG_ALLOC_BITSFT_COORD_A); \
    addr.s.b = (uint8_t)                                                       \
        ((nodeid & UTF_BG_ALLOC_MSK_6DADDR_B) >> UTF_BG_ALLOC_BITSFT_COORD_B); \
    addr.s.c = (uint8_t)                                                       \
        ((nodeid & UTF_BG_ALLOC_MSK_6DADDR_C) >> UTF_BG_ALLOC_BITSFT_COORD_C); \
}

/*
 * make each of nodes information which is belong to this axis
 * which is in "utf_bf_alloc_proc_info_t"
 *
 *   (IN)  rankset 
 *   (OUT) dst  A pointer to each of nodes information which is belong to axes
 *   (IN)  src  A pointer to the array of each of processes information.
 */
static void set_axis_node_info(
                               uint32_t *rankset,
                               utf_bg_alloc_axis_node_info_t *dst,
                               utf_bf_alloc_proc_info_t      *src)
{

    dst->index = src->index;
    dst->log_3d_addr.s.x = utf_info.vname[rankset[dst->index]].xyz.s.x;
    dst->log_3d_addr.s.y = utf_info.vname[rankset[dst->index]].xyz.s.y;
    dst->log_3d_addr.s.z = utf_info.vname[rankset[dst->index]].xyz.s.z;
    dst->phy_6d_addr.s.x = utf_info.vname[rankset[dst->index]].xyzabc.s.x;
    dst->phy_6d_addr.s.y = utf_info.vname[rankset[dst->index]].xyzabc.s.y;
    dst->phy_6d_addr.s.z = utf_info.vname[rankset[dst->index]].xyzabc.s.z;
    dst->phy_6d_addr.s.a = utf_info.vname[rankset[dst->index]].xyzabc.s.a;
    dst->phy_6d_addr.s.b = utf_info.vname[rankset[dst->index]].xyzabc.s.b;
    dst->phy_6d_addr.s.c = utf_info.vname[rankset[dst->index]].xyzabc.s.c;

    return;
}

/*
 * make each of axes information
 *
 * rankset      (IN)  バリアに参加する全てのプロセスのVCQのIDの配列
 *   (IN)     len       Total number of the process.
 *   (IN)     my_index  Rank of the process.
 *   (OUT)    axisptr   A pointer to the array of each of axes information.
 * intrainfo    (OUT) ノード内プロセス情報領域アドレスを返すポインタ
 * errcase      (OUT) エラー情報
 *
 */
int make_axes_and_intra_node_infos(
                                   uint32_t       *rankset,
                                   size_t len,
                                   size_t my_index,
                                   int max_ppn,
                                   utf_bg_alloc_axis_detail_info_t *axisptr,
                                   utf_bg_intra_node_detail_info_t **intrainfo,
                                   uint8_t *errcase)
{
    int rc;
    bool found,toomany,unmatch,oneppn=false,hexa;
    size_t i,s,target_index=0;
    size_t num_nodes_job=0,num_nodes=0,my_nodeinfo_index=0,procinfo_size,end_axis;
    size_t *my_node_indexes=NULL;
    uint32_t my_nodeid,nodeid;
    utf_bf_alloc_node_info_t *nodeinfo=NULL;
    utf_bf_alloc_proc_info_t *procinfo=NULL;
    utf_bg_alloc_axis_node_info_t *ninfoptrs[UTF_BG_ALLOC_AXIS_NUM_AXES];
#if defined(DEBUGLOG)
    size_t ninfoptrs_cnt[UTF_BG_ALLOC_AXIS_NUM_AXES];
#endif
    utf_bg_intra_node_detail_info_t *intraptr;

    /* 自プロセスのノードIDと3次元アドレスを求める */
    GET_NODEID_FROM_RANKSET(my_nodeid,rankset[my_index]);

    /*
     * calculate number of nodes.
     * make each of nodes information.
     */

    /* ノード内プロセス数の最大値を問い合わせる */
    toomany = false;
    if (utf_bg_first_call) {
        ppn_job = max_ppn;
#if defined(DEBUGLOG)
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: utf_bg_first_call=%d [UTF_BG_UNDER_MPICH_TOFU ver4.0] rankset is VPID version. ppn_job=%d \n"
                ,my_hostname,__func__,my_index
                ,utf_bg_first_call,ppn_job
        );
#endif
    }
    rc = check_ppn_make_nodenum_make_targer_index(rankset,
                                                  len,
                                                  my_index,
                                                  my_nodeid,
                                                  &oneppn,
                                                  &num_nodes_job,
                                                  &target_index);
#if defined(DEBUGLOG)
    fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: check_ppn_make_nodenum_make_targer_index=%d oneppn=%d num_nodes_job=%zu target_index=%zu \n"
        ,my_hostname,__func__,my_index
        ,rc,oneppn,num_nodes_job,target_index
    );
#endif
    if (UTF_SUCCESS != rc) {
        *intrainfo = NULL;
        FREEAREA_AXES();
        return rc;
    }

    /* ranksetの中の自プロセスと同じノードのプロセスのindex */
    my_node_indexes = (size_t *)malloc(sizeof(size_t) * ppn_job);
    if (NULL == my_node_indexes) {
        *intrainfo = NULL;
        FREEAREA_AXES();
        return UTF_ERR_OUT_OF_MEMORY;
    }
    for (i=0UL;i<(size_t)ppn_job;i++) {
        my_node_indexes[i] = UTF_BG_ALLOC_INVALID_INDEX;
    }

    nodeinfo = (utf_bf_alloc_node_info_t *)malloc(
        sizeof(utf_bf_alloc_node_info_t) * num_nodes_job);
    if (NULL == nodeinfo) {
        *intrainfo = NULL;
        FREEAREA_AXES();
        return UTF_ERR_OUT_OF_MEMORY;
    }
    memset((void *)nodeinfo,0,sizeof(utf_bf_alloc_node_info_t) * num_nodes_job);

    (axisptr + 0)->biggest  = 0UL;
    (axisptr + 0)->smallest = 0UL;
    for (i=(size_t)UTF_BG_ALLOC_AXIS_ON_X;
         i<=(size_t)UTF_BG_ALLOC_AXIS_ON_Z;i++) {
        /* coords[0->X,1->Y,2->Z] */
        (axisptr + i)->biggest  = utf_info.vname[rankset[my_index]].xyz.a[i-1];
        (axisptr + i)->smallest = utf_info.vname[rankset[my_index]].xyz.a[i-1];
    }

    for (s=0UL;s<len;s++) {
        GET_NODEID_FROM_RANKSET(nodeid,rankset[s]);
        for (found=false,i=0UL;i<num_nodes;i++) {
            if (nodeinfo[i].nodeid == nodeid) {
                found = true;
                break;
            }
        }
        assert(i < num_nodes_job);
        if (found) {
            /* 同じノードが存在。*/
            if ((size_t)UTF_BG_ALLOC_MAX_PROC_IN_NODE <= nodeinfo[i].size) {
                /* 49ppn以上が指定された。*/
                *intrainfo = NULL;
                FREEAREA_AXES();
                return UTF_ERR_INTERNAL;
            }
            /* ノードIDが自プロセスのノードIDと一致するか？ */
            if (my_nodeid == nodeid) {
                assert(nodeinfo[i].size < ppn_job);
                my_node_indexes[nodeinfo[i].size] = s;
            }
            /* ノード間代表プロセスのindexの更新 */
            if (nodeinfo[i].size == target_index) {
                nodeinfo[i].manager_index = s;
            }
            /* ノード内プロセス数の更新 */
            nodeinfo[i].size++;
            if ((size_t)max_intra_node_procs_for_hb <
                nodeinfo[i].size) {
                /* 5ppn以上のノードを発見したため、記憶する。*/
                toomany = true;
            }
        }
        else {
            /* 新しいノード。*/
            nodeinfo[i].nodeid = nodeid;
            /* ノードIDが自プロセスのノードIDと一致するか？ */
            if (my_nodeid == nodeid) {
                my_nodeinfo_index = num_nodes;
                assert(nodeinfo[i].size < ppn_job);
                my_node_indexes[nodeinfo[i].size] = s;
            }
            /* ノード間代表プロセスのindexの更新 */
            nodeinfo[i].manager_index = s;
            /* ノード内プロセス数の更新 */
            nodeinfo[i].size++;
            COMPARE_LOGADDR(utf_info.vname[rankset[my_index]].xyz,
                            utf_info.vname[rankset[s]].xyz);
            num_nodes++;
            assert(num_nodes <= num_nodes_job);
        } /* 新しいノード。 */
    } /* s */

#if defined(DEBUGLOG)
    for (i=0UL;i<(size_t)ppn_job;i++) {
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu:MYNODEINFO %zu/%zu my_node_indexes=%zu \n"
            ,my_hostname,__func__,my_index
            ,i,(size_t)ppn_job,my_node_indexes[i]
        );
    }
    for (i=0UL;i<num_nodes;i++) {
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu:NODEINFO %zu/%zu toomany=%d nodeid=%x size=%d axis_kind=%d manager_index=%zu log3daddr=%u-%u-%u \n"
            ,my_hostname,__func__,my_index
            ,i,num_nodes,toomany
            ,nodeinfo[i].nodeid,nodeinfo[i].size,nodeinfo[i].axis_kind
            ,nodeinfo[i].manager_index
            ,utf_info.vname[rankset[nodeinfo[i].manager_index]].xyz.s.x
            ,utf_info.vname[rankset[nodeinfo[i].manager_index]].xyz.s.y
            ,utf_info.vname[rankset[nodeinfo[i].manager_index]].xyz.s.z
        );
    }
#endif

    /*
     * make each of processes information.
     */
    intraptr = (utf_bg_intra_node_detail_info_t *)malloc(
       sizeof(utf_bg_intra_node_detail_info_t));
    if (NULL == intraptr) {
        *intrainfo = NULL;
        FREEAREA_AXES();
        return UTF_ERR_OUT_OF_MEMORY;
    }
    memset((void *)intraptr,0,sizeof(utf_bg_intra_node_detail_info_t));

    procinfo = (utf_bf_alloc_proc_info_t *)malloc(
        sizeof(utf_bf_alloc_proc_info_t) * len);
    if (NULL == procinfo) {
        free(intraptr);
        *intrainfo = NULL;
        FREEAREA_AXES();
        return UTF_ERR_OUT_OF_MEMORY;
    }
    memset((void *)procinfo,0,sizeof(utf_bf_alloc_proc_info_t) * len);
    procinfo_size = 0UL;

    /* UTF_BG_ALLOC_AXIS_IN_NODE,UTF_BG_ALLOC_AXIS_MYSELFの作成。*/
    /* ノード内プロセス情報の作成。*/
    intraptr->size = (size_t)nodeinfo[my_nodeinfo_index].size;
    for (s=0UL;s<nodeinfo[my_nodeinfo_index].size;s++) {
        (procinfo + procinfo_size)->nodeid =
            nodeinfo[my_nodeinfo_index].nodeid;
        (procinfo + procinfo_size)->axis_kind =
            nodeinfo[my_nodeinfo_index].axis_kind;
        if (my_index == my_node_indexes[s]) {
            (procinfo + procinfo_size)->axis_kind = UTF_BG_ALLOC_AXIS_MYSELF;
            intraptr->state = (0 == s) ?
                 UTF_BG_INTRA_NODE_MANAGER : UTF_BG_INTRA_NODE_WORKER;
            intraptr->intra_index = s;
        }
        (procinfo + procinfo_size)->index = my_node_indexes[s];
        procinfo_size++;
        assert(procinfo_size <= len);
    }

    /* UTF_BG_ALLOC_AXIS_ON_X,Y,Z,UTF_BG_ALLOC_AXIS_NUM_AXESの作成。*/

    unmatch = false;
    for (i=0UL;i<num_nodes;i++) {
        if (0 == nodeinfo[i].size) { continue; }
        if ( (utf_bg_intra_node_barrier_is_tofu) &&
             (i) &&
             (nodeinfo[i].size != nodeinfo[i-1].size) ) {
            unmatch=true;
            break;
        }
        if (i == my_nodeinfo_index) {
            assert( nodeinfo[i].nodeid == my_nodeid);
            assert(nodeinfo[i].axis_kind == UTF_BG_ALLOC_AXIS_IN_NODE);
            continue;
        }

        /* ここでは、絶対にノード内プロセス数が一致しているはず。*/
        (procinfo + procinfo_size)->nodeid      = nodeinfo[i].nodeid;
        (procinfo + procinfo_size)->axis_kind   = nodeinfo[i].axis_kind;
        (procinfo + procinfo_size)->index = nodeinfo[i].manager_index;
        procinfo_size++;
        assert(procinfo_size <= len);
    }

#if defined(DEBUGLOG)
    for (i=0UL;i<procinfo_size;i++) {
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu:PROCINFO %zu/%zu unmatch=%d nodeid=%x axis_kind=%d index=%zu log3daddr=%u-%u-%u \n"
            ,my_hostname,__func__,my_index
            ,i,procinfo_size,unmatch
            ,procinfo[i].nodeid,procinfo[i].axis_kind
            ,procinfo[i].index
            ,utf_info.vname[rankset[procinfo[i].index]].xyz.s.x
            ,utf_info.vname[rankset[procinfo[i].index]].xyz.s.y
            ,utf_info.vname[rankset[procinfo[i].index]].xyz.s.z
        );
    }
#endif

    rc = UTF_SUCCESS;
    if (unmatch) {
        *errcase |= UTF_BG_ALLOC_UNMATCH_PROC;
        rc = UTF_BG_ALLOC_ERR_UNMATCH_PROC_PER_NODE;
    }
    if ( (!utf_bg_intra_node_barrier_is_tofu) &&
         (num_nodes < (size_t)UTF_BG_ALLOC_REQUIRED_NODE) ) {
        /* ノード内ソフトバリア時にノード数が足りない。*/
        *errcase |= UTF_BG_ALLOC_TOO_FEW_NODE;
        rc = UTF_BG_ALLOC_ERR_TOO_FEW_NODE;
    }
    if ((toomany) && (utf_bg_intra_node_barrier_is_tofu)) {
        /* ノード内プロセス数過多。*/
        *errcase |= UTF_BG_ALLOC_TOO_MANY_PROC;
        rc = UTF_BG_ALLOC_ERR_TOO_MANY_PROC_IN_NODE;
    }
    if (UTF_SUCCESS != rc) {
        FREEAREA_AXES(); 
        *intrainfo = intraptr; 
        return rc;
    }

    /* 各軸の長さを計算。*/
    for (i=(size_t)UTF_BG_ALLOC_AXIS_ON_X;
         i<=(size_t)UTF_BG_ALLOC_AXIS_ON_Z;i++) {
        (axisptr + i)->size =
            (size_t)((axisptr + i)->biggest - (axisptr + i)->smallest + 1);
    }
    (axisptr + UTF_BG_ALLOC_AXIS_IN_NODE)->size = intraptr->size;
#if defined(DEBUGLOG)
    fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu:INTRAINFO ptr=%p state=%ld size=%zu intra_index=%zu log3daddr=%u-%u-%u \n"
        ,my_hostname,__func__,my_index
        ,intraptr,intraptr->state,intraptr->size,intraptr->intra_index
        ,utf_info.vname[rankset[my_index]].xyz.s.x
        ,utf_info.vname[rankset[my_index]].xyz.s.y
        ,utf_info.vname[rankset[my_index]].xyz.s.z
    );
#endif
    for (i=(size_t)UTF_BG_ALLOC_AXIS_IN_NODE;
         i<(size_t)UTF_BG_ALLOC_AXIS_NUM_AXES;i++) {
#if defined(DEBUGLOG)
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu:AXISINFO %zu biggest=%u smallest=%u size=%zu \n"
            ,my_hostname,__func__,my_index
            ,i,(axisptr + i)->biggest,(axisptr + i)->smallest
            ,(axisptr + i)->size
        );
        ninfoptrs_cnt[i]=0UL;
#endif
        ninfoptrs[i]=NULL;
    }

    if ( !utf_bg_intra_node_barrier_is_tofu &&
         (UTF_BG_INTRA_NODE_WORKER == intraptr->state) ) {
        FREEAREA_AXES();
        *intrainfo = intraptr;
        return UTF_SUCCESS;
    }

    /*
     * 軸毎の所属ノード情報の作成
     */
    if (num_nodes == (axisptr + UTF_BG_ALLOC_AXIS_ON_X)->size *
                     (axisptr + UTF_BG_ALLOC_AXIS_ON_Y)->size *
                     (axisptr + UTF_BG_ALLOC_AXIS_ON_Z)->size) {
        /* ノード割り当てが六面体*/
        end_axis = (size_t)UTF_BG_ALLOC_AXIS_NUM_AXES;
        hexa = true;
    }
    else {
        end_axis = (size_t)UTF_BG_ALLOC_AXIS_ON_Y;
        hexa = false;
    }

#if defined(DEBUGLOG)
   if (xonlyflag) {
        end_axis = (size_t)UTF_BG_ALLOC_AXIS_ON_Y;
        hexa = false;
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: FORCE NOT HEXA!!! \n"
            ,my_hostname,__func__,my_index
        );
   }
#endif

    if (!hexa) {
        /* 6面体割り当てではないのでノード間は、一つの軸(X)に纏める。*/
#if defined(DEBUGLOG)
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: ISNOT HEXA X=%zu->%zu Y=%zu->0 Z=%zu->0 \n"
            ,my_hostname,__func__,my_index
            ,(axisptr + UTF_BG_ALLOC_AXIS_ON_X)->size,num_nodes
            ,(axisptr + UTF_BG_ALLOC_AXIS_ON_Y)->size
            ,(axisptr + UTF_BG_ALLOC_AXIS_ON_Z)->size
        );
#endif
        (axisptr + UTF_BG_ALLOC_AXIS_ON_X)->size = num_nodes;
        (axisptr + UTF_BG_ALLOC_AXIS_ON_Y)->size = 0ULL;
        (axisptr + UTF_BG_ALLOC_AXIS_ON_Z)->size = 0ULL;
    }

    for (i=(size_t)UTF_BG_ALLOC_AXIS_IN_NODE;i<end_axis;i++) {
        (axisptr + i)->nodeinfo = (utf_bg_alloc_axis_node_info_t *)malloc(
            sizeof(utf_bg_alloc_axis_node_info_t) * (axisptr + i)->size);
        if (NULL == (axisptr + i)->nodeinfo) {
            i--;
            do {
                free((axisptr + i)->nodeinfo);
            } while (!i);
            free(intraptr);
            *intrainfo = NULL;
            FREEAREA_AXES();
            return UTF_ERR_OUT_OF_MEMORY;
        }
        ninfoptrs[i]=(axisptr + i)->nodeinfo;
    }

    for (i=0UL;i<procinfo_size;i++) {
        s = (size_t)procinfo[i].axis_kind;
        if ( hexa && ((size_t)UTF_BG_ALLOC_AXIS_OFF_AXES == s) ) { continue; }
        if ((size_t)UTF_BG_ALLOC_AXIS_MYSELF != s) {
            if ( !hexa && (s != (size_t)UTF_BG_ALLOC_AXIS_IN_NODE) ) {
                /* 6面体割り当てではないのでノード間は、一つの軸(X)に纏める。*/
                s = (size_t)UTF_BG_ALLOC_AXIS_ON_X;
            }
            set_axis_node_info(
                               rankset,
                               ninfoptrs[s],&procinfo[i]);
            ninfoptrs[s]++;
#if defined(DEBUGLOG)
            ninfoptrs_cnt[s]++;
            assert(ninfoptrs_cnt[s] <= (axisptr + s)->size);
#endif
        }
        else {
            for (s=(size_t)UTF_BG_ALLOC_AXIS_IN_NODE;s<end_axis;s++) {
                set_axis_node_info(
                                   rankset,
                                   ninfoptrs[s],&procinfo[i]);
                ninfoptrs[s]++;
#if defined(DEBUGLOG)
                ninfoptrs_cnt[s]++;
                assert(ninfoptrs_cnt[s] <= (axisptr + s)->size);
#endif
            }
        }
    }

    /*
     * ノード情報のソート
     * ノード間軸(X,Y,Z)のみをソートする。
     */
    for (i=(size_t)UTF_BG_ALLOC_AXIS_ON_X;i<end_axis;i++) {
        qsort((axisptr + i)->nodeinfo,
              (axisptr + i)->size,
              sizeof(utf_bg_alloc_axis_node_info_t),
              sort_nodeinfo_by_phy_6d_addr);
    }

    /* ソート済の軸毎の所属ノード情報から自プロセスの位置を見つける。*/
    for (i=(size_t)UTF_BG_ALLOC_AXIS_IN_NODE;i<end_axis;i++) {
        for ((axisptr + i)->nodeinfo_index=0UL;
             (axisptr + i)->nodeinfo[(axisptr + i)->nodeinfo_index].index !=
                 my_index;
             (axisptr + i)->nodeinfo_index++) {
            if ((axisptr + i)->size <= (axisptr + i)->nodeinfo_index) {
                /* internal error */
                free(intraptr);
                *intrainfo = NULL;
                FREEAREA_AXES();
                return UTF_ERR_INTERNAL;
            }
        }
    }

#if defined(DEBUGLOG)
    for (i=(size_t)UTF_BG_ALLOC_AXIS_IN_NODE;
         i<(size_t)UTF_BG_ALLOC_AXIS_NUM_AXES;i++) {
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu:AXISINFO %zu size=%zu nodeinfo=%p nodeinfo_index=%zu end_axis=%zu hexa=%d \n"
            ,my_hostname,__func__,my_index
            ,i,(axisptr + i)->size,(axisptr + i)->nodeinfo
            ,(axisptr + i)->nodeinfo_index,end_axis,hexa
        );
        if ((axisptr + i)->nodeinfo) {
            utf_bg_alloc_axis_node_info_t *temp;
            for (s=0UL,temp=(axisptr + i)->nodeinfo;
                 s<(axisptr + i)->size;s++,temp++) {
                fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu:AXISINFO %zu nodeinfo=%zu/%zu index=%zu log3daddr=%u-%u-%u phy6daddr=%02u-%02u-%02u-%1u-%1u-%1u \n"
                    ,my_hostname,__func__,my_index
                    ,i,s,(axisptr + i)->size,temp->index
                    ,temp->log_3d_addr.s.x
                    ,temp->log_3d_addr.s.y
                    ,temp->log_3d_addr.s.z
                    ,temp->phy_6d_addr.s.x
                    ,temp->phy_6d_addr.s.y
                    ,temp->phy_6d_addr.s.z
                    ,temp->phy_6d_addr.s.a
                    ,temp->phy_6d_addr.s.b
                    ,temp->phy_6d_addr.s.c
                );
            }
        }
    }
#endif

    FREEAREA_AXES();
    *intrainfo = intraptr;
    return UTF_SUCCESS;
}

/*
 * バタフライネットワークの構築
 */

/* Freeing temporary data buffers */
#define FREEAREA_BUTTERFLY()              \
{                                         \
  if (NULL != sender)  { free(sender); }  \
  if (NULL != reciver) { free(reciver); } \
}

/* Get peer process for my process on power of two */
#define GET_PAIR_OF_POWEROFTWO(index,on,gates,i,result)           \
{                                                                 \
    result = ( (index - on) ^  (1 << (gates - i - 1)) ) + on ;    \
}

/* バタフライネットワークシーケンスの2冪部分の送受信プロセスを求める。 */
#define SET_PAIR_POWEROFTWO(on_val)                               \
for (i=0;i<axisptr->number_gates_of_poweroftwo;i++) {             \
    GET_PAIR_OF_POWEROFTWO(axisptr->nodeinfo_index,               \
                           on_val,                                \
                           axisptr->number_gates_of_poweroftwo,   \
                           i,                                     \
                           *sendtable);                           \
    *recvtable = *sendtable;                                      \
    sendtable++;                                                  \
    recvtable++;                                                  \
}

/* EARのバタフライネットワークシーケンスの送受信不要部分の作成 */
#define SET_NOOP_FOR_EAR()                                        \
for (i=0;i<(axisptr->number_gates_of_poweroftwo + 1);i++) {       \
    *sendtable = UTF_BG_ALLOC_INVALID_INDEX;                      \
    *recvtable = UTF_BG_ALLOC_INVALID_INDEX;                      \
    sendtable++;                                                  \
    recvtable++;                                                  \
}

#if defined(DEBUGLOG)
#define UTF_BG_ALLOC_INVALID_6DADDR ((uint8_t)0xff)
/*
 * シーケンステーブルの内容を表示する。
 *
 * seqptr           (IN) シーケンステーブルへのポインタ
 * number_gates_max (IN) 最大ゲート数
 * print_opt        (IN) 最適化した状態での表示も行うか。
 */
static void print_sequence(utf_bf_alloc_butterfly_sequence_info_t *seqptr,
                           size_t number_gates_max,
                           bool   print_opt)
{
    size_t i;
    utf_bf_alloc_butterfly_sequence_info_t *temp = seqptr;
    char recvaddr[14];
    char sendaddr[14];

    fprintf(stderr,"***************************************** \n");
    for (i=0;i<number_gates_max;i++,seqptr++) {

        if (seqptr->recvinfo.phy_6d_addr) {
            sprintf(recvaddr,"%02u-%02u-%02u-%1u-%1u-%1u"
                ,seqptr->recvinfo.phy_6d_addr->s.x
                ,seqptr->recvinfo.phy_6d_addr->s.y
                ,seqptr->recvinfo.phy_6d_addr->s.z
                ,seqptr->recvinfo.phy_6d_addr->s.a
                ,seqptr->recvinfo.phy_6d_addr->s.b
                ,seqptr->recvinfo.phy_6d_addr->s.c
            );
        }
        else { strcpy(&recvaddr[0],"NULL"); }

        if (seqptr->sendinfo.phy_6d_addr) {
            sprintf(sendaddr,"%02u-%02u-%02u-%1u-%1u-%1u"
                ,seqptr->sendinfo.phy_6d_addr->s.x
                ,seqptr->sendinfo.phy_6d_addr->s.y
                ,seqptr->sendinfo.phy_6d_addr->s.z
                ,seqptr->sendinfo.phy_6d_addr->s.a
                ,seqptr->sendinfo.phy_6d_addr->s.b
                ,seqptr->sendinfo.phy_6d_addr->s.c
            );
        }
        else { strcpy(&sendaddr[0],"NULL"); }

        fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: [ALL]number_gates_max=%zu/%zu : seqptr=%p :vbg_id=%016lx : Recv vbg_id=%016lx nodeinfo_index=%ld index=%ld phy6daddr=%s : Send vbg_id=%016lx nodeinfo_index=%ld index=%ld phy6daddr=%s : nextptr=%p \n"
            ,my_hostname,__func__,my_rankset_index
            ,i,number_gates_max
            ,seqptr
            ,seqptr->vbg_id
            ,seqptr->recvinfo.vbg_id
            ,seqptr->recvinfo.nodeinfo_index
            ,seqptr->recvinfo.index
            ,&recvaddr[0]
            ,seqptr->sendinfo.vbg_id
            ,seqptr->sendinfo.nodeinfo_index
            ,seqptr->sendinfo.index
            ,&sendaddr[0]
            ,seqptr->nextptr
        );
    }

    if (!print_opt) { return; }

    seqptr = temp;
    i=0;
    fprintf(stderr,"***************************************** \n");
    while (seqptr) {

        if (seqptr->recvinfo.phy_6d_addr) {
            sprintf(recvaddr,"%02u-%02u-%02u-%1u-%1u-%1u"
                ,seqptr->recvinfo.phy_6d_addr->s.x
                ,seqptr->recvinfo.phy_6d_addr->s.y
                ,seqptr->recvinfo.phy_6d_addr->s.z
                ,seqptr->recvinfo.phy_6d_addr->s.a
                ,seqptr->recvinfo.phy_6d_addr->s.b
                ,seqptr->recvinfo.phy_6d_addr->s.c
            );
        }
        else { strcpy(&recvaddr[0],"NULL"); }

        if (seqptr->sendinfo.phy_6d_addr) {
            sprintf(sendaddr,"%02u-%02u-%02u-%1u-%1u-%1u"
                ,seqptr->sendinfo.phy_6d_addr->s.x
                ,seqptr->sendinfo.phy_6d_addr->s.y
                ,seqptr->sendinfo.phy_6d_addr->s.z
                ,seqptr->sendinfo.phy_6d_addr->s.a
                ,seqptr->sendinfo.phy_6d_addr->s.b
                ,seqptr->sendinfo.phy_6d_addr->s.c
            );
        }
        else { strcpy(&sendaddr[0],"NULL"); }

        fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: [OPT]number_gates_max=%zu/%zu : seqptr=%p :vbg_id=%016lx : Recv vbg_id=%016lx nodeinfo_index=%ld index=%ld phy6daddr=%s : Send vbg_id=%016lx nodeinfo_index=%ld index=%ld phy6daddr=%s : nextptr=%p \n"
            ,my_hostname,__func__,my_rankset_index
            ,i,number_gates_max
            ,seqptr
            ,seqptr->vbg_id
            ,seqptr->recvinfo.vbg_id
            ,seqptr->recvinfo.nodeinfo_index
            ,seqptr->recvinfo.index
            ,&recvaddr[0]
            ,seqptr->sendinfo.vbg_id
            ,seqptr->sendinfo.nodeinfo_index
            ,seqptr->sendinfo.index
            ,&sendaddr[0]
            ,seqptr->nextptr
        );
        seqptr = seqptr->nextptr;
        i++;
    }

    return;
}
#endif

/*
 * 自プロセスの該当ゾーンの識別
 *
 * nodeinfo_index (IN) 軸毎の情報テーブルのnodeinfo内の自プロセスのインデックス
 *  (IN)    axisptr   A pointer to the array of each of axes information.
 */
/*
 * バタフライネットワークを組んだ時に
 *  - 2冪に入らないものを"耳"                      UTF_BG_ALLOC_POWEROFTWO_EAR
 *  - 2冪に入り、かつ、耳とも送受信するものを"頬"  UTF_BG_ALLOC_POWEROFTWO_CHEEK
 *  - 2冪に入り、かつ、耳との送受信もないものを"顔"UTF_BG_ALLOC_POWEROFTWO_FACE
 *  と表現する。
 *
 *      +----------+
 *     +            +
 *     +  ^      ^  +
 *  +--|  e      e  |--+
 *  |  |    |  |    |  |
 *  +--|     ~~     |--+
 *  a  |      b     |  c
 *   E |C  F    F  C| E
 *     |            |
 *      +   ====   +
 *       +--------+
 *
 * E → UTF_BG_ALLOC_POWEROFTWO_EAR
 * C → UTF_BG_ALLOC_POWEROFTWO_CHEEK
 * F → UTF_BG_ALLOC_POWEROFTWO_FACE
 *
 */
static int get_my_zone(utf_bg_alloc_axis_detail_info_t *axisptr,
                       size_t nodeinfo_index)
{
    if ( (nodeinfo_index < axisptr->on_poweroftwo) ||
         (axisptr->off_poweroftwo <= nodeinfo_index) ) {
        return UTF_BG_ALLOC_POWEROFTWO_EAR;
    }
    if ( ((axisptr->on_poweroftwo * 2) <=  nodeinfo_index) &&
         (nodeinfo_index <
          (axisptr->size - ((axisptr->size - axisptr->off_poweroftwo)*2)))
       ) {
        return UTF_BG_ALLOC_POWEROFTWO_FACE;
    }
    return UTF_BG_ALLOC_POWEROFTWO_CHEEK;

}

/*
 *  シーケンステーブルの最適化を行う。
 *
 *  seqptr           (OUT)    シーケンステーブルへのポインタ
 *  number_gates_max (IN/OUT) 最大ゲート数
 */
static void optimized_sequence(utf_bf_alloc_butterfly_sequence_info_t **seqptr,
                               size_t *number_gates_max)
{
    utf_bf_alloc_butterfly_sequence_info_t *curr,*next;
    size_t i;

    if ( (UTF_BG_ALLOC_INVALID_INDEX == (*seqptr)->sendinfo.nodeinfo_index) &&
         (UTF_BG_ALLOC_INVALID_INDEX == (*seqptr)->recvinfo.nodeinfo_index) &&
         (UTF_BG_ALLOC_INVALID_INDEX == (*seqptr + 1)->recvinfo.nodeinfo_index) )
    {
        /*
         *  先頭の段に送受信がないため、
         *  シーケンステーブルの先頭の段を消せる。
         *  ただし、自分が、CHEEKの場合は、ローカルに繋ぐ始点ゲートの部分
         *  になるため、消せない。
         */
        (*seqptr)++;
        (*number_gates_max)--;
    }

    /*
     *
     * step1
     *   リストを作成する。シーケンステーブルのnextptrを設定する。
     *
     * max_gates_number = 8 (2x3x2)
     *
     *            recv         send
     *  -----------------------------------
     *  0          -1           1
     *  1           1          -1
     *  2          -1          -1
     *  3          -1           1
     *  4           1          -1
     *  5          -1          -1 ← ここまでがforでの最適化対象
     *  6          -1           1
     *  7           1          -1
     *
     *  for(i=0;i<number_gates_max - 2;i++)
     *    0->nextptr を 1にする。
     *    1->nextptr を 3にする。→2は送受信ともに-1(NOP)のため
     *    3->nextptr を 4にする。
     *    4->nextptr を 6にする。→5は送受信ともに-1(NOP)のため
     *
     * 7の送受信がともに-1であり、6の送信が-1の場合は、
     *  - 6でシーケンスは終わるため
     *    - 6->nextptr を NULLにする。
     * そうでない場合は、
     *  - 7までシーケンスは続くため、
     *     - 6->nextptr を 7にする。
     *     - 6->nextptr を NULLにする。
     *
     *            recv         send
     *  -----------------------------------
     *  0          -1           1 0/8 : seqptr=0x10bba80  nextptr=0x10bbad0
     *  1           1          -1 1/8 : seqptr=0x10bbad0  nextptr=0x10bbb70
     *  3          -1           1 3/8 : seqptr=0x10bbb70  nextptr=0x10bbbc0
     *  4           1          -1 4/8 : seqptr=0x10bbbc0  nextptr=0x10bbc60
     *  6          -1           1 6/8 : seqptr=0x10bbc60  nextptr=0x10bbcb0
     *  7           1          -1 7/8 : seqptr=0x10bbcb0  nextptr=(nil)
     *
     */

    curr = next = (*seqptr);
    next++;
    for (i=0;i<(*number_gates_max - 2);i++) {
        if ( (UTF_BG_ALLOC_INVALID_INDEX == next->sendinfo.nodeinfo_index) &&
             (UTF_BG_ALLOC_INVALID_INDEX == next->recvinfo.nodeinfo_index) ) {
            /* 送信も受信もないので、この段を読み飛ばす。 */
            next++;
            continue;
        }
        curr->nextptr = next;  /* 今の段のnextptrを更新 */
        curr          = next;
        next++;
    }

    /* 最後の2個分の段のチェック */
    if ( (UTF_BG_ALLOC_INVALID_INDEX == next->sendinfo.nodeinfo_index) &&
         (UTF_BG_ALLOC_INVALID_INDEX == next->recvinfo.nodeinfo_index) &&
         (UTF_BG_ALLOC_INVALID_INDEX == curr->sendinfo.nodeinfo_index) ) {
        /* next →最後の段 , curr →今の段 */
        curr->nextptr = NULL;  /* 今の段で終わらせる */
    }
    else {
        /* 最終段まで送受信が続く */
        curr->nextptr = next;
        next->nextptr = NULL;
    }

    /*
     *
     * step2
     *   送受信位置を最適化する。
     *
     *          recv       send    nextptr
     *    ----------------------------------
     *    0      1         -1        1
     *    1     -1          1        2
     *
     *    2段の送受信シーケンスが
     *    0 → recvが-1でない、sendが-1
     *    1 → recvが-1,       sendが-1でない
     *    場合、
     *      a. 0段のsendを1の段のsendに書き換える。→0と1を0に統合する。
     *      b. 0段のnextptrを1から1->nextptrに書き換える。
     *         →1の段をリストから消す。
     *
     *          recv       send    nextptr
     *    ----------------------------------
     *    0      1          1        2
     *
     */

    curr = (*seqptr);
    while (curr->nextptr) {
        next = curr->nextptr;
        if ( (curr->recvinfo.nodeinfo_index != UTF_BG_ALLOC_INVALID_INDEX) &&
             (curr->sendinfo.nodeinfo_index == UTF_BG_ALLOC_INVALID_INDEX) &&
             (next->recvinfo.nodeinfo_index == UTF_BG_ALLOC_INVALID_INDEX) &&
             (next->sendinfo.nodeinfo_index != UTF_BG_ALLOC_INVALID_INDEX) ) {
            curr->sendinfo.vbg_id         = next->sendinfo.vbg_id;
            curr->sendinfo.nodeinfo_index = next->sendinfo.nodeinfo_index;
            curr->sendinfo.index          = next->sendinfo.index;
            curr->sendinfo.phy_6d_addr    = next->sendinfo.phy_6d_addr;
            curr->nextptr = next->nextptr;
        }
        curr = next;
    }

    return;
}

/*
 * バタフライネットワークの送受信プロセスの情報を作成。
 *
 * srinfo         (OUT)  受信元情報または送信先情報へのポインタ
 * nodeinfo_index (IN) 軸毎の情報テーブルのnodeinfo内の自プロセスのインデックス
 *  (IN)    axisptr   A pointer to the array of each of axes information.
 */
static void initialize_butterfly_proc_info(
                                      utf_bf_alloc_butterfly_proc_info_t *srinfo,
                                      size_t nodeinfo_index,
                                      utf_bg_alloc_axis_detail_info_t *axisptr)
{
    int my_zone = get_my_zone(axisptr,axisptr->nodeinfo_index);

    srinfo->nodeinfo_index = nodeinfo_index;
    if (UTF_BG_ALLOC_INVALID_INDEX != srinfo->nodeinfo_index) {
        srinfo->vbg_id = UTF_BG_ALLOC_VBG_ID_UNDECIDED;
        srinfo->phy_6d_addr =
            &(axisptr->nodeinfo + srinfo->nodeinfo_index)->phy_6d_addr;
        srinfo->index =
            (axisptr->nodeinfo + srinfo->nodeinfo_index)->index;
    }
    else {
        if (UTF_BG_ALLOC_POWEROFTWO_EAR == my_zone) {
            srinfo->vbg_id = UTF_BG_ALLOC_VBG_ID_UNNECESSARY;
        }
        else {
            srinfo->vbg_id = UTF_BG_ALLOC_VBG_ID_NOOPARATION;
        }
        srinfo->phy_6d_addr = NULL;
        srinfo->index       = UTF_BG_ALLOC_INVALID_INDEX;
    }
    return;
}

/*
 * シーケンステーブルに初期値を設定する。
 *
 * seqptr  (OUT) シーケンステーブルへのポインタ
 *  (IN)    axisptr   A pointer to the array of each of axes information.
 * sendtable (IN) 送信先プロセス配列
 * recvtable (IN) 受信元プロセス配列
 * tail_axis (IN) バタフライネットワークの末尾となる軸
 *
 */
static int initialize_sequence(utf_bf_alloc_butterfly_sequence_info_t **seqptr,
                               utf_bg_alloc_axis_detail_info_t *axisptr,
                               size_t *sendtable,
                               size_t *recvtable,
                               bool   tail_axis)
{
    size_t i;

    /*
     *
     *   recvinfo   sendinfo
     *  0   A          B
     *  1   B          C
     *  2   C          D
     *  3   D          E
     *
     *  number_gates_max = 4
     *
     *  1. Initialize A.
     *  2. for(i=0;i<axisptr->number_gates;i++)ループでB,C,Dの初期化を行う。
     *  3. for(i=0;i<axisptr->number_gates;i++)ループ終了後、Eの初期化を行う。
     *
     */

    (*seqptr)->recvinfo.vbg_id         = UTF_BG_ALLOC_VBG_ID_NOOPARATION;
    (*seqptr)->recvinfo.nodeinfo_index = UTF_BG_ALLOC_INVALID_INDEX;
    (*seqptr)->recvinfo.index          = UTF_BG_ALLOC_INVALID_INDEX;
    (*seqptr)->recvinfo.phy_6d_addr    = NULL;
    (*seqptr)->nextptr                 = NULL;

    for (i=0;i<axisptr->number_gates;i++) {
        /* Initialize send side infomation. */
        initialize_butterfly_proc_info(&((*seqptr)->sendinfo),*sendtable,axisptr);
        /* Initialize recvice side infomation. */
        initialize_butterfly_proc_info(&((*seqptr + 1)->recvinfo),*recvtable,axisptr); 
        (*seqptr)->vbg_id = UTF_BG_ALLOC_VBG_ID_UNDECIDED;
        sendtable++;
        recvtable++;
        (*seqptr)++;
    }

    (*seqptr)->sendinfo.vbg_id         = UTOFU_VBG_ID_NULL;
    (*seqptr)->sendinfo.nodeinfo_index = UTF_BG_ALLOC_INVALID_INDEX;
    (*seqptr)->sendinfo.index          = UTF_BG_ALLOC_INVALID_INDEX;
    (*seqptr)->sendinfo.phy_6d_addr    = NULL;
    (*seqptr)->vbg_id = UTF_BG_ALLOC_VBG_ID_UNDECIDED;

    (*seqptr)++;
    return UTF_SUCCESS;
}

/*
 * バタフライネットワークシーケンス用の送受信プロセステーブルの作成
 *
 * sendtable (OUT) 送信先プロセスを記録する配列
 * recvtable (OUT) 受信元プロセスを記録する配列
 *
 */
static int make_sequence(utf_bg_alloc_axis_detail_info_t *axisptr,
                         size_t *sendtable,
                         size_t *recvtable)
{
#if defined(DEBUGLOG)
    size_t *sendtable_temp = sendtable;
    size_t *recvtable_temp = recvtable;
    char *tempa,*tempb;
#endif
    size_t i;

    if (!(axisptr->flag_poweroftwo)) {
        /* 軸のノード数は、2冪ではない */
        if (axisptr->nodeinfo_index < axisptr->on_poweroftwo) {
            /* EAR(left) */
            *sendtable = axisptr->nodeinfo_index + axisptr->on_poweroftwo;
            sendtable++;
            SET_NOOP_FOR_EAR();
            *recvtable = axisptr->nodeinfo_index + axisptr->on_poweroftwo;
        }
        else if (axisptr->nodeinfo_index < (axisptr->on_poweroftwo * 2)) {
            /* CHHEK(left) */
            *sendtable = UTF_BG_ALLOC_INVALID_INDEX;
            *recvtable = axisptr->nodeinfo_index - axisptr->on_poweroftwo;
            sendtable++;
            recvtable++;
            SET_PAIR_POWEROFTWO(axisptr->on_poweroftwo);
            *sendtable = axisptr->nodeinfo_index - axisptr->on_poweroftwo;
            *recvtable = UTF_BG_ALLOC_INVALID_INDEX;
        }
        else if (axisptr->nodeinfo_index <
                 ((axisptr->off_poweroftwo * 2) - axisptr->size)) {
            /* FACE */
            *sendtable = UTF_BG_ALLOC_INVALID_INDEX;
            *recvtable = UTF_BG_ALLOC_INVALID_INDEX;
            sendtable++;
            recvtable++;
            SET_PAIR_POWEROFTWO(axisptr->on_poweroftwo);
            *sendtable = UTF_BG_ALLOC_INVALID_INDEX;
            *recvtable = UTF_BG_ALLOC_INVALID_INDEX;
        }
        else if (axisptr->nodeinfo_index < axisptr->off_poweroftwo) {
            /* CHEEK(right) */
            *sendtable = UTF_BG_ALLOC_INVALID_INDEX;
            *recvtable = axisptr->nodeinfo_index +
                (axisptr->size - axisptr->off_poweroftwo);
            sendtable++;
            recvtable++;
            SET_PAIR_POWEROFTWO(axisptr->on_poweroftwo);
            *sendtable = axisptr->nodeinfo_index +
                (axisptr->size - axisptr->off_poweroftwo);
            *recvtable = UTF_BG_ALLOC_INVALID_INDEX;
        }
        else if (axisptr->nodeinfo_index < axisptr->size) {
            /* EAR(right) */
            *sendtable = axisptr->nodeinfo_index -
                (axisptr->size - axisptr->off_poweroftwo);
            sendtable++;
            SET_NOOP_FOR_EAR();
            *recvtable = axisptr->nodeinfo_index -
                (axisptr->size - axisptr->off_poweroftwo);
        }
        else {
            assert(0);
            return UTF_ERR_INTERNAL;
        }
    }
    else {
        /* 軸のノード数は、2冪 */
        assert(axisptr->number_gates_of_poweroftwo == axisptr->number_gates);
        SET_PAIR_POWEROFTWO(0);
    }

#if defined(DEBUGLOG)
    tempa = (char *)malloc(
        (size_t)8 * axisptr->number_gates + 1);
    if (NULL == tempa) {
        return UTF_ERR_OUT_OF_MEMORY;
    }
    tempb = tempa;

    for (i=0;i<axisptr->number_gates;i++){
        sprintf(tempb," %7d",(int)(*recvtable_temp));
        recvtable_temp++;
        tempb += 8;
    }
    *tempb = '\0';
    fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: nodeinfo_index=%zu Recvtable=%s \n"
        ,my_hostname,__func__,my_rankset_index
        ,axisptr->nodeinfo_index,tempa
    );

    tempb = tempa;
    for (i=0;i<axisptr->number_gates;i++){
        sprintf(tempb," %7d",(int)(*sendtable_temp));
        sendtable_temp++;
        tempb += 8;
    }
    *tempb = '\0';
    fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: nodeinfo_index=%zu Sendtable=%s \n"
        ,my_hostname,__func__,my_rankset_index
        ,axisptr->nodeinfo_index,tempa
    );
    free(tempa);
#endif

    return UTF_SUCCESS;
}

/*
 * バタフライネットワークを作成し、シーケンステーブルへのポインタと
 * 確保すべき中継ゲート数を返却する。
 *
 *  (IN)    axisptr   A pointer to the array of each of axes information.
 * start_axis (IN)         バタフライネットワークの先頭となる軸
 * sequence   (OUT)        シーケンステーブル領域の先頭アドレス(領域解放用)
 * optimizedsequence (OUT) 最適化されたシーケンステーブルの先頭部分を指す
 * number_vbgs (OUT)       確保すべきVBGの数
 *
 */
static int make_butterfly_network(utf_bg_alloc_axis_detail_info_t *axisptr,
                                  int start_axis,
                                  utf_bf_alloc_butterfly_sequence_info_t **sequence,
                                  utf_bf_alloc_butterfly_sequence_info_t **optimizedsequence,
                                  size_t *number_vbgs
)
{
    int rc,i,tail_axis=start_axis,curr_zone,prev_zone;
#if defined(DEBUGLOG)
    int head_axis=start_axis;
#endif
    size_t s;
    size_t number_gates_max=0UL;
    size_t number_relay_gates_allaxis=0UL; /* 半欠けの数 */
    utf_bf_alloc_butterfly_sequence_info_t *sequenceptr,*sequence_head;
    size_t *sender,*reciver;

    /*
     * バタフライネットワークを構築するのに必要な情報を作成する。
     */

    for (i=start_axis;i<UTF_BG_ALLOC_AXIS_NUM_AXES;i++) {
        if ((axisptr + i)->size <= 1) {
            /*
             * 軸上には自ノードしかいない。
             */
            continue;
        }

        /* バタフライネットワークに必要なゲート数 */
        /* 軸の開始点+軸の終了点でnumber_gates1個、中継はnumber_gates1個ずつ。*/
        /* 4ノード |) - (||) - (| → 2個 */
        for ((axisptr + i)->number_gates=0UL,s=(axisptr + i)->size;
             1 < s;
             s >>= 1,(axisptr + i)->number_gates++);
        (axisptr + i)->number_gates_of_poweroftwo =
            (axisptr + i)->number_gates; /* バタフライネットワーク構築で利用 */

        /*
         * 中継ゲートの数
         * number_relay_gatesに入る値はi必要数の2倍の数→ 半欠けの数
         */
        (axisptr + i)->number_relay_gates[UTF_BG_ALLOC_POWEROFTWO_EAR] =
            (size_t)1 * 2;
        (axisptr + i)->number_relay_gates[UTF_BG_ALLOC_POWEROFTWO_FACE] =
            (axisptr + i)->number_gates * 2;
        (axisptr + i)->number_relay_gates[UTF_BG_ALLOC_POWEROFTWO_CHEEK] =
            ((axisptr + i)->number_gates + 1) * 2;

        /* sizeとsize-1をビットANDして、先頭ビットが立っている場合は、2冪ではない。*/
        if ( (axisptr + i)->size & ((axisptr + i)->size - 1) ) {
            size_t w;
            /* int k; */
            /*
             * 軸内のノード数は２冪ではない。
             * 例 size=12
             *   1100(12)      (axisptr + i)->size
             * & 1011(12-1=11) (axisptr + i)->size - 1
             * ------
             *   1000          先頭ビットが立つので2冪ではない
             */
            /*
             * 2冪でないノード数からバタフライを行う場所を計算する。
             * 例 size=12
             *
             * A. 2冪レベルでのノード数を計算
             *     1 << number_gates_of_poweroftwo
             *     → 1 << 3 = 8 (1000)
             *
             * B. バタフライ開始位置を計算する。
             *    (size - A.の値) / 2
             *    → (12 - 8) / 2 = 2
             *       (12 - 8)がバタフライに参加しないプロセス数
             *       2で割っているのは、参加しないノードは
             *       バタフライの両端に作るため
             *
             * C. バタフライを超えた位置を計算する。
             *    B.の値(バタフライ先頭位置) + A.の値
             *    2 + 8 = 10
             *    10がバタフライを超えた位置、-1した9までがバタフライを行う
             *
             *   0 1 2 3 4 5 6 7 8 9 10 11
             *       B               C
             *       <2^n-butterfly>
             */
            w = (1 << (axisptr + i)->number_gates_of_poweroftwo);
            /* for (k=0,w=1;k<=(axisptr + i)->number_gates;k++,w*=2); */
            (axisptr + i)->on_poweroftwo = ((axisptr + i)->size - w) / 2;
            (axisptr + i)->off_poweroftwo = (axisptr + i)->on_poweroftwo + w;
            /*
             * number_gatesは、後に作成するシーケンステーブルは、
             * 最大の値になるCHEEKの条件で作成するため、
             * バタフライ前後にEARからの送受信部分を加算(+2)した値とする。
             *
             *    例 size=12のバタフライシーケンス
             *       2冪レベルでのnumber_gates(number_gates_of_poweroftwo) == 3
             *       3に+2する。
*              <---------- (axisptr+x)->number_gates ---------------------->
*                         0           1           2          3            4
*              +-----------+-----------+-----------+-----------+-----------+
*  EAR  -send  |send       |Nooparation|Nooparation|Nooparation|Nooparation|
*              +-----------------------------------------------------------+
*  EAR  -recv  |Nooparation|Nooparation|Nooparation|Nooparation|recv       |
*              +-----------------------------------------------------------+
*  CHEEK-send  |Nooparation|butterfly  |butterfly  |butterfly  |send       |
*              +-----------------------------------------------------------+
*  CHEEK-recv  |recv       |butterfly  |butterfly  |butterfly  |Nooparation|
*              +-----------------------------------------------------------+
*  FACE -send  |Nooparation|butterfly  |butterfly  |butterfly  |Nooparation|
*              +-----------------------------------------------------------+
*  FACE -recv  |Nooparation|butterfly  |butterfly  |butterfly  |Nooparation|
*              +-----------------------------------------------------------+
             */
            (axisptr + i)->number_gates += 2;
            (axisptr + i)->flag_poweroftwo = false;
        }
        else {
            /*
             * 軸内のノード数は２冪
             * 例 size=8
             *   1000(8)     (axisptr + i)->size
             * & 0111(8-1=7) (axisptr + i)->size - 1
             * ------
             *   0000     すべてのビットがOFFなので2冪
             */
            (axisptr + i)->on_poweroftwo = 0;
            (axisptr + i)->off_poweroftwo = (axisptr + i)->size;
            (axisptr + i)->flag_poweroftwo = true;
        }

        if (!number_gates_max) {
            /*
             * 最初に見つかった２ノード以上存在する軸
             * なので、始点ゲート分を減らす
             * FACEまたはEARのケースのみ。 |)だから消せる。
             */
            (axisptr + i)->number_relay_gates[UTF_BG_ALLOC_POWEROFTWO_EAR]--;
            (axisptr + i)->number_relay_gates[UTF_BG_ALLOC_POWEROFTWO_FACE]--;
#if defined(DEBUGLOG)
            head_axis = i;
#endif
        }
        number_gates_max += ((axisptr + i)->number_gates + 1);
        tail_axis = i;
    }

    /*
     * 最後に見つかった２ノード以上存在する軸
     * なので、終点ゲート分を減らす
     * FACEまたはEARのケースのみ。 (|だから消せる。
     */
    (axisptr + tail_axis)->number_relay_gates[UTF_BG_ALLOC_POWEROFTWO_EAR]--;
    (axisptr + tail_axis)->number_relay_gates[UTF_BG_ALLOC_POWEROFTWO_FACE]--;

#if defined(DEBUGLOG)
    fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu:AXISINFO number_gates_max=%zu head_axis=%d tail_axis=%d \n"
        ,my_hostname,__func__,my_rankset_index
        ,number_gates_max,head_axis,tail_axis
    );
#endif

    prev_zone=(-1);
    for (i=start_axis;i<UTF_BG_ALLOC_AXIS_NUM_AXES;i++) {
        if ((axisptr + i)->size <= 1)  { continue; }

        curr_zone = get_my_zone((axisptr + i),(axisptr + i)->nodeinfo_index);
#if defined(DEBUGLOG)
    fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu:AXISINFO i=%d zone=%d \n"
        ,my_hostname,__func__,my_rankset_index
        ,i,curr_zone
    );
#endif

        switch (prev_zone) {
            case UTF_BG_ALLOC_POWEROFTWO_EAR:
            case UTF_BG_ALLOC_POWEROFTWO_FACE:
                if (UTF_BG_ALLOC_POWEROFTWO_CHEEK == curr_zone) {
                    /*
                     * prev   (|
                     *   NEED  |)
                     * curr      (|
                     */
                    number_relay_gates_allaxis++;
                }
                else {
                    /*
                     * prev   (|
                     * curr      |)
                     */
                }
                break;
            case UTF_BG_ALLOC_POWEROFTWO_CHEEK:
                if ( (UTF_BG_ALLOC_POWEROFTWO_EAR == curr_zone) ||
                     (UTF_BG_ALLOC_POWEROFTWO_FACE  == curr_zone) ) {
                    /*
                     * prev   |)
                     *   NEED   (|
                     * curr      |)
                     */
                    number_relay_gates_allaxis++;
                }
                else {
                    /*
                     * prev   |)
                     * curr      (|
                     */
                }
                break;
            default:
                /* this is the first axis */
                break;
        }

        number_relay_gates_allaxis +=
            (axisptr + i)->number_relay_gates[curr_zone];
        prev_zone = curr_zone;
    }

#if defined(DEBUGLOG)
    fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu:AXISINFO number_relay_gates_allaxis=%zu \n"
        ,my_hostname,__func__,my_rankset_index
        ,number_relay_gates_allaxis
    );
    for (i=UTF_BG_ALLOC_AXIS_IN_NODE;i<UTF_BG_ALLOC_AXIS_NUM_AXES;i++) {
        fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu:AXISINFO %d size=%zu nodeinfo=%p number_gates=%zu relay_gates=%zu/%zu/%zu on_2^n=%zu off_2^n=%zu number_gates2^=%zu flag2^=%d \n"
            ,my_hostname,__func__,my_rankset_index
            ,i,(axisptr + i)->size,(axisptr + i)->nodeinfo
            ,(axisptr + i)->number_gates
            ,(axisptr + i)->number_relay_gates[UTF_BG_ALLOC_POWEROFTWO_EAR]
            ,(axisptr + i)->number_relay_gates[UTF_BG_ALLOC_POWEROFTWO_CHEEK]
            ,(axisptr + i)->number_relay_gates[UTF_BG_ALLOC_POWEROFTWO_FACE]
            ,(axisptr + i)->on_poweroftwo,(axisptr + i)->off_poweroftwo
            ,(axisptr + i)->number_gates_of_poweroftwo
            ,(axisptr + i)->flag_poweroftwo
        );
    }
#endif

    /* バタフライネットワーク作成 */
    sequence_head =
    sequenceptr = (utf_bf_alloc_butterfly_sequence_info_t *)malloc(
        sizeof(utf_bf_alloc_butterfly_sequence_info_t) * number_gates_max);
    if (NULL == sequenceptr) {
        *sequence = NULL;
        *optimizedsequence = NULL;
        *number_vbgs = 0UL;
        return UTF_ERR_OUT_OF_MEMORY;
    }
    memset((void *)sequenceptr,0,
           sizeof(utf_bf_alloc_butterfly_sequence_info_t) * number_gates_max);

    for (i=start_axis;i<UTF_BG_ALLOC_AXIS_NUM_AXES;i++) {
        if ((axisptr + i)->size <= 1)  { continue; }

        sender  = (size_t *)malloc(
            sizeof(size_t) * (axisptr + i)->number_gates);
        reciver = (size_t *)malloc(
            sizeof(size_t) * (axisptr + i)->number_gates);
        if ( (NULL == sender) || (NULL == reciver) ) {
            FREEAREA_BUTTERFLY();
            free(sequenceptr);
            *sequence = NULL;
            *optimizedsequence = NULL;
            *number_vbgs = 0UL;
            return UTF_ERR_OUT_OF_MEMORY;
        }

#if defined(DEBUGLOG)
    fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu:AXISINFO %d make_sequence \n"
        ,my_hostname,__func__,my_rankset_index
        ,i
    );
#endif
        rc = make_sequence((axisptr + i),sender,reciver);
        if (rc != UTF_SUCCESS) {
            FREEAREA_BUTTERFLY();
            free(sequenceptr);
            *sequence = NULL;
            *optimizedsequence = NULL;
            *number_vbgs = 0UL;
            return rc;
        }

#if defined(DEBUGLOG)
    fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu:AXISINFO %d initialize_sequence ptr=%p \n"
        ,my_hostname,__func__,my_rankset_index
        ,i,sequenceptr
    );
#endif
        initialize_sequence(&sequenceptr,
                            (axisptr + i),sender,reciver,(i == tail_axis));

        FREEAREA_BUTTERFLY();
    }

#if defined(DEBUGLOG)
    print_sequence(sequence_head,number_gates_max,false);
#endif

    sequenceptr=sequence_head;

    optimized_sequence(&sequenceptr,&number_gates_max);

#if defined(DEBUGLOG)
    print_sequence(sequenceptr,number_gates_max,true);
#endif

    *sequence = sequence_head;        /* 領域解放用先頭アドレス */
    *optimizedsequence = sequenceptr; /* 最適化された領域の先頭を指す */

    /*
     * utofu_alloc_vbgに使用するVBGの数を計算する。
     */
    for (*number_vbgs=0UL;
         sequenceptr->nextptr!=NULL;
         sequenceptr=sequenceptr->nextptr,(*number_vbgs)++);


    return UTF_SUCCESS;
}

/*
 * mmap領域の確保
 *
 * intra_node_info (IN) ノード内プロセス情報
 */
int make_mmap_file(
                   uint32_t vpid,
                   utf_bg_intra_node_detail_info_t *intra_node_info)
{
    char filename_temp[64];
    char *dirname=".";
    size_t namelen=0UL;

    /*
     * 最大ノード内プロセス数 == MPI_COMM_WORLD(utf_bg_first_call == true)
     * のノード内プロセス数を代入
     */
    utf_bg_mmap_file_info.width = intra_node_info->size;

    /* mmapファイルのサイズを計算 */
    if (utf_bg_intra_node_barrier_is_tofu) {
        utf_bg_mmap_file_info.size = UTF_BG_MMAP_HEADER_SIZE;
    }
    else {
        utf_bg_mmap_file_info.size =
            /* 先頭のTNI,BGID情報 */
            UTF_BG_MMAP_HEADER_SIZE
            +
            /* BUFFER + SEQ */
            UTF_BG_MMAP_TNI_INF_SIZE * UTF_BG_NUM_TNI;
    }

    if (!utf_bg_mmap_file_info.disable_utf_shm) {
#if defined(TOFUGIJI)
        if (UTF_BG_INTRA_NODE_MANAGER == intra_node_info->state) {
#endif
#if defined(DEBUGLOG)
    fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu call utf_shm_init \n"
        ,my_hostname,__func__,my_rankset_index
    );
#endif
            if (0 != utf_shm_init(utf_bg_mmap_file_info.size,
                                  (void **)(&utf_bg_mmap_file_info.mmaparea))) {
                return UTF_ERR_INTERNAL;
            }
#if defined(TOFUGIJI)
        }
#endif
        return UTF_SUCCESS;
    }

    /* mmapファイルを作成するディレクトリ名を求める */
    namelen += strlen(dirname);

    /* mmapファイル名の生成 */
    sprintf(filename_temp,"/utf_bg_mmap_file-%08x-%08x-%08x-%08x"
        ,my_jobid
        ,utf_info.vname[vpid].xyz.s.x
        ,utf_info.vname[vpid].xyz.s.y
        ,utf_info.vname[vpid].xyz.s.z
    );
    namelen += strlen(&filename_temp[0]);

    utf_bg_mmap_file_info.file_name = (char *)malloc(namelen);
    if (NULL == utf_bg_mmap_file_info.file_name) {
        return UTF_ERR_OUT_OF_MEMORY;
    }
    memset((void *)utf_bg_mmap_file_info.file_name,0,namelen);

    strcat(utf_bg_mmap_file_info.file_name,dirname);
    strcat(utf_bg_mmap_file_info.file_name,filename_temp);

    if (UTF_BG_INTRA_NODE_MANAGER == intra_node_info->state) {
        void *mmaparea=NULL;
        ssize_t wrc;
        /* ファイルの作成 */
        utf_bg_mmap_file_info.fd = open(utf_bg_mmap_file_info.file_name,
                                        O_RDWR|O_CREAT, 0666);
        if (-1 == utf_bg_mmap_file_info.fd) {
#if defined(DEBUGLOG)
            fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: open=%d errno=%d \n"
                ,my_hostname,__func__,my_rankset_index
                ,utf_bg_mmap_file_info.fd,errno
            );
#else
            utf_printf("[OPEN-ERR] fd=%d errno=%d \n",utf_bg_mmap_file_info.fd,errno);
#endif
            return UTF_ERR_INTERNAL;
        }
        mmaparea = (void *)malloc(utf_bg_mmap_file_info.size);
        if (NULL == mmaparea) {
            return UTF_ERR_OUT_OF_MEMORY;
        }
        memset(mmaparea,0,utf_bg_mmap_file_info.size);
        wrc = write(utf_bg_mmap_file_info.fd,
                    (const void *)mmaparea,
                    utf_bg_mmap_file_info.size);
        free(mmaparea);
        if ( ((ssize_t)-1 == wrc) ||
             (wrc < (ssize_t)utf_bg_mmap_file_info.size) ) {
#if defined(DEBUGLOG)
            fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: write=%ld errno=%d \n"
                ,my_hostname,__func__,my_rankset_index
                ,wrc,errno
            );
#else
            utf_printf("[WRITE-ERR] wrc=%ld errno=%d \n",wrc,errno);
#endif
            return UTF_ERR_INTERNAL;
        }

        /* mmap */
        mmaparea=NULL;
        mmaparea = mmap(0,
                        utf_bg_mmap_file_info.size,
                        PROT_READ | PROT_WRITE, MAP_SHARED,
                        utf_bg_mmap_file_info.fd,
                        0);
        if (MAP_FAILED == mmaparea) {
#if defined(DEBUGLOG)
            fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: MAP_FAILED errno=%d \n"
                ,my_hostname,__func__,my_rankset_index
                ,errno
            );
#else
            utf_printf("[MMAP-ERR] errno=%d \n",errno);
#endif
            return UTF_ERR_OUT_OF_MEMORY;
        }
        utf_bg_mmap_file_info.mmaparea = (utf_bg_mmap_header_info_t *)mmaparea;
    }

    return UTF_SUCCESS;
}

/*
 * ゲートの確保
 */

/*
 * utofuインターフェースを使用し、VBGを確保する。
 *
 * start_tni   (IN)  VBGの確保を試みる最初のTNI
 * num_vbg     (IN)  VBG数(始点+中継)
 * vbg_ids     (OUT) VBGIDを格納する配列へのポインタ
 * my_tni      (OUT) VBGの確保に成功したTNI
 *
 */
static int alloc_vbg(utofu_tni_id_t my_tni,
                     size_t num_vbg,
                     utofu_vbg_id_t *vbg_ids)
{
    int rc;
    UTOFU_CALL_RC(rc,utofu_alloc_vbg,my_tni,num_vbg,utf_bg_alloc_flags,vbg_ids)
}

static int allocate_vbg_core(utofu_tni_id_t start_tni,
                             size_t num_vbg,
                             utofu_vbg_id_t *vbg_ids,
                             utofu_tni_id_t *my_tni)
{
    int rc = UTF_SUCCESS;
#if defined(DEBUGLOG)
    size_t i;
#endif

    *my_tni = start_tni;

    do {
        rc = alloc_vbg(*my_tni,num_vbg,vbg_ids);
        UTOFU_ERRCHECK_EXIT_IF(0,rc)
        if (UTOFU_ERR_FULL != rc) {
            /* UTOFU_SUCCESSまたは、致命的なエラー */
            break;
        }

        /*
         * UTOFU_ERR_FULLが返却された場合、utofuライブラリ内は実際には
         * 1つのVBGも確保していないので、ここでの解放処理は必要ない。
         */

        *my_tni = (*my_tni + 1) % UTF_BG_NUM_TNI; 
    } while(*my_tni != start_tni);

#if defined(DEBUGLOG)
    for (i=0;i<num_vbg;i++) {
        fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: utofu_alloc_vbg=%d my_tni=%d/%d num_vbg=%zu/%zu vbg_ids=%016lx \n"
            ,my_hostname,__func__,my_rankset_index
            ,rc,*my_tni,start_tni,i,num_vbg,vbg_ids[i]
        );
    }
#endif
    return rc;
}

/*
 * dst remoteの情報の作成
 * rmt_src_dst_seqsのdst(dst == send)を設定
 */
#define SET_DST_REMOTE(tempseq,i) \
{ \
    if (UTF_BG_ALLOC_INVALID_INDEX != tempseq->sendinfo.nodeinfo_index) { \
        utf_bg_detail_info->rmt_src_dst_seqs[i].dst_rmt_index = \
            tempseq->sendinfo.index; \
    } \
    else { \
        utf_bg_detail_info->rmt_src_dst_seqs[i].dst_rmt_index = \
            UTF_BG_BGINFO_INVALID_INDEX; \
    } \
}

/*
 * src remoteの情報の作成
 * rmt_src_dst_seqsのsrc(src == recv)を設定
 */
#define SET_SRC_REMOTE(tempseq,i) \
{ \
    if (UTF_BG_ALLOC_INVALID_INDEX != tempseq->recvinfo.nodeinfo_index) { \
        utf_bg_detail_info->rmt_src_dst_seqs[i].src_rmt_index = \
            tempseq->recvinfo.index; \
    } \
    else { \
        utf_bg_detail_info->rmt_src_dst_seqs[i].src_rmt_index = \
            UTF_BG_BGINFO_INVALID_INDEX; \
    } \
}

/* VBGからBGIDを抜粋 */
#define GET_BGID_FROM_VBGID(vbgid,id)                                  \
{                                                                      \
    id = (utofu_bg_id_t)(vbgid & UTF_BG_ALLOC_MSK_BGID);               \
}

/*
 * VBGの確保
 *
 * number_vbgs     (IN) 必要なVBGの数
 * intra_node_info (IN) ノード内プロセス情報
 * sequenceptr     (IN) シーケンステーブルへのポインタ
 */
static int allocate_vbg(size_t number_vbgs,
                        utf_bg_intra_node_detail_info_t *intra_node_info,
                        utf_bf_alloc_butterfly_sequence_info_t *sequenceptr,
                        utofu_tni_id_t *my_tni)
{
    int rc = UTF_SUCCESS;
    size_t i;
    utf_bf_alloc_butterfly_sequence_info_t *tempseq;
    utofu_tni_id_t start_tni;

    /* 確保ゲート数の決定 */
    utf_bg_detail_info->num_vbg = number_vbgs;

    /* 領域確保 */
    utf_bg_detail_info->vbg_ids = (utofu_vbg_id_t *)malloc
        (sizeof(utofu_vbg_id_t) * utf_bg_detail_info->num_vbg);
    if (NULL == utf_bg_detail_info->vbg_ids) {
        return UTF_ERR_OUT_OF_MEMORY;
    }
    memset((void *)utf_bg_detail_info->vbg_ids,0,
         sizeof(utofu_vbg_id_t) * utf_bg_detail_info->num_vbg);

    utf_bg_detail_info->vbg_settings = (struct utofu_vbg_setting *)malloc
        (sizeof(struct utofu_vbg_setting) * utf_bg_detail_info->num_vbg);
    if (NULL == utf_bg_detail_info->vbg_settings) {
        free(utf_bg_detail_info->vbg_ids);
        return UTF_ERR_OUT_OF_MEMORY;
    }
    memset((void *)utf_bg_detail_info->vbg_settings,0,
         sizeof(struct utofu_vbg_setting) * utf_bg_detail_info->num_vbg);

    utf_bg_detail_info->rmt_src_dst_seqs = (utf_rmt_src_dst_index_t *)malloc
        (sizeof(utf_rmt_src_dst_index_t) * utf_bg_detail_info->num_vbg);
    if (NULL == utf_bg_detail_info->rmt_src_dst_seqs) {
        free(utf_bg_detail_info->vbg_ids);
        free(utf_bg_detail_info->vbg_settings);
        return UTF_ERR_OUT_OF_MEMORY;
    }
    memset((void *)utf_bg_detail_info->rmt_src_dst_seqs,0,
         sizeof(utf_rmt_src_dst_index_t) * utf_bg_detail_info->num_vbg);

    /* TNIの決定 */
    if (utf_bg_first_call) {
        start_tni = 0;
    }
    else {
        /* mmap領域から先頭を読み込む */
        assert(utf_bg_mmap_file_info.mmaparea);
        start_tni = utf_bg_mmap_file_info.mmaparea->next_tni_id;
#if defined(DEBUGLOG)
        fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: mmap: read next_tni_id=%u \n"
            ,my_hostname,__func__,my_rankset_index
            ,utf_bg_mmap_file_info.mmaparea->next_tni_id
        );
#endif
    }
    /* 次のTNI番号を計算 */
    utf_bg_next_tni_id = ( start_tni + ((utf_bg_intra_node_barrier_is_tofu) ? intra_node_info->size : 1) ) % UTF_BG_NUM_TNI;
    start_tni = (start_tni + intra_node_info->intra_index) % UTF_BG_NUM_TNI;

    rc = allocate_vbg_core(start_tni,
                           utf_bg_detail_info->num_vbg,
                           utf_bg_detail_info->vbg_ids,
                           my_tni);
    if (UTF_SUCCESS != rc) {
        /* UTF_SUCCESS以外ならば復帰してしまう。 */
        free(utf_bg_detail_info->rmt_src_dst_seqs);
        utf_bg_detail_info->rmt_src_dst_seqs = NULL;
        free(utf_bg_detail_info->vbg_settings);
        utf_bg_detail_info->vbg_settings = NULL;
        free(utf_bg_detail_info->vbg_ids);
        utf_bg_detail_info->vbg_ids=NULL;
        utf_bg_detail_info->num_vbg = 0;
        return rc;
    }

    /*
     * ゲート確保できた場合、rmt_src_dst_seqsを作成し、復帰
     */

    tempseq = sequenceptr;
    i = 0UL;
    while (tempseq) {
        if (tempseq == sequenceptr) {
            /* 始点ゲート位置 */
            tempseq->vbg_id = utf_bg_detail_info->vbg_ids[0];
            SET_DST_REMOTE(tempseq,0);
        }
        else if (NULL == tempseq->nextptr) {
            /* 始点ゲート終点位置 */
            tempseq->vbg_id = utf_bg_detail_info->vbg_ids[0];
            SET_SRC_REMOTE(tempseq,0);
        }
        else {
            /* 中継ゲート位置 */
            tempseq->vbg_id = utf_bg_detail_info->vbg_ids[i];
            SET_SRC_REMOTE(tempseq,i);
            SET_DST_REMOTE(tempseq,i);
        }
        tempseq = tempseq->nextptr;
        i++;
    }
    assert((i - 1)==utf_bg_detail_info->num_vbg);

    if (utf_bg_first_call) {
        /* MPI_COMM_WORLDの始点ゲート */
        utf_bg_mmap_file_info.first_vbg_id = utf_bg_detail_info->vbg_ids[0];
    }

    if (UTF_BG_INTRA_NODE_MANAGER == intra_node_info->state) {
        utofu_bg_id_t temp_start_bg;

        assert(utf_bg_mmap_file_info.mmaparea);
        if (utf_bg_first_call) {
            utf_bg_mmap_file_info.mmaparea->first_vbg_id =
                utf_bg_mmap_file_info.first_vbg_id;
        }
        GET_BGID_FROM_VBGID(utf_bg_detail_info->vbg_ids[0],temp_start_bg); 
        utf_bg_mmap_file_info.mmaparea->manager_start_bg_id = temp_start_bg;
        utf_bg_mmap_file_info.mmaparea->manager_tni_id = *my_tni;
        msync((void *)utf_bg_mmap_file_info.mmaparea,
              (size_t)UTF_BG_MMAP_HEADER_SIZE,MS_SYNC);
#if defined(DEBUGLOG)
        fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: mmap: first_vbg_id=%016lx manager_start_bg_id=%u/%u(%016lx) manager_tni_id=%u/%u next_tni_id=%u \n"
            ,my_hostname,__func__,my_rankset_index
            ,utf_bg_mmap_file_info.mmaparea->first_vbg_id
            ,utf_bg_mmap_file_info.mmaparea->manager_start_bg_id
            ,temp_start_bg
            ,utf_bg_detail_info->vbg_ids[0]
            ,utf_bg_mmap_file_info.mmaparea->manager_tni_id
            ,*my_tni
            ,utf_bg_mmap_file_info.mmaparea->next_tni_id
        );
#endif
    }

#if defined(DEBUGLOG)
    tempseq = sequenceptr;
    i = 0UL;
    fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: my_tni=%d utf_bg_next_tni_id=%d rc=%d \n"
        ,my_hostname,__func__,my_rankset_index
        ,*my_tni,utf_bg_next_tni_id,rc
    );
    while ((tempseq) && (tempseq->nextptr)) {
        fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: num_vbg=%zu/%zu vbg_ids=%016lx :src_rmt=%ld dst_rmt=%ld :seq=%p/%p vbg=%016lx recv=%ld/%ld send=%ld/%ld \n"
            ,my_hostname,__func__,my_rankset_index
            ,i,utf_bg_detail_info->num_vbg,utf_bg_detail_info->vbg_ids[i]
            ,utf_bg_detail_info->rmt_src_dst_seqs[i].src_rmt_index
            ,utf_bg_detail_info->rmt_src_dst_seqs[i].dst_rmt_index
            ,tempseq,sequenceptr,tempseq->vbg_id
            ,tempseq->recvinfo.index,tempseq->recvinfo.nodeinfo_index
            ,tempseq->sendinfo.index,tempseq->sendinfo.nodeinfo_index
        );
        tempseq = tempseq->nextptr;
        i++;
    }
    fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: seq=%p/%p vbg=%016lx recv=%ld/%ld send=%ld/%ld \n"
        ,my_hostname,__func__,my_rankset_index
        ,tempseq,sequenceptr,tempseq->vbg_id
        ,tempseq->recvinfo.index,tempseq->recvinfo.nodeinfo_index
        ,tempseq->sendinfo.index,tempseq->sendinfo.nodeinfo_index
    );
#endif

    return rc;
}

/*
 * EARの場合のシーケンスは先頭の段のsendinfo.nodeinfo_indexの次から
 * 最後の段のrecvinfo.nodeinfo_indexの手前までがUTF_BG_ALLOC_VBG_ID_UNNECESSARY
 * で埋まっている。
 *
 *              recv           send             nextptr
 *    ---------------------------------------------------
 *    0          -1             1                 4
 *    1          UNNECESSARY    UNNECESSARY
 *    2          UNNECESSARY    UNNECESSARY
 *    3          UNNECESSARY★  UNNECESSARY☆
 *    4          1              -1                NULL
 *
 *  EARは、dst_lcl_vbg,src_lcl_vbgを繋がない。→UTOFU_VBG_ID_NULLを設定
 *
 *  dst_lcl_vbg_idをUTOFU_VBG_ID_NULLに設定する条件も
 *  src_lcl_vbg_idをUTOFU_VBG_ID_NULLに設定する条件も
 *  最終段の一つ前の段の情報を見る。(上記の例で3の場所)
 *
 *  dst_lcl_vbg_idをUTOFU_VBG_ID_NULLに設定する条件
 *    0->sendinfo.nodeinfo_index != -1 &&
 *    3->sendinfo.nodeinfo_index == UNNECESSARY ☆
 *    3は、(0->nextptr - 1)でたどる。
 *
 *  src_lcl_vbg_idをUTOFU_VBG_ID_NULLに設定する条件
 *    4->recvinfo.nodeinfo_index != -1 &&
 *    3->recvinfo.nodeinfo_inde == UNNECESSARY ★
 *    3は、(4 - 1)でたどる。
 *
 */
/* 出力シグナルの送信先となるローカルVBGのIDの設定 */
#define SET_DST_LOCAL(tempseq,i)                                          \
{                                                                         \
    if ( (UTF_BG_ALLOC_VBG_ID_NOOPARATION != tempseq->sendinfo.vbg_id) && \
         (UTF_BG_ALLOC_VBG_ID_UNNECESSARY ==                              \
          (tempseq->nextptr - 1)->sendinfo.vbg_id) ) {                    \
        /* EARのため、dst-localは繋がない */                              \
        utf_bg_detail_info->vbg_settings[i].dst_lcl_vbg_id =              \
            UTOFU_VBG_ID_NULL;                                            \
    }                                                                     \
    else {                                                                \
        utf_bg_detail_info->vbg_settings[i].dst_lcl_vbg_id =              \
            (tempseq->nextptr)->vbg_id;                                   \
    }                                                                     \
}

/* 入力シグナルの送信元となるローカルVBGのIDの設定 */
#define SET_SRC_LOCAL(tempseq,i)                                          \
{                                                                         \
    if ( (UTF_BG_ALLOC_VBG_ID_NOOPARATION != tempseq->recvinfo.vbg_id) && \
         (UTF_BG_ALLOC_VBG_ID_UNNECESSARY ==                              \
          (tempseq - 1)->recvinfo.vbg_id) ) {                             \
        /* EARのため、src-localは繋がない */                              \
        utf_bg_detail_info->vbg_settings[i].src_lcl_vbg_id =              \
            UTOFU_VBG_ID_NULL;                                            \
    }                                                                     \
    else {                                                                \
        utf_bg_detail_info->vbg_settings[i].src_lcl_vbg_id =              \
            prevseq->vbg_id;                                              \
    }                                                                     \
}

/*
 * ローカルVBGの設定
 *
 * sequenceptr     (IN) シーケンステーブルへのポインタ
 */
static void set_local_vbg(utf_bf_alloc_butterfly_sequence_info_t *sequenceptr)
{
    utf_bf_alloc_butterfly_sequence_info_t *tempseq,*prevseq=NULL;
    size_t i=0UL;

    tempseq = sequenceptr;

    while (tempseq) {
        if (tempseq == sequenceptr) {
            /* 始点ゲート位置 */
            utf_bg_detail_info->vbg_settings[0].vbg_id = tempseq->vbg_id;
            SET_DST_LOCAL(tempseq,0);
        }
        else if (NULL == tempseq->nextptr) {
            /* 始点ゲート終点位置 */
            SET_SRC_LOCAL(tempseq,0);
        }
        else {
            /* 中継ゲート位置 */
            utf_bg_detail_info->vbg_settings[i].vbg_id = tempseq->vbg_id;
            SET_DST_LOCAL(tempseq,i);
            SET_SRC_LOCAL(tempseq,i);
        }
        prevseq = tempseq;
        tempseq = tempseq->nextptr;
        i++;
    }
    assert((i-1) == utf_bg_detail_info->num_vbg);

#if defined(DEBUGLOG)
    for (i=0;i<utf_bg_detail_info->num_vbg;i++) {
        fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: num_vbg=%zu/%zu src_lcl_vbg_id=%016lx vbg_id=%016lx dst_lcl_vbg_id=%016lx src_rmt_index=%ld dst_rmt_index=%ld \n"
            ,my_hostname,__func__,my_rankset_index
            ,i,utf_bg_detail_info->num_vbg
            ,utf_bg_detail_info->vbg_settings[i].src_lcl_vbg_id
            ,utf_bg_detail_info->vbg_settings[i].vbg_id
            ,utf_bg_detail_info->vbg_settings[i].dst_lcl_vbg_id
            ,utf_bg_detail_info->rmt_src_dst_seqs[i].src_rmt_index
            ,utf_bg_detail_info->rmt_src_dst_seqs[i].dst_rmt_index
        );
    }
#endif

    return;
}

/*
 * bginfoの作成
 */

/* VBGからTNIを抜粋 */
#define GET_TNI_FROM_VBGID(vbgid,tni)                                  \
{                                                                      \
    tni = (utofu_tni_id_t)                                             \
        ((vbgid & UTF_BG_ALLOC_MSK_TNI) >> UTF_BG_BITSFT_BGINFO_TNI);  \
}

/* シーケンステーブル内の物理6次元アドレスとrankset内の6次元アドレスとの比較 */
#define CHECK_PHY_6D_ADDR_SEQ(found,seq,temp) \
{                                             \
    found =                                   \
    ( (seq.phy_6d_addr->s.x == temp.s.x)  &&  \
      (seq.phy_6d_addr->s.y == temp.s.y)  &&  \
      (seq.phy_6d_addr->s.z == temp.s.z)  &&  \
      (seq.phy_6d_addr->s.a == temp.s.a)  &&  \
      (seq.phy_6d_addr->s.b == temp.s.b)  &&  \
      (seq.phy_6d_addr->s.c == temp.s.c) );   \
}

/* ゲートを確保できなかったため、bginfoにバリア通信使用不可ビットを設定する。*/
#define SET_DISABLE_BGINFO(bginfoposition)                           \
{                                                                    \
    bginfoposition = ((uint64_t)1 << UTF_BG_BITSFT_BGINFO_DISABLE);  \
}

/* ゲートを確保できたため、bginfoに情報を設定する。 */
#define SET_ENABLE_BGINFO(bginfoposition,                                   \
                         mytni,                                             \
                         coordc,                                            \
                         coordb,                                            \
                         coorda,                                            \
                         coordz,                                            \
                         coordy,                                            \
                         coordx,                                            \
                         src_bg_id,                                         \
                         dst_bg_id)                                         \
{                                                                           \
    bginfoposition = ((uint64_t)mytni     << UTF_BG_BITSFT_BGINFO_TNI)    | \
                    ((uint64_t)coordc    << UTF_BG_BITSFT_BGINFO_COORD_C) | \
                    ((uint64_t)coordb    << UTF_BG_BITSFT_BGINFO_COORD_B) | \
                    ((uint64_t)coorda    << UTF_BG_BITSFT_BGINFO_COORD_A) | \
                    ((uint64_t)coordz    << UTF_BG_BITSFT_BGINFO_COORD_Z) | \
                    ((uint64_t)coordy    << UTF_BG_BITSFT_BGINFO_COORD_Y) | \
                    ((uint64_t)coordx    << UTF_BG_BITSFT_BGINFO_COORD_X) | \
                    ((uint64_t)src_bg_id << UTF_BG_BITSFT_BGINFO_SRCBGID) | \
                    ((uint64_t)dst_bg_id);                                  \
}

/*
 * bginfoの作成
 *
 * my_tni      (IN)     VBGを確保したTNI番号
 * sequenceptr (IN)     シーケンステーブルへのポインタ
 * rankset     (IN)     バリアに参加する全てのプロセスのVCQのIDの配列
 *   (IN)     my_index  Rank of the process.
 *   (IN/OUT) bginfo    A pointer to the array of the VBG information of
 *                      the process.
 *
 */
int make_bginfo(utofu_tni_id_t my_tni,
                utf_bf_alloc_butterfly_sequence_info_t *sequenceptr,
                uint32_t       *rankset,
                size_t my_index,
                utf_bg_info_t *bginfo)
{
    utf_bf_alloc_butterfly_sequence_info_t *sendseq,*recvseq;
    uint32_t temp_nodeid,my_nodeid;
    utofu_tni_id_t temp_tni;

    /* ローカルVBGをつなぐ */
    set_local_vbg(sequenceptr);

    /* 自プロセスのノードIDと6次元アドレスを作成 */
    GET_NODEID_FROM_RANKSET(my_nodeid,rankset[my_index]);

    sendseq = sequenceptr;
    while (sendseq) {

        if (UTF_BG_ALLOC_INVALID_INDEX != sendseq->sendinfo.nodeinfo_index) {
            bool found;
            utofu_bg_id_t src_id,dst_id;

            src_id =(utofu_bg_id_t)0;
            GET_BGID_FROM_VBGID(sendseq->vbg_id,src_id);
            found = false;
            recvseq = sequenceptr;
            while (recvseq) {
                /*
                 * シーケンステーブルの受信情報から
                 * sendseq->sendinfo.indexと同じindex
                 * を検索する。
                 */
                /*
                 *  sequence table
                 *
                 *  VBG    src_rmt    dst_rmt
                 *  ----------------------------
                 *  6        7          1
                 *  18       1          3
                 *  19       3          7
                 *
                 *  1.
                 *     dst_rmt == 1
                 *       - 1へのsrc-VBGは6
                 *       - src_rmtが1のものを探す。
                 *       - src_rmtが1のVBGは18
                 *         - 1へのdst-VBGは18
                 *  2.
                 *     dst_rmt == 3
                 *       - 3へのsrc-VBGは18
                 *       - src_rmtが3のものを探す。
                 *       - src_rmtが3のVBGは19
                 *         - 3へのdst-VBGは19
                 *  3.
                 *     dst_rmt == 7
                 *       - 7へのsrc-VBGは19
                 *       - src_rmtが7のものを探す。
                 *       - src_rmtが7のVBGは6
                 *         - 7へのdst-VBGは6
                 */
                if (sendseq->sendinfo.index == recvseq->recvinfo.index) {
                    found = true;
                    dst_id =(utofu_bg_id_t)0;
                    GET_BGID_FROM_VBGID(recvseq->vbg_id,dst_id);
                    break;
                }
                recvseq = recvseq->nextptr;
            }
            if (!found) {
                /* internal error */
                return UTF_ERR_INTERNAL;
            }

            /* sanity check */
            /*
             * sendseq->sendinfo.phy_6d_addrが
             * rankset[sendseq->sendinfo.index]の6daddrと一致しているか？
             */
            CHECK_PHY_6D_ADDR_SEQ(found,sendseq->sendinfo,
                utf_info.vname[rankset[sendseq->sendinfo.index]].xyzabc);
            if (!found) {
                /* internal error */
                return UTF_ERR_INTERNAL;
            }
            /*
             * recvseq->recvinfo.phy_6d_addrが
             * rankset[sendseq->sendinfo.index]の6daddrと一致しているか？
             */
            CHECK_PHY_6D_ADDR_SEQ(found,recvseq->recvinfo,
                utf_info.vname[rankset[recvseq->recvinfo.index]].xyzabc);
            if (!found) {
                /* internal error */
                return UTF_ERR_INTERNAL;
            }

            bginfo[sendseq->sendinfo.index] = (utf_bg_info_t)0;
                /* sanity check */
                /* sendseq->vbg_id内の6daddrが自分の6daddrと一致しているか？ */
                GET_NODEID_FROM_VCQ(temp_nodeid,sendseq->vbg_id);
                if (temp_nodeid != my_nodeid) {
                    /* internal error */
                    return UTF_ERR_INTERNAL;
                }
                /* recvseq->vbg_id内の6daddrが自分の6daddrと一致しているか？ */
                GET_NODEID_FROM_VCQ(temp_nodeid,recvseq->vbg_id);
                if (temp_nodeid != my_nodeid) {
                    /* internal error */
                    return UTF_ERR_INTERNAL;
                }

                /* sendseq->vbg_id内のtniが引数のtniと一致しているか？ */
                GET_TNI_FROM_VBGID(sendseq->vbg_id,temp_tni);
                if (temp_tni != my_tni) {
                    /* internal error */
                    return UTF_ERR_INTERNAL;
                }
                /* recvseq->vbg_id内のtniが引数のtniと一致しているか？ */
                GET_TNI_FROM_VBGID(recvseq->vbg_id,temp_tni);
                if (temp_tni != my_tni) {
                    /* internal error */
                    return UTF_ERR_INTERNAL;
                }

                SET_ENABLE_BGINFO(bginfo[sendseq->sendinfo.index],
                    my_tni,
                    utf_info.vname[rankset[my_index]].xyzabc.s.c,
                    utf_info.vname[rankset[my_index]].xyzabc.s.b,
                    utf_info.vname[rankset[my_index]].xyzabc.s.a,
                    utf_info.vname[rankset[my_index]].xyzabc.s.z,
                    utf_info.vname[rankset[my_index]].xyzabc.s.y,
                    utf_info.vname[rankset[my_index]].xyzabc.s.x,
                    src_id,
                    dst_id);

#if defined(DEBUGLOG)
           fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: sendseq=%p vbg=%016lx :recvseq=%p vbg=%016lx : bginfo index=%zu bginfo=%016lx [src=%x dst=%x 6daddr=%02u-%02u-%02u-%1u-%1u-%1u tni=%d] \n"
               ,my_hostname,__func__,my_rankset_index
               ,sendseq,sendseq->vbg_id,recvseq,recvseq->vbg_id
               ,sendseq->sendinfo.index,bginfo[sendseq->sendinfo.index]
               ,src_id,dst_id
               ,utf_info.vname[rankset[my_index]].xyzabc.s.x
               ,utf_info.vname[rankset[my_index]].xyzabc.s.y
               ,utf_info.vname[rankset[my_index]].xyzabc.s.z
               ,utf_info.vname[rankset[my_index]].xyzabc.s.a
               ,utf_info.vname[rankset[my_index]].xyzabc.s.b
               ,utf_info.vname[rankset[my_index]].xyzabc.s.c
               ,my_tni
           );
#endif
        }

        sendseq = sendseq->nextptr;
    }

    return UTF_SUCCESS;
}

/*
 * utf_bg_alloc:  Allocation of a barrier network
 * @param
 *   (IN)     rankset[] バリアに参加する全てのプロセスのVCQのIDの配列。
 *   (IN)     len       Total number of the process.
 *   (IN)     my_index  Rank of the process.
 *   (IN/OUT) bginfo    A pointer to the array of the VBG information of
 *                      the process.
 * @return
 *   UTF_SUCCESS
 *   UTF_ERR_????
 */

/* 形状不具合のため
 *   bginfoにバリア通信使用不可ビットを設定する。
 *   bginfoに形状不具合種別を設定する。
 *   errcase 1: ノード内プロセス数の条件が合わない。
 *   errcase 2: 全ノードでノード内プロセス数が一致しない。
 *   errcase 3: 1と2の両方
 *   errcase 4: ノード数不足
 */
#define SET_DISABLE_ALL(errcase)                                           \
{                                                                          \
    memset((void *)bginfo,0,sizeof(utf_bg_info_t) * len);                  \
    for (i=0UL;i<len;i++) {                                                \
        bginfo[i] = ((uint64_t)1       << UTF_BG_BITSFT_BGINFO_DISABLE) |  \
                     ((uint64_t)errcase << UTF_BG_BITSFT_BGINFO_ERRCASE);  \
    }                                                                      \
}

/* Initializing bginfo */
#define INITIALIZE_BGINFO()                                                \
{                                                                          \
    for (i=0UL;i<len;i++) {                                                \
       bginfo[i] = (utf_bg_info_t)UTF_BG_ALLOC_INIT_BGINFO;                \
    }                                                                      \
}

/* Freeing data buffers */
#define FREE_ALLOC()                                       \
{                                                          \
    if (utf_bg_detail_info) { free(utf_bg_detail_info); }  \
    if (intra_node_info)    { free(intra_node_info); }     \
    if (sequenceptr)        { free(sequenceptr); }         \
}

#define SETINFO_CASE_SUCCESS()                                \
{                                                             \
    utf_bg_alloc_count++;                                     \
    if (utf_bg_first_call) {                                  \
        utf_bg_detail_info_first_alloc = utf_bg_detail_info;  \
    }                                                         \
    utf_bg_first_call = false;                                \
}

#if (!defined(TOFUGIJI))
/*
 * ジョブID作成関数
 */
static inline uint32_t hash_str(const char *str)
{
    register uint32_t hash = 0;

    while( *str ) {
        hash += *str++;
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash = (hash + (hash << 15));

    hash &= ~(0x8000);

    return hash;
}
#endif

int utf_bg_alloc(
                 uint32_t rankset[],
                 size_t len,
                 size_t my_index,
                 int max_ppn,
                 int comm_type,
                 utf_bg_info_t *bginfo)
{
    int rc = UTF_SUCCESS;
    int start_axis;
    char *tempval;
    uint8_t errcase=(uint8_t)0;
    utf_bg_alloc_axis_detail_info_t axes_info[UTF_BG_ALLOC_AXIS_NUM_AXES];
    utf_bg_intra_node_detail_info_t *intra_node_info=NULL;
    utf_bf_alloc_butterfly_sequence_info_t *sequenceptr=NULL;
    utf_bf_alloc_butterfly_sequence_info_t *optimizedsequence=NULL;
    size_t i,number_vbgs=0UL;
    utofu_tni_id_t my_tni=UTF_BG_MIN_TNI_NUMBER;

    /*
     * Check the arguments
     */
    if (NULL == (void *)rankset) {
        rc = UTF_ERR_INVALID_POINTER;
        return rc;
    }
    if (0UL == len) {
        rc = UTF_ERR_INVALID_NUMBER;
        return rc;
    }
    if (len <= my_index) {
        rc = UTF_ERR_INVALID_NUMBER;
        return rc;
    }
    if ( (UTF_BG_ALLOC_MAX_PROC_IN_NODE < max_ppn) || (max_ppn <= 0) ) {
        rc = UTF_ERR_INVALID_NUMBER;
        return rc;
    }
    switch (comm_type) {
        case UTF_BG_TOFU:
        case UTF_BG_SM:
            if (!utf_bg_first_call) {
                if (
                    ((UTF_BG_TOFU == comm_type) &&
                     (!utf_bg_intra_node_barrier_is_tofu))
                    ||
                    ((UTF_BG_SM == comm_type) &&
                     (utf_bg_intra_node_barrier_is_tofu))
                   ) {
                    rc = UTF_ERR_INVALID_NUMBER;
                    return rc;
                }
            }
            break;
        default:
            rc = UTF_ERR_INVALID_NUMBER;
            return rc;
    }
    if (NULL == (void *)bginfo) {
        rc = UTF_ERR_INVALID_POINTER;
        return rc;
    }

#if defined(DEBUGLOG)
    my_rankset_index = my_index;
#endif
    /*
     * 環境変数の確認
     * 最初の呼び出し時に行えば良いと考える。
     */
    if (utf_bg_first_call) {
        utf_bg_intra_node_barrier_is_tofu = (UTF_BG_TOFU == comm_type) ?
            true : false;
        memset((void *)&utf_bg_mmap_file_info,
               0,sizeof(utf_bg_mmap_file_info_t));
        if (NULL != (tempval=getenv("UTF_BG_DISABLE_UTF_SHM"))) {
            utf_bg_mmap_file_info.disable_utf_shm = true;
        }
        if (true == utf_bg_mmap_file_info.disable_utf_shm) {
            if (NULL != (tempval=getenv("PJM_JOBID"))) {
                my_jobid = atoi(tempval);
#if (!defined(TOFUGIJI))
                {
                    pmix_proc_t myproc;

                    memset((void *)&myproc,0,sizeof(pmix_proc_t));
                    if (PMIX_SUCCESS !=  PMIx_Init(&myproc,NULL,0)) {
                        return UTF_ERR_INTERNAL;
                    }
                    my_jobid = (int)hash_str((const char *)myproc.nspace);
                    if (PMIX_SUCCESS != PMIx_Finalize(NULL, 0)) {
                        return UTF_ERR_INTERNAL;
                    }
                }
#else
                /* internal error */
                rc = UTF_ERR_INTERNAL;
                return rc;
#endif
            }
            else {
#if defined(TOFUGIJI)
                if (NULL != (tempval=getenv("UTOFU_WORLD_ID"))) {
                    my_jobid = atoi(tempval);
                }
                else if (NULL != (tempval=getenv("OMPI_MCA_ess_base_jobid"))) {
                    my_jobid = atoi(tempval);
                }
                else if (NULL != (tempval=getenv("OMPI_MCA_orte_ess_jobid"))) {
                    my_jobid = atoi(tempval);
                }
                else {
#endif
                    /* internal error */
                    rc = UTF_ERR_INTERNAL;
                    return rc;
#if defined(TOFUGIJI)
                }
#endif
            }
        } /* false == utf_bg_mmap_file_info.disable_utf_shm */
        if (NULL != (tempval=getenv("UTF_BG_ENABLE_THREAD_SAFE"))) {
            utf_bg_alloc_flags = UTOFU_VBG_FLAG_THREAD_SAFE;
        }
        if (NULL != (tempval=getenv("UTF_BG_MAX_INTRA_NODE_PROCS_FOR_HB"))) {
            max_intra_node_procs_for_hb = atoi(tempval);
        }
#if defined(DEBUGLOG)
        if (NULL != (tempval=getenv("XONLY"))) {
            xonlyflag = 1;
        }
#endif
#if defined(UTF_BG_UNIT_TEST)
        /*
         * 仮デバッグマクロ
         * UTF本体結合するまでのテスト用
         */
        if (NULL != (tempval=getenv("UTF_DEBUG"))) {
            if (!strncmp(tempval, "0x", 2)) {
                sscanf(tempval, "%x", &utf_dflag);
            } else {
                utf_dflag = atoi(tempval);
            }
        }
        mypid = my_index;
#endif
        gethostname(my_hostname,64);
    }

    start_axis=UTF_BG_ALLOC_AXIS_IN_NODE;
    if (!utf_bg_intra_node_barrier_is_tofu) { start_axis=UTF_BG_ALLOC_AXIS_ON_X; }

    utf_bg_detail_info =
       (utf_coll_group_detail_t *)malloc(sizeof(utf_coll_group_detail_t));
    if (NULL == utf_bg_detail_info) {
        FREE_ALLOC();
        rc = UTF_ERR_OUT_OF_MEMORY;
        return rc;
    }
    memset((void *)utf_bg_detail_info,0,sizeof(utf_coll_group_detail_t));

    utf_bg_detail_info->arg_info.rankset  = rankset;
    utf_bg_detail_info->arg_info.len      = len;
    utf_bg_detail_info->arg_info.my_index = my_index;

#if defined(DEBUGLOG)
#if defined(UTF_BG_UNIT_TEST)
/*
 * 仮デバッグマクロ
 * UTF本体結合するまでのテスト用
 */
    fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: mypid=%d utf_dflag=%x \n"
        ,my_hostname,__func__,my_index
        ,mypid,utf_dflag
    );
#endif
    fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: len=%zu my_jobid=%d utf_bg_first_call=%d utf_bg_alloc_count=%lu utf_bg_intra_node_barrier_is_tofu=%d : utf_bg_alloc_flags=%lu max_intra_node_procs_for_hb=%d xonlyflag=%d disable_utf_shm=%d \n"
        ,my_hostname,__func__,my_index
        ,len,my_jobid
        ,utf_bg_first_call,utf_bg_alloc_count
        ,utf_bg_intra_node_barrier_is_tofu
        ,utf_bg_alloc_flags,max_intra_node_procs_for_hb,xonlyflag
        ,utf_bg_mmap_file_info.disable_utf_shm
    );
    fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: max_ppn=%d comm_type=%d \n"
        ,my_hostname,__func__,my_index
        ,max_ppn,comm_type
    );
#endif

    /*
     * プロセス数の確認
     */
    if (len < (size_t)4) {
        errcase |= UTF_BG_ALLOC_TOO_FEW_NODE;
        SET_DISABLE_ALL(errcase);
        SETINFO_CASE_SUCCESS();
        return rc;
    }

    /*
     * 最初のutf_bg_alloc呼び出しによるMPI_COMM_WORLD用のVBG確保に
     * 失敗したため、以降のutf_bg_initはUTF_ERR_RESOURCE_BUSYで復帰させる。
     */
    if ( (!utf_bg_first_call) &&
         (utf_bg_mmap_file_info.first_alloc_is_failed) ) {
#if defined(DEBUGLOG)
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: utf barrier communication is invalid \n"
            ,my_hostname,__func__,my_index
        );
#endif
        SET_DISABLE_ALL(errcase);
        utf_bg_alloc_count++;
        return rc;
    }

    /*
     * アクシステーブル
     */
    memset((void *)(&axes_info[0]),0,
        sizeof(utf_bg_alloc_axis_detail_info_t) * UTF_BG_ALLOC_AXIS_NUM_AXES);
    rc = make_axes_and_intra_node_infos(rankset,len,my_index,
                                        max_ppn,
                                        &axes_info[0],&intra_node_info,
                                        &errcase);
#if defined(DEBUGLOG)
    fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: make_axes_and_intra_node_infos=%d intra_node_info=%p errcase=%x \n"
        ,my_hostname,__func__,my_index
        ,rc,intra_node_info,errcase
    );
#endif
    switch (rc) {
        case UTF_SUCCESS:
            utf_bg_detail_info->intra_node_info = intra_node_info;
            break;
        case UTF_BG_ALLOC_ERR_UNMATCH_PROC_PER_NODE:
        case UTF_BG_ALLOC_ERR_TOO_MANY_PROC_IN_NODE:
        case UTF_BG_ALLOC_ERR_TOO_FEW_NODE:
            utf_bg_detail_info->intra_node_info = intra_node_info;
            if (!errcase) {
                /* internal error */
                FREE_ALLOC();
                return UTF_ERR_INTERNAL;
            }
            break;
        default:
            return rc;
    }

    /*
     * mmap領域の確保
     */
    if (utf_bg_first_call) {
        int rcmmap;
        errno = 0;
        rcmmap = make_mmap_file(
                                rankset[my_index],
                                intra_node_info);
#if defined(DEBUGLOG)
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: make_mmap_file=%d disable_utf_shm=%d file_name=%s size=%zu width=%zu fd=%d mmaparea=%p first_vbg_id=%016lx \n"
            ,my_hostname,__func__,my_index
            ,rcmmap
            ,utf_bg_mmap_file_info.disable_utf_shm
            ,utf_bg_mmap_file_info.file_name
            ,utf_bg_mmap_file_info.size
            ,utf_bg_mmap_file_info.width
            ,utf_bg_mmap_file_info.fd
            ,utf_bg_mmap_file_info.mmaparea
            ,utf_bg_mmap_file_info.first_vbg_id
        );
#endif
        if (UTF_SUCCESS != rcmmap) {
            FREE_ALLOC();
            return rcmmap;
        }
    }

    if ( (max_intra_node_procs_for_hb < max_ppn) &&
         (utf_bg_intra_node_barrier_is_tofu) ) {
        errcase |= UTF_BG_ALLOC_TOO_MANY_PROC;
        SET_DISABLE_ALL(errcase);
        SETINFO_CASE_SUCCESS();
        return UTF_SUCCESS;
    }

    /*
     * アクシステーブル作成の合否(rc)判定
     */
    if (UTF_SUCCESS != rc) {
        /*
         * case UTF_BG_ALLOC_ERR_UNMATCH_PROC_PER_NODE:
         * case UTF_BG_ALLOC_ERR_TOO_MANY_PROC_IN_NODE:
         * case UTF_BG_ALLOC_ERR_TOO_FEW_NODE:
         */
        SET_DISABLE_ALL(errcase);
        SETINFO_CASE_SUCCESS();
        return UTF_SUCCESS;
    }

    if ( (UTF_BG_INTRA_NODE_WORKER == intra_node_info->state) &&
         (!utf_bg_intra_node_barrier_is_tofu) ) {
        /*
         * ノード内ソフトバリアのため、UTF_BG_INTRA_NODE_WORKERは
         * ゲートを組む必要がない。
         * 必要な情報をセットして、復帰する。
         */
        INITIALIZE_BGINFO();
        SETINFO_CASE_SUCCESS();
        return rc;
    }

    /*
     * バタフライネットワークの構築
     */
    rc = make_butterfly_network(&axes_info[0],start_axis,
                                &sequenceptr, /* 領域解放用先頭アドレス */
                                &optimizedsequence,
                                        /* 最適化された領域の先頭を指す */
                                &number_vbgs
                           );
#if defined(DEBUGLOG)
    fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: make_butterfly_network=%d sequenceptr=%p optimizedsequence=%p number_vbgs=%zu \n"
        ,my_hostname,__func__,my_index
        ,rc,sequenceptr,optimizedsequence,number_vbgs
    );
#endif
    if (UTF_SUCCESS != rc) {
        FREE_ALLOC();
        return rc;
    }

    /*
     * ゲート確保
     */
    rc = allocate_vbg(number_vbgs,intra_node_info,optimizedsequence,&my_tni);
#if defined(DEBUGLOG)
    fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: allocate_vbg=%d first_vbg_id=%016lx my_tni=%d \n"
        ,my_hostname,__func__,my_index
        ,rc,utf_bg_mmap_file_info.first_vbg_id,my_tni
    );
#endif
    if ( (UTF_SUCCESS != rc) && (UTF_ERR_FULL != rc) ) {
        FREE_ALLOC();
        return rc;
    }
#if defined(DEBUGLOG)
    fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu:INTRAINFO ptr=%p state=%ld size=%zu intra_index=%zu curr_seq=%lu mmap_buf=%p \n"
        ,my_hostname,__func__,my_index
        ,utf_bg_detail_info->intra_node_info
        ,utf_bg_detail_info->intra_node_info->state
        ,utf_bg_detail_info->intra_node_info->size
        ,utf_bg_detail_info->intra_node_info->intra_index
        ,utf_bg_detail_info->intra_node_info->curr_seq
        ,utf_bg_detail_info->intra_node_info->mmap_buf
    );
#endif

    /*
     * bginfo作成
     */
    INITIALIZE_BGINFO();
    if (UTF_SUCCESS ==rc) {
        rc = make_bginfo(my_tni,optimizedsequence,rankset,my_index,bginfo);
#if defined(DEBUGLOG)
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: make_bginfo=%d \n"
            ,my_hostname,__func__,my_index
            ,rc
    );
#endif
        if (UTF_SUCCESS != rc) {
            FREE_ALLOC();
            return rc;
        }
    }
    else {
        errcase=0;
        SET_DISABLE_ALL(errcase);
        rc = UTF_SUCCESS; 
#if defined(DEBUGLOG)
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: UTOFU_ERR_FULL \n"
            ,my_hostname,__func__,my_index
        );
#endif
    } 
    DEBUG(DLEVEL_PROTO_VBG) {
        char msg[256];
        for (i=0UL;i<len;i++) {
            snprintf(msg, 256,"%s: %s: count=%lu my_index=%zu len=%zu/%zu rankset=%016lx bginfo=%016lx \n"
                ,__func__,my_hostname,utf_bg_alloc_count
                ,my_index,i,len
                ,(long unsigned int)rankset[i]
                ,bginfo[i]);
            utf_printf("%s",msg);
        }
    }

    SETINFO_CASE_SUCCESS();
    return rc;
}
