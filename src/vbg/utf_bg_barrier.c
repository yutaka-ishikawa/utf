/*
 * Copyright (c) 2020 XXXXXXXXXXXXXXXXXXXXXXXX.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include "utf_bg_internal.h"
#include "utf_bg_barrier.h"

utf_bg_poll_info_t poll_info;

int            utf_bg_poll_barrier_func;
int           (*utf_bg_poll_reduce_func) (void **);

enum utf_bg_poll_barrier_index {
    UTF_POLL_BARRIER,
    UTF_POLL_BARRIER_SM
};

/**
 *
 * utf_barrier
 * call utofu_barrier().
 *
 * group_struct (IN)  A pointer to the structure that stores the VBG information.
 */
int utf_barrier(utf_coll_group_t group_struct)
{
    utf_coll_group_detail_t *utf_bg_grp = (utf_coll_group_detail_t *)group_struct;

    /* algorithm check */
    /* sm指定でも1ノード１プロセス実行の場合はtofu(ハードバリア処理)を実行する */
    if (utf_bg_intra_node_barrier_is_tofu ||
        1 == ((utf_coll_group_detail_t *)group_struct)->intra_node_info->size){
        /* tofu (hard barrier) */
        poll_info.utf_bg_poll_ids = *(utf_bg_grp->vbg_ids);
        utf_bg_poll_barrier_func = UTF_POLL_BARRIER;
        return utofu_barrier(poll_info.utf_bg_poll_ids, 0);
    }else{
        /* sm (soft barrier) */
        utf_bg_poll_barrier_func = UTF_POLL_BARRIER_SM;
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s:rank=%zu seq_val=%zu(%zu)\n",
                __func__, poll_info.sm_intra_index, poll_info.sm_seq_val, utf_bg_grp->intra_node_info->curr_seq);
#endif
        UTF_BG_INIT_SM_COMMON(utf_bg_grp);
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s:rank=%zu seq_val=%zu(%zu)\n",
                __func__, poll_info.sm_intra_index, poll_info.sm_seq_val, utf_bg_grp->intra_node_info->curr_seq);
#endif
        return UTF_SUCCESS;
    }
}

/**
 *
 * utf_poll_barrier
 * Polls for barrier synchronization completion.
 *
 * group_struct (IN)  A pointer to the structure that stores the VBG information.
 */
#if defined(__clang__)
#pragma clang optimize off
#endif
int utf_poll_barrier(utf_coll_group_t group_struct)
{
    if (utf_bg_poll_barrier_func == UTF_POLL_BARRIER){
        return utofu_poll_barrier(poll_info.utf_bg_poll_ids, 0);
    }else if(utf_bg_poll_barrier_func == UTF_POLL_BARRIER_SM){
        return utf_poll_barrier_sm();
    }else{
        return UTF_ERR_NOT_AVAILABLE;
    }
}
#if defined(__clang__)
#pragma clang optimize on
#endif


static int utf_poll_reduce_double(void **data)
{
    int rc, i;

    if(UTOFU_SUCCESS !=
       (rc = utofu_poll_reduce_double(poll_info.utf_bg_poll_ids,
                                      0,
                                      (double *)poll_info.utf_bg_poll_odata))){
        return rc;
    }
    /* 開始側で実施した処理を戻してユーザバッファへ格納する */
    UTF_BG_SET_RESULT_DOUBLE();

    return UTF_SUCCESS;
}


static int utf_poll_reduce_float(void **data)
{
    int rc, i;

    if(UTOFU_SUCCESS !=
       (rc = utofu_poll_reduce_double(poll_info.utf_bg_poll_ids,
                                      0,
                                      (double *)poll_info.utf_bg_poll_odata))){
        return rc;
    }
    /* 開始側で実施した処理を戻してユーザバッファへ格納する */
    UTF_BG_SET_RESULT_FLOAT();

    return UTF_SUCCESS;
}

#if defined(__clang__)
static int utf_poll_reduce_float16(void **data)
{
    int rc, i;

    if(UTOFU_SUCCESS !=
       (rc = utofu_poll_reduce_double(poll_info.utf_bg_poll_ids,
                                      0,
                                      (double *)poll_info.utf_bg_poll_odata))){
        return rc;
    }
    /* 開始側で実施した処理を戻してユーザバッファへ格納する */
    UTF_BG_SET_RESULT_FLOAT16();

    return UTF_SUCCESS;
}
#endif

static int utf_poll_reduce_comp(void **data)
{
    int rc;

    if(UTOFU_SUCCESS !=
       (rc = utofu_poll_reduce_double(poll_info.utf_bg_poll_ids,
                                      0,
                                      (double *)poll_info.utf_bg_poll_odata))){
        return rc;
    }
    /* 開始側で実施した処理を戻してユーザバッファへ格納する */
    UTF_BG_SET_RESULT_COMP();

    return UTF_SUCCESS;
}


static int utf_poll_reduce_uint64(void **data)
{
    int rc, i;

    if(UTOFU_SUCCESS !=
       (rc = utofu_poll_reduce_uint64(poll_info.utf_bg_poll_ids,
                                      0,
                                      (uint64_t *)poll_info.utf_bg_poll_odata))){
        return rc;
    }
    /* 開始側で実施した処理を戻してユーザバッファへ格納する */
    UTF_BG_SET_RESULT_UINT64();

    return UTF_SUCCESS;
}


static int utf_poll_reduce_double_max_min(void **data)
{
    int rc, i;

    if(UTOFU_SUCCESS !=
       (rc = utofu_poll_reduce_double(poll_info.utf_bg_poll_ids,
                                      0,
                                      (double *)poll_info.utf_bg_poll_odata))){
        return rc;
    }
    /* 開始側で実施した処理を戻してユーザバッファへ格納する */
    UTF_BG_SET_RESULT_DOUBLE_MAX_MIN();

    return UTF_SUCCESS;
}

static int utf_poll_reduce_uint64_max_min(void **data)
{
    int rc, i;
    bool is_signed;

    if(UTOFU_SUCCESS !=
       (rc = utofu_poll_reduce_uint64(poll_info.utf_bg_poll_ids,
                                      0,
                                      (uint64_t *)poll_info.utf_bg_poll_odata))){
        return rc;
    }
    /* 開始側で実施した処理を戻してユーザバッファへ格納する */
    UTF_BG_SET_RESULT_UINT64_MAX_MIN();

    return UTF_SUCCESS;
}

static int utf_poll_reduce_logical(void **data)
{
    int rc, i, j;
    int bit_count;

    if(UTOFU_SUCCESS !=
       (rc = utofu_poll_reduce_uint64(poll_info.utf_bg_poll_ids,
                                      0,
                                      (uint64_t *)poll_info.utf_bg_poll_odata))){
        return rc;
    }
    /* 開始側で実施した処理を戻してユーザバッファへ格納する */
    UTF_BG_SET_RESULT_LOGICAL();

    return UTF_SUCCESS;
}

static int utf_poll_reduce_bitwise(void **data)
{
    int rc;

    if(UTOFU_SUCCESS !=
       (rc = utofu_poll_reduce_uint64(poll_info.utf_bg_poll_ids,
                                      0,
                                      (uint64_t *)poll_info.utf_bg_poll_odata))){
        return rc;
    }
    /* 開始側で実施した処理を戻してユーザバッファへ格納する */
    UTF_BG_SET_RESULT_BITWISE();

    return UTF_SUCCESS;
}

static int utf_poll_reduce_maxmin_loc(void **data)
{
    int rc, i;

    if(UTOFU_SUCCESS !=
       (rc = utofu_poll_reduce_uint64(poll_info.utf_bg_poll_ids,
                                      0,
                                      (uint64_t *)poll_info.utf_bg_poll_odata))){
        return rc;
    }
    /* 開始側で実施した処理を戻してユーザバッファへ格納する */
    UTF_BG_SET_RESULT_MAXMIN_LOC();

    return UTF_SUCCESS;
}

static int utf_poll_bcast(void **data)
{
    int rc;

    if(UTOFU_SUCCESS !=
       (rc = utofu_poll_reduce_uint64(poll_info.utf_bg_poll_ids,
                                      0,
                                      (uint64_t *)poll_info.utf_bg_poll_odata))){
        return rc;
    }
    /* 開始側で実施した処理を戻してユーザバッファへ格納する */
    UTF_BG_SET_RESULT_BCAST();

    return UTF_SUCCESS;
}


/**
 *
 * utf_bg_reduce_uint64
 * 整数型のリダクション演算(SUM)における処理を実施します。
 *
 * utf_bg_grp (IN)  A pointer to the structure that stores the VBG information
 * buf        (IN)  address of send buffer
 * count      (IN)  number of elements in send buffer
 * size       (IN)  data type size
 */
static int utf_bg_reduce_uint64(utf_coll_group_detail_t *utf_bg_grp,
                                const void *buf,
                                int count,
                                size_t size)
{
    uint64_t *idata = poll_info.utf_bg_poll_idata;
    int i;

    for(i=0;i<count;i++){
        *(idata + i) = (uint64_t)0;
        memcpy((char *)idata+i*sizeof(uint64_t)+sizeof(uint64_t)-size, (char *)buf+i*size, size);
    }

    /* algorithm check */
    if(utf_bg_intra_node_barrier_is_tofu || 1 == utf_bg_grp->intra_node_info->size){
        /* tofu (hard barrier) */
        utf_bg_poll_reduce_func = utf_poll_reduce_uint64;
        return utofu_reduce_uint64(poll_info.utf_bg_poll_ids, (enum utofu_reduce_op)UTF_REDUCE_OP_SUM,
                                   idata, (size_t)count, 0);
    }else{
        /* sm (soft barrier) */
        utf_bg_poll_reduce_func = utf_poll_reduce_uint64_sm;
        UTF_BG_INIT_SM(utf_bg_grp, UTF_REDUCE_OP_SUM, count);
        return UTF_SUCCESS;
    }
}


/**
 *
 * utf_bg_reduce_double
 * 浮動小数点型のリダクション演算(SUM)における処理を実施します。
 *
 * utf_bg_grp (IN)  A pointer to the structure that stores the VBG information.
 * buf        (IN)  address of send buffer
 * count      (IN)  number of elements in send buffer
 * size       (IN)  data type size
 */
static int utf_bg_reduce_double(utf_coll_group_detail_t *utf_bg_grp,
                                const void *buf,
                                int count,
                                size_t size)
{
    double *idata = poll_info.utf_bg_poll_idata;
    int i;

    switch(size){
      case sizeof(double):
        for(i=0;i<count;i++){
            *(idata + i) = *((double *)buf + i);
        }
        utf_bg_poll_reduce_func = utf_poll_reduce_double;
        break;
      case sizeof(float):
        for(i=0;i<count;i++){
            *(idata + i) = (double)*((float *)buf + i);
        }
        utf_bg_poll_reduce_func = utf_poll_reduce_float;
        break;
#if defined(__clang__)
      case sizeof(_Float16):
        for(i=0;i<count;i++){
            *(idata + i) = (double)*((_Float16 *)buf + i);
        }
        utf_bg_poll_reduce_func = utf_poll_reduce_float16;
#endif
    }

    /* algorithm check */
    if(utf_bg_intra_node_barrier_is_tofu || 1 == utf_bg_grp->intra_node_info->size){
        /* tofu (hard barrier) */
        poll_info.utf_bg_poll_ids = *(utf_bg_grp->vbg_ids);
        return utofu_reduce_double(poll_info.utf_bg_poll_ids, (enum utofu_reduce_op)UTF_REDUCE_OP_BFPSUM,
                                   idata, (size_t)count, 0);
    }else{
        /* sm (soft barrier) */
        utf_bg_poll_reduce_func = utf_poll_reduce_double_sm;
        UTF_BG_INIT_SM(utf_bg_grp, UTF_REDUCE_OP_BFPSUM, count);
        return UTF_SUCCESS;
    }
}


/**
 *
 * utf_bg_reduce_uint64_max_min
 * 整数型のリダクション演算(MAX/MIN)における前処理を実施します。
 *
 * utf_bg_grp (IN)  A pointer to the structure that stores the VBG information.
 * buf        (IN)  address of send buffer
 * count      (IN)  number of elements in send buffer
 * size       (IN)  data type size
 * op         (IN)  reduce operation
 * sign       (IN)  データ型の符号有無
 */
static int utf_bg_reduce_uint64_max_min(utf_coll_group_detail_t *utf_bg_grp,
                                        const void *buf,
                                        int count,
                                        size_t size,
                                        enum utf_reduce_op op,
                                        bool is_signed)
{
    uint64_t *idata = poll_info.utf_bg_poll_idata;
    int i;

    for(i=0;i<count;i++){
        memcpy((char *)idata+i*sizeof(uint64_t)+sizeof(uint64_t)-size, (char *)buf+i*size , size);

        /* 負の値を反転させる */
        if(is_signed){
             *((uint64_t *)idata + i) += UTF_BG_REDUCE_MASK_HB;
        }

        /* 最小値を最大値へ変換 */
        if(UTF_REDUCE_OP_MIN == op){
            *((uint64_t *)idata + i) = ~(*((uint64_t *)idata + i));
        }
    }

    /* algorithm check */
    if(utf_bg_intra_node_barrier_is_tofu || 1 == utf_bg_grp->intra_node_info->size){
        /* tofu (hard barrier) */
        poll_info.utf_bg_poll_ids = *(utf_bg_grp->vbg_ids);
        utf_bg_poll_reduce_func = utf_poll_reduce_uint64_max_min;
        return utofu_reduce_uint64(poll_info.utf_bg_poll_ids, (enum utofu_reduce_op)UTF_REDUCE_OP_MAX,
                                   idata, (size_t)count, 0);
    }else{
        /* sm (soft barrier) */
        utf_bg_poll_reduce_func = utf_poll_reduce_uint64_sm;
        UTF_BG_INIT_SM(utf_bg_grp, UTF_REDUCE_OP_MAX, count);
        return UTF_SUCCESS;
    }
}


/**
 *
 * utf_bg_reduce_double_max_min
 * 浮動小数点型のリダクション演算(MAX/MIN)における前処理を実施します。
 *
 * utf_bg_grp (IN)  A pointer to the structure that stores the VBG information.
 * buf        (IN)  address of send buffer
 * count      (IN)  number of elements in send buffer
 * size       (IN)  data type size
 * op         (IN)  reduce operation
 */
static int utf_bg_reduce_double_max_min(utf_coll_group_detail_t *utf_bg_grp,
                                        const void *buf,
                                        int count,
                                        size_t size,
                                        enum utf_reduce_op op)
{
    uint64_t *idata = poll_info.utf_bg_poll_idata;
    int i;

    for(i=0;i<count;i++){
        memcpy((char *)idata+i*sizeof(uint64_t)+sizeof(uint64_t)-size, (char *)buf+i*size , size);

        if(op == UTF_REDUCE_OP_MAX){
            *(idata + i) ^= (*(idata + i) & UTF_BG_REDUCE_MASK_HB)?
                            UTF_BG_REDUCE_MASK_MAX: UTF_BG_REDUCE_MASK_HB;
        }else{
            if(!(*(idata + i) & UTF_BG_REDUCE_MASK_HB)){
                *(idata + i) ^= UTF_BG_REDUCE_MASK_MIN;
            }
        }

        switch((int)size){
            case sizeof(double):
                *(idata + i) += UTF_BG_REDUCE_MASK_NAN_FP64;
                break;
            case sizeof(float):
                *(idata + i) += UTF_BG_REDUCE_MASK_NAN_FP32;
                break;
#if defined(__clang__)
            case sizeof(_Float16):
                *(idata + i) += UTF_BG_REDUCE_MASK_NAN_FP16;
#endif
        }
    }

    /* algorithm check */
    if(utf_bg_intra_node_barrier_is_tofu || 1 == utf_bg_grp->intra_node_info->size){
        /* tofu (hard barrier) */
        poll_info.utf_bg_poll_ids = *(utf_bg_grp->vbg_ids);
        utf_bg_poll_reduce_func = utf_poll_reduce_double_max_min;
        return utofu_reduce_uint64(poll_info.utf_bg_poll_ids, (enum utofu_reduce_op)UTF_REDUCE_OP_MAX,
                                   idata, (size_t)count, 0);
    }else{
        /* sm (soft barrier) */
        utf_bg_poll_reduce_func = utf_poll_reduce_uint64_sm;
        UTF_BG_INIT_SM(utf_bg_grp, UTF_REDUCE_OP_MAX, count);
        return UTF_SUCCESS;
    }
}


/**
 *
 * utf_bg_reduce_logical
 * リダクション演算(LAND/LOR/LXOR)における前処理を実施します。
 *
 * utf_bg_grp (IN)  A pointer to the structure that stores the VBG information.
 * buf        (IN)  starting address of buffer
 * count      (IN)  number of elements in send buffer
 * size       (IN)  data type size
 * op         (IN)  reduce operation
 */
static int utf_bg_reduce_logical(utf_coll_group_detail_t *utf_bg_grp,
                                 const void *buf,
                                 int count,
                                 size_t size,
                                 enum utf_reduce_op op)
{
    uint64_t *idata = poll_info.utf_bg_poll_idata;
    int i=0, j=0;
    uint64_t conpare_buff=0;
    int op_type;

    /* idata何要素分に収まるか計算 */
    poll_info.utf_bg_poll_numcount = (count + (UTF_BG_REDUCE_ULMT_ELMS_64 -1)) / UTF_BG_REDUCE_ULMT_ELMS_64;

    /* bufの値を確認し、0以外ならbitを立てる。idata(8byte変数)に64bitずつ詰め込む。 */
    while(1){
        if(0 == memcmp(((char *)buf+i*size), &conpare_buff, size)){
            *(idata+j) &= ~UTF_BG_REDUCE_MASK_LB; /* 0 */
        }else{
            *(idata+j) |= UTF_BG_REDUCE_MASK_LB;  /* 1 */
        }
        if((i+1)%UTF_BG_REDUCE_ULMT_ELMS_64 == 0 && i !=0){
            j++;
        }
        if(i == ((count)-1)){
            break;
        }
        *(idata+j) <<= 1;
        i++;
    }

    /* operation decision */
    if(op == UTF_REDUCE_OP_LAND){
        op_type = UTF_REDUCE_OP_BAND;
    }else if(op == UTF_REDUCE_OP_LOR){
        op_type = UTF_REDUCE_OP_BOR;
    }else{
        op_type = UTF_REDUCE_OP_BXOR;
    }

    /* algorithm check */
    if(utf_bg_intra_node_barrier_is_tofu || 1 == utf_bg_grp->intra_node_info->size){
        /* tofu (hard barrier) */
        poll_info.utf_bg_poll_ids = *(utf_bg_grp->vbg_ids);
        utf_bg_poll_reduce_func = utf_poll_reduce_logical;
        return utofu_reduce_uint64(poll_info.utf_bg_poll_ids, (enum utofu_reduce_op)op_type,
                                   idata, (size_t)poll_info.utf_bg_poll_numcount, 0);
    }else{
        /* sm (soft barrier) */
        utf_bg_poll_reduce_func = utf_poll_reduce_uint64_sm;
        UTF_BG_INIT_SM(utf_bg_grp, (enum utf_reduce_op)op_type, poll_info.utf_bg_poll_numcount);
        return UTF_SUCCESS;
    }
}


/**
 *
 * utf_bg_reduce_bitwise
 * リダクション演算(BAND/BOR/BXOR)を実施します。
 *
 * utf_bg_grp (IN)  A pointer to the structure that stores the VBG information.
 * buf        (IN)  starting address of buffer
 * count      (IN)  number of elements in send buffer
 * size       (IN)  data type size
 * op         (IN)  reduce operation
 */
static int utf_bg_reduce_bitwise(utf_coll_group_detail_t *utf_bg_grp,
                                 const void *buf,
                                 int count,
                                 size_t size,
                                 enum utf_reduce_op op)
{
    uint64_t *idata = poll_info.utf_bg_poll_idata;
    int num_count;

    // idata詰めたときのcountを計算
    num_count = (count + (sizeof(uint64_t)/size - 1)) / (sizeof(uint64_t)/size);

    // tmpバッファへのコピー
    memcpy(idata, buf, count*size);

    /* algorithm check */
    if(utf_bg_intra_node_barrier_is_tofu || 1 == utf_bg_grp->intra_node_info->size){
        /* tofu (hard barrier) */
        poll_info.utf_bg_poll_ids = *(utf_bg_grp->vbg_ids);
        utf_bg_poll_reduce_func = utf_poll_reduce_bitwise;
        return utofu_reduce_uint64(poll_info.utf_bg_poll_ids, (enum utofu_reduce_op)op,
                                   idata, (size_t)num_count, 0);
    }else{
        /* sm (soft barrier) */
        utf_bg_poll_reduce_func = utf_poll_reduce_uint64_sm;
        UTF_BG_INIT_SM(utf_bg_grp, op, num_count);
        return UTF_SUCCESS;
    }
}


/**
 *
 * utf_bg_reduce_maxmin_loc
 * リダクション演算(MAXLOC/MINLOC)を実施します。
 *
 * utf_bg_grp (IN)  A pointer to the structure that stores the VBG information.
 * buf        (IN)  starting address of buffer
 * count      (IN)  number of elements in send buffer
 * size       (IN)  data type size
 * op         (IN)  reduce operation
 */
static int utf_bg_reduce_maxmin_loc(utf_coll_group_detail_t *utf_bg_grp,
                                    const void *buf,
                                    int count,
                                    size_t size,
                                    enum utf_reduce_op op)
{
    uint64_t *idata = poll_info.utf_bg_poll_idata;
    int i;

    for(i=0;i<count*2;i++){
        switch(size){
            case 8:
                memcpy((char *)idata+i*sizeof(uint64_t)+sizeof(uint64_t)-size/2, (char *)buf+i*size/2, size/2);
                break;
            case 12:
                if(i%2 == 0){
                    memcpy(idata+i, (long *)buf+i, sizeof(long));
                }else{
                    memcpy((char *)idata+i*sizeof(uint64_t)+sizeof(uint64_t)-sizeof(int),
                           (char *)buf+i*sizeof(long), sizeof(int));
                }
                break;
            case 6:
                if(i%2 == 0){
                    memcpy((char *)idata+i*sizeof(uint64_t)+sizeof(uint64_t)-sizeof(short),
                           (char *)buf+i*sizeof(int), sizeof(short));
                }else{
                    memcpy((char *)idata+i*sizeof(uint64_t)+sizeof(uint64_t)-sizeof(int),
                           (char *)buf+i*sizeof(int), sizeof(int));
                }
        }

        /* 符号無整数に変換 */
        *(idata+i) += UTF_BG_REDUCE_MASK_HB;
    }
    for(i=0;i<count*2;i+=2){
        if(UTF_REDUCE_OP_MINLOC == op){
            *(idata + i) = ~(*(idata + i));
        }
    }

    /* algorithm check */
    if(utf_bg_intra_node_barrier_is_tofu || 1 == utf_bg_grp->intra_node_info->size){
        /* tofu (hard barrier) */
        poll_info.utf_bg_poll_ids = *(utf_bg_grp->vbg_ids);
        utf_bg_poll_reduce_func = utf_poll_reduce_maxmin_loc;
        return utofu_reduce_uint64(poll_info.utf_bg_poll_ids, (enum utofu_reduce_op)UTF_REDUCE_OP_MAXLOC,
                                   idata, (size_t)(count*2), 0);
    }else{
        /* sm (soft barrier) */
        utf_bg_poll_reduce_func = utf_poll_reduce_uint64_sm;
        UTF_BG_INIT_SM(utf_bg_grp, UTF_REDUCE_OP_MAXLOC, count*2);
        return UTF_SUCCESS;
    }
}


/**
 *
 * utf_broadcast
 * Starts the reduction operation of utf_broadcast.
 *
 * utf_bg_grp (IN)  A pointer to the structure that stores the VBG information.
 * buf        (IN)  starting address of buffer
 * size       (IN)  Number of bytes to send (number of elements x size of data type)
 * desc       (IN)  desc address(unuesd)
 * root       (IN)  rank of broadcast root
 *
 */
int utf_broadcast(utf_coll_group_t group_struct, void *buf, size_t size, void *desc, int root)
{
    utf_coll_group_detail_t *utf_bg_grp = (utf_coll_group_detail_t *)group_struct;
    uint64_t *idata = poll_info.utf_bg_poll_idata;
    int num_count;

    /* バリア適用確認 */
    if((size_t)UTF_BG_REDUCE_ULMT_ELMS_48 < size){
        return UTF_ERR_NOT_AVAILABLE;
    }

    /* countの調整(切り上げ計算) */
    num_count = ((int)size + (sizeof(uint64_t) - 1)) / sizeof(uint64_t);

    /* tmpバッファへ格納 */
    if(utf_bg_grp->arg_info.my_index == root){
        memcpy(idata, buf, size);
    }else{
        memset(idata, 0, size);
    }

    /* 情報を退避 */
    UTF_BG_REDUCE_INFO_SET(buf, UTF_BG_BCAST, num_count, (uint64_t)size,
                           (utf_bg_grp->arg_info.my_index == root));

    /* algorithm check */
    if(utf_bg_intra_node_barrier_is_tofu || 1 == utf_bg_grp->intra_node_info->size){
        /* tofu (hard barrier) */
        poll_info.utf_bg_poll_ids = *(utf_bg_grp->vbg_ids);
        utf_bg_poll_reduce_func = utf_poll_bcast;
        return utofu_reduce_uint64(poll_info.utf_bg_poll_ids,
                                   (enum utofu_reduce_op)UTF_REDUCE_OP_BOR, idata, (size_t)num_count, 0);
    }else{
        /* sm (soft barrier) */
        utf_bg_poll_reduce_func = utf_poll_reduce_uint64_sm;
        UTF_BG_INIT_SM(utf_bg_grp, UTF_REDUCE_OP_BOR, num_count);
        return UTF_SUCCESS;
    }
}


/**
 *
 * utf_bg_reduce_base
 * 演算種別とデータ型から実施するリダクション演算を決定し対応した関数を呼び出します。
 *
 * utf_bg_grp (IN)  A pointer to the structure that stores the VBG information.
 * buf        (IN)  starting address of buffer
 * count      (IN)  number of elements in send buffer
 * result     (IN)  address of receive buffer
 * datatype   (IN)  data type of elements in send buffer
 * op         (IN)  reduce operation
 * root       (IN)  trueの場合に結果を受信バッファへ返すためのフラグ
 */
static int utf_bg_reduce_base(utf_coll_group_detail_t *utf_bg_grp,
                              const void *buf,
                              int count,
                              void *result,
                              enum utf_datatype datatype,
                              enum utf_reduce_op op,
                              bool reduce_root)
{
    size_t datatype_div = datatype >> 24;
    bool is_signed;

    /* Check the arguments. */
    if(UTF_IN_PLACE == buf){
        buf = result;
    }

    /* 演算、データタイプから実施するリダクション演算を決める */
    switch((int)op){
        case UTF_REDUCE_OP_SUM:
            if(datatype_div == UTF_DATATYPE_DIV_REAL){
                /* 実数型処理  */

                /* 要素数の確認 */
                if(UTF_BG_REDUCE_ULMT_ELMS_3 < count){
                    return UTF_ERR_NOT_AVAILABLE;
                }
                /* 情報を退避 */
                UTF_BG_REDUCE_INFO_SET(result, op, count, datatype, reduce_root);
                return utf_bg_reduce_double(utf_bg_grp, buf, count,
                                            (size_t)(datatype & UTF_DATATYPE_SIZE));
            }else if(datatype_div == UTF_DATATYPE_DIV_INT){
                /* 整数型処理 */

                /* 要素数の確認 */
                if(UTF_BG_REDUCE_ULMT_ELMS_6 < count){
                    return UTF_ERR_NOT_AVAILABLE;
                }
                /* 情報を退避 */
                UTF_BG_REDUCE_INFO_SET(result, op, count, datatype, reduce_root);
                return utf_bg_reduce_uint64(utf_bg_grp, buf, count,
                                            (size_t)(datatype & UTF_DATATYPE_SIZE));
            }else if(datatype_div == UTF_DATATYPE_DIV_COMP){
                /* 複素数処理 */

                /* 要素数の確認 */
                if (UTF_BG_REDUCE_ULMT_ELMS_1 < count){
                    return UTF_ERR_NOT_AVAILABLE;
                }
                /* 情報を退避 */
                UTF_BG_REDUCE_INFO_SET(result, op, count, datatype, reduce_root);
                utf_bg_poll_reduce_func = utf_poll_reduce_comp;

                /* 実部と虚部をそれぞれ１要素ずつ処理するためcountとsizeを調整 */
                return utf_bg_reduce_double(utf_bg_grp, buf, 2,
                                            (size_t)(datatype & UTF_DATATYPE_SIZE)/2);
            }
            break;
        case UTF_REDUCE_OP_MAX:
        case UTF_REDUCE_OP_MIN:
            /* 要素数の確認 */
            if(UTF_BG_REDUCE_ULMT_ELMS_6 < count){
                return UTF_ERR_FATAL;
            }
            if(datatype_div == UTF_DATATYPE_DIV_REAL){
                /* 実数型処理  */

                /* 情報を退避 */
                UTF_BG_REDUCE_INFO_SET(result, op, count, datatype, reduce_root);

                return utf_bg_reduce_double_max_min(utf_bg_grp, buf, count,
                                                    (size_t)(datatype & UTF_DATATYPE_SIZE), op);
            }else if(datatype_div == UTF_DATATYPE_DIV_INT){
                /* 整数型処理 */

                /* 符号有無 */
                is_signed = datatype >>16;

                /* 情報を退避 */
                UTF_BG_REDUCE_INFO_SET(result, op, count, datatype, reduce_root);

                return utf_bg_reduce_uint64_max_min(utf_bg_grp, buf, count,
                                                    (size_t)(datatype & UTF_DATATYPE_SIZE), op, is_signed);
            }
            break;
        case UTF_REDUCE_OP_LAND:
        case UTF_REDUCE_OP_LOR:
        case UTF_REDUCE_OP_LXOR:
            /* 要素数の確認 */
            if (UTF_BG_REDUCE_ULMT_ELMS_384 < count){
                return UTF_ERR_NOT_AVAILABLE;
            }
            /* 情報を退避 */
            UTF_BG_REDUCE_INFO_SET(result, op, count, datatype, reduce_root);

            return utf_bg_reduce_logical(utf_bg_grp, buf, count,
                                         (size_t)(datatype & UTF_DATATYPE_SIZE), op);
        case UTF_REDUCE_OP_BAND:
        case UTF_REDUCE_OP_BOR:
        case UTF_REDUCE_OP_BXOR:
            /* 要素数の確認 */
            if(UTF_BG_REDUCE_ULMT_ELMS_48 < (int)(datatype & UTF_DATATYPE_SIZE) * count){
                return UTF_ERR_NOT_AVAILABLE;
            }
            /* 情報を退避 */
            UTF_BG_REDUCE_INFO_SET(result, op, count, datatype, reduce_root);

            return utf_bg_reduce_bitwise(utf_bg_grp, buf, count, (size_t)(datatype & UTF_DATATYPE_SIZE), op);
        case UTF_REDUCE_OP_MAXLOC:
        case UTF_REDUCE_OP_MINLOC:
            /* 要素数の確認 */
            if(UTF_BG_REDUCE_ULMT_ELMS_3 < count){
                return UTF_ERR_NOT_AVAILABLE;
            }
            if(datatype_div == UTF_DATATYPE_DIV_PAIR){
                /* 情報を退避 */
                UTF_BG_REDUCE_INFO_SET(result, op, count, datatype, reduce_root);

                return utf_bg_reduce_maxmin_loc(utf_bg_grp, buf, count, (size_t)(datatype & UTF_DATATYPE_SIZE), op);
            }else{
                return UTF_ERR_NOT_AVAILABLE;
            }
    }
    return UTF_ERR_NOT_AVAILABLE;
}


/**
 *
 * utf_allreduce
 * utf_allreduce()のリダクション演算を開始します。
 *
 * group_struct (IN)  A pointer to the structure that stores the VBG information.
 * buf          (IN)  starting address of buffer
 * count        (IN)  number of elements in send buffer
 * desc         (IN)  libfabric関数から引き継がれてくる変数
 * result       (IN)  address of receive buffer
 * result_desc  (IN)  libfabric関数から引き継がれてくる変数
 * datatype     (IN)  data type of elements in send buffer
 * op           (IN)  reduce operation
 */
int utf_allreduce(utf_coll_group_t group_struct,
                  const void *buf,
                  size_t count,
                  void *desc,
                  void *result,
                  void *result_desc,
                  enum utf_datatype datatype,
                  enum utf_reduce_op op)
{
    return utf_bg_reduce_base((utf_coll_group_detail_t *)group_struct,
                              buf, count, result, datatype, op,
                              true);
}


/**
 *
 * utf_reduce
 * utf_reduce()のリダクション演算を開始します。
 *
 * group_struct (IN)  A pointer to the structure that stores the VBG information.
 * buf          (IN)  starting address of buffer
 * count        (IN)  number of elements in send buffer
 * desc         (IN)  libfabric関数から引き継がれてくる変数
 * result       (IN)  address of receive buffer
 * result_desc  (IN)  libfabric関数から引き継がれてくる変数
 * datatype     (IN)  data type of elements in send buffer
 * op           (IN)  reduce operation
 * root         (IN)  rankset配列中のルートプロセスの位置
 */
int utf_reduce(utf_coll_group_t group_struct,
               const void *buf,
               size_t count,
               void *desc,
               void *result,
               void *result_desc,
               enum utf_datatype datatype,
               enum utf_reduce_op op,
               int root)
{
    return utf_bg_reduce_base((utf_coll_group_detail_t *)group_struct,
                              buf, count, result, datatype, op,
                              (((utf_coll_group_detail_t *)group_struct)->arg_info.my_index == root));
}


/**
 *
 * utf_poll_reduce
 * リダクション演算の完了を確認します。
 *
 * group_struct (IN)   A pointer to the structure that stores the VBG information.
 * data         (OUT)  address of receive buffer
 */
int utf_poll_reduce(utf_coll_group_t group_struct, void **data)
{
    return utf_bg_poll_reduce_func(data);
}
