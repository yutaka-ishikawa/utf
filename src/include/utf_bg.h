/*
 * Copyright (c) 2020 XXXXXXXXXXXXXXXXXXXXXXXX.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

/**
 * @file
 *
 * utf集団通信機能を実装するためのインターフェイス.
 *
 * uTofu が提供するバリアゲート機能を用いた
 * MPI_Barrier, MPI_Bcast, MPI_Reduce, and MPI_Allreduceを行う。
 */

#ifndef UTF_BG_COLL_H
#define UTF_BG_COLL_H

#include <utofu.h>

/** 復帰値 */
enum utf_return_code {
     UTF_SUCCESS              = UTOFU_SUCCESS,

     UTF_ERR_NOT_FOUND        = UTOFU_ERR_NOT_FOUND,
     UTF_ERR_NOT_COMPLETED    = UTOFU_ERR_NOT_COMPLETED,
     UTF_ERR_NOT_PROCESSED    = UTOFU_ERR_NOT_PROCESSED,
     UTF_ERR_BUSY             = UTOFU_ERR_BUSY,
     UTF_ERR_USED             = UTOFU_ERR_USED,
     UTF_ERR_FULL             = UTOFU_ERR_FULL,
     UTF_ERR_NOT_AVAILABLE    = UTOFU_ERR_NOT_AVAILABLE,
     UTF_ERR_NOT_SUPPORTED    = UTOFU_ERR_NOT_SUPPORTED,

     UTF_ERR_TCQ_OTHER        = UTOFU_ERR_TCQ_OTHER,
     UTF_ERR_TCQ_DESC         = UTOFU_ERR_TCQ_DESC,
     UTF_ERR_TCQ_MEMORY       = UTOFU_ERR_TCQ_MEMORY,
     UTF_ERR_TCQ_STADD        = UTOFU_ERR_TCQ_STADD,
     UTF_ERR_TCQ_LENGTH       = UTOFU_ERR_TCQ_LENGTH,

     UTF_ERR_MRQ_OTHER        = UTOFU_ERR_MRQ_OTHER,
     UTF_ERR_MRQ_PEER         = UTOFU_ERR_MRQ_PEER,
     UTF_ERR_MRQ_LCL_MEMORY   = UTOFU_ERR_MRQ_LCL_MEMORY,
     UTF_ERR_MRQ_RMT_MEMORY   = UTOFU_ERR_MRQ_RMT_MEMORY,
     UTF_ERR_MRQ_LCL_STADD    = UTOFU_ERR_MRQ_LCL_STADD,
     UTF_ERR_MRQ_RMT_STADD    = UTOFU_ERR_MRQ_RMT_STADD,
     UTF_ERR_MRQ_LCL_LENGTH   = UTOFU_ERR_MRQ_LCL_LENGTH,
     UTF_ERR_MRQ_RMT_LENGTH   = UTOFU_ERR_MRQ_RMT_LENGTH,

     UTF_ERR_BARRIER_OTHER    = UTOFU_ERR_BARRIER_OTHER,
     UTF_ERR_BARRIER_MISMATCH = UTOFU_ERR_BARRIER_MISMATCH,

     UTF_ERR_INVALID_ARG      = UTOFU_ERR_INVALID_ARG,
     UTF_ERR_INVALID_POINTER  = UTOFU_ERR_INVALID_POINTER,
     UTF_ERR_INVALID_FLAGS    = UTOFU_ERR_INVALID_FLAGS,
     UTF_ERR_INVALID_COORDS   = UTOFU_ERR_INVALID_COORDS,
     UTF_ERR_INVALID_PATH     = UTOFU_ERR_INVALID_PATH,
     UTF_ERR_INVALID_TNI_ID   = UTOFU_ERR_INVALID_TNI_ID,
     UTF_ERR_INVALID_CQ_ID    = UTOFU_ERR_INVALID_CQ_ID,
     UTF_ERR_INVALID_BG_ID    = UTOFU_ERR_INVALID_BG_ID,
     UTF_ERR_INVALID_CMP_ID   = UTOFU_ERR_INVALID_CMP_ID,
     UTF_ERR_INVALID_VCQ_HDL  = UTOFU_ERR_INVALID_VCQ_HDL,
     UTF_ERR_INVALID_VCQ_ID   = UTOFU_ERR_INVALID_VCQ_ID,
     UTF_ERR_INVALID_VBG_ID   = UTOFU_ERR_INVALID_VBG_ID,
     UTF_ERR_INVALID_PATH_ID  = UTOFU_ERR_INVALID_PATH_ID,
     UTF_ERR_INVALID_STADD    = UTOFU_ERR_INVALID_STADD,
     UTF_ERR_INVALID_ADDRESS  = UTOFU_ERR_INVALID_ADDRESS,
     UTF_ERR_INVALID_SIZE     = UTOFU_ERR_INVALID_SIZE,
     UTF_ERR_INVALID_STAG     = UTOFU_ERR_INVALID_STAG,
     UTF_ERR_INVALID_EDATA    = UTOFU_ERR_INVALID_EDATA,
     UTF_ERR_INVALID_NUMBER   = UTOFU_ERR_INVALID_NUMBER,
     UTF_ERR_INVALID_OP       = UTOFU_ERR_INVALID_OP,
     UTF_ERR_INVALID_DESC     = UTOFU_ERR_INVALID_DESC,
     UTF_ERR_INVALID_DATA     = UTOFU_ERR_INVALID_DATA,

     UTF_ERR_OUT_OF_RESOURCE  = UTOFU_ERR_OUT_OF_RESOURCE,
     UTF_ERR_OUT_OF_MEMORY    = UTOFU_ERR_OUT_OF_MEMORY,

     UTF_ERR_FATAL            = UTOFU_ERR_FATAL,

     UTF_ERR_RESOURCE_BUSY    = -1024,
     UTF_ERR_INTERNAL         = -1025
};

/* UTF_IN_PLACE */
#define UTF_IN_PLACE  (void *) -1

/** リダクション演算の種別 */
enum utf_reduce_op{
    UTF_REDUCE_OP_BARRIER = UTOFU_REDUCE_OP_BARRIER,
    UTF_REDUCE_OP_BAND    = UTOFU_REDUCE_OP_BAND,
    UTF_REDUCE_OP_BOR     = UTOFU_REDUCE_OP_BOR,
    UTF_REDUCE_OP_BXOR    = UTOFU_REDUCE_OP_BXOR,
    UTF_REDUCE_OP_MAX     = UTOFU_REDUCE_OP_MAX,
    UTF_REDUCE_OP_MAXLOC  = UTOFU_REDUCE_OP_MAXLOC,
    UTF_REDUCE_OP_SUM     = UTOFU_REDUCE_OP_SUM,
    UTF_REDUCE_OP_BFPSUM  = UTOFU_REDUCE_OP_BFPSUM,

    UTF_REDUCE_OP_LAND    = 256,
    UTF_REDUCE_OP_LOR,
    UTF_REDUCE_OP_LXOR,
    UTF_REDUCE_OP_MIN,
    UTF_REDUCE_OP_MINLOC
};

/** リダクション演算のデータ型 */
enum utf_datatype{
    /* 符号無整数 */
    UTF_DATATYPE_UINT8_T         = 0x00000001,
    UTF_DATATYPE_UINT16_T        = 0x00000002,
    UTF_DATATYPE_UINT32_T        = 0x00000004,
    UTF_DATATYPE_UINT64_T        = 0x00000008,
    /* 符号有整数 */
    UTF_DATATYPE_INT8_T          = 0x00010001,
    UTF_DATATYPE_INT16_T         = 0x00010002,
    UTF_DATATYPE_INT32_T         = 0x00010004,
    UTF_DATATYPE_INT64_T         = 0x00010008,
    /* 実数 */
    UTF_DATATYPE_FLOAT16         = 0x01000002,
    UTF_DATATYPE_FLOAT           = 0x01000004,
    UTF_DATATYPE_DOUBLE          = 0x01000008,
    /* 複素数 */
    UTF_DATATYPE_FLOAT16_COMPLEX = 0x02000004,
    UTF_DATATYPE_FLOAT_COMPLEX   = 0x02000008,
    UTF_DATATYPE_DOUBLE_COMPLEX  = 0x02000010,
    /* MAXLOC, MINLOC*/
    UTF_DATATYPE_SHORT_INT       = 0x04010006,
    UTF_DATATYPE_2INT            = 0x04010008,
    UTF_DATATYPE_LONG_INT        = 0x0401000C
};

/** BG(バリアゲート情報)格納用 */
typedef uint64_t utf_bg_info_t;

/** バリア回路(コミュニケータ)毎のVBGの設定情報へのポインタ */
typedef void * utf_coll_group_t;


/*
 ** バリアゲートを用いた基本機能
 */
/* バリア回路を確保する */
extern int utf_bg_alloc(utofu_vcq_id_t rankset[],
                        size_t len,
                        size_t my_index,
                        utf_bg_info_t *bginfo);

/* バリア回路を設定する */
extern int utf_bg_init(utofu_vcq_id_t rankset[],
                       size_t len,
                       size_t my_index,
                       utf_bg_info_t bginfo[],
                       utf_coll_group_t *group_struct);

/* バリア回路を解放する */
extern int utf_bg_free(utf_coll_group_t group_struct);


/*
 ** utf集団通信機能
 */
/* barrier */
extern int utf_barrier(utf_coll_group_t group_struct);

/* broadcast */
extern int utf_broadcast(utf_coll_group_t group_struct,
                         void *buf,
                         size_t size,
                         void *desc,
                         int root);

/* allreduce */
extern int utf_allreduce(utf_coll_group_t group_struct,
                         const void *buf,
                         size_t count,
                         void *desc,
                         void *result,
                         void *result_desc,
                         enum utf_datatype datatype,
                         enum utf_reduce_op op);

/* reduce */
extern int utf_reduce(utf_coll_group_t,
                      const void *buf,
                      size_t count,
                      void *desc,
                      void *result,
                      void *result_desc,
                      enum utf_datatype datatype,
                      enum utf_reduce_op op,
                      int root);

/* poll barrier */
extern int utf_poll_barrier(utf_coll_group_t group_struct);

/* poll reduce */
extern int utf_poll_reduce(utf_coll_group_t group_struct,
                           void **data);


#endif /* UTF_BG_COLL_H */
