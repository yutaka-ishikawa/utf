/*
 * Copyright (C) 2020 RIKEN, Japan. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include "utf_bg_internal.h"
#include "utf_bg_barrier.h"

#if defined(DEBUGLOG2)
void          *utf_bg_poll_grp_addr;
#endif

utf_bg_poll_info_t poll_info;

int            utf_bg_poll_barrier_func;
int           (*utf_bg_poll_reduce_func) (void **);

enum utf_bg_poll_barrier_index {
    UTF_POLL_BARRIER,
    UTF_POLL_BARRIER_SM
};

#define UTF_BG_ALGORITHM                                                           \
     (utf_bg_intra_node_barrier_is_tofu || 1 == utf_bg_grp->intra_node_info->size)

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

#if defined(DEBUGLOG2)
    /* Check the arguments. */
    assert(group_struct != NULL);
    utf_bg_poll_grp_addr = group_struct;
#endif
    /* Check the algorithm. */
    if(UTF_BG_ALGORITHM){
        /* tofu (hard barrier) */
        poll_info.utf_bg_poll_ids = *(utf_bg_grp->vbg_ids);
        utf_bg_poll_barrier_func = UTF_POLL_BARRIER;
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s:vbg_ids=%016lx\n",
                __func__, poll_info.utf_bg_poll_ids);
#endif
        return utofu_barrier(poll_info.utf_bg_poll_ids, 0);
    }else{
        /* sm (soft barrier) */
        utf_bg_poll_barrier_func = UTF_POLL_BARRIER_SM;
        UTF_BG_INIT_SM_COMMON(utf_bg_grp);
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
#if defined(DEBUGLOG2)
    assert(group_struct == utf_bg_poll_grp_addr);
#endif
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


/**
 *
 * utf_poll_reduce_double
 * Confirm the completion of the reduction operation(SUM:double).
 *
 * data         (OUT)  address of receive buffer
 */
static int utf_poll_reduce_double(void **data)
{
    int rc;
    size_t i;

    if(UTOFU_SUCCESS !=
       (rc = utofu_poll_reduce_double(poll_info.utf_bg_poll_ids,
                                      0,
                                      (double *)poll_info.utf_bg_poll_odata))){
        return rc;
    }
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s:vbg_ids=%016lx count=%zu odata=%lf %lf %lf\n",
                __func__, poll_info.utf_bg_poll_ids, poll_info.utf_bg_poll_count,
                *((double *)poll_info.utf_bg_poll_odata), *((double *)poll_info.utf_bg_poll_odata+1),
                *((double *)poll_info.utf_bg_poll_odata+2));
#endif
    /* Set the reduction results(SUM:double) */
    UTF_BG_SET_RESULT_DOUBLE();

    return UTF_SUCCESS;
}


/**
 *
 * utf_poll_reduce_float
 * Confirm the completion of the reduction operation(SUM:float).
 *
 * data         (OUT)  address of receive buffer
 */
static int utf_poll_reduce_float(void **data)
{
    int rc;
    size_t i;

    if(UTOFU_SUCCESS !=
       (rc = utofu_poll_reduce_double(poll_info.utf_bg_poll_ids,
                                      0,
                                      (double *)poll_info.utf_bg_poll_odata))){
        return rc;
    }
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s:vbg_ids=%016lx count=%zu odata=%lf %lf %lf\n",
                __func__, poll_info.utf_bg_poll_ids, poll_info.utf_bg_poll_count,
                *((double *)poll_info.utf_bg_poll_odata),
                *((double *)poll_info.utf_bg_poll_odata+1),
                *((double *)poll_info.utf_bg_poll_odata+2));
#endif
    /* Set the reduction results(SUM:float) */
    UTF_BG_SET_RESULT_FLOAT();

    return UTF_SUCCESS;
}


#if defined(__clang__)
/**
 *
 * utf_poll_reduce_float16
 * Confirm the completion of the reduction operation(SUM:_Float16).
 *
 * data         (OUT)  address of receive buffer
 */
static int utf_poll_reduce_float16(void **data)
{
    int rc;
    size_t i;

    if(UTOFU_SUCCESS !=
       (rc = utofu_poll_reduce_double(poll_info.utf_bg_poll_ids,
                                      0,
                                      (double *)poll_info.utf_bg_poll_odata))){
        return rc;
    }
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s:vbg_ids=%016lx count=%zu odata=%lf %lf %lf\n",
                __func__, poll_info.utf_bg_poll_ids, poll_info.utf_bg_poll_count,
                *((double *)poll_info.utf_bg_poll_odata),
                *((double *)poll_info.utf_bg_poll_odata+1),
                *((double *)poll_info.utf_bg_poll_odata+2));
#endif
    /* Set the reduction results(SUM:_Float16) */
    UTF_BG_SET_RESULT_FLOAT16();

    return UTF_SUCCESS;
}
#endif


/**
 *
 * utf_poll_reduce_uint64
 * Confirm the completion of the reduction operation(SUM:integer).
 *
 * data         (OUT)  address of receive buffer
 */
static int utf_poll_reduce_uint64(void **data)
{
    int rc;
    size_t i;

    if(UTOFU_SUCCESS !=
       (rc = utofu_poll_reduce_uint64(poll_info.utf_bg_poll_ids,
                                      0,
                                      (uint64_t *)poll_info.utf_bg_poll_odata))){
        return rc;
    }
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s:vbg_ids=%016lx count=%zu type=0x%08lx odata=%lx %lx %lx %lx %lx %lx\n",
                __func__, poll_info.utf_bg_poll_ids, poll_info.utf_bg_poll_count, poll_info.utf_bg_poll_datatype,
                *((uint64_t *)poll_info.utf_bg_poll_odata), *((uint64_t *)poll_info.utf_bg_poll_odata+1),
                *((uint64_t *)poll_info.utf_bg_poll_odata+2), *((uint64_t *)poll_info.utf_bg_poll_odata+3),
                *((uint64_t *)poll_info.utf_bg_poll_odata+4), *((uint64_t *)poll_info.utf_bg_poll_odata+5));
#endif
    /* Set the reduction results(SUM:uint64_t) */
    UTF_BG_SET_RESULT_UINT64();

    return UTF_SUCCESS;
}


/**
 *
 * utf_poll_reduce_double_max_min
 * Confirm the completion of the reduction operation(MAX/MIN:double).
 *
 * data         (OUT)  address of receive buffer
 */
static int utf_poll_reduce_double_max_min(void **data)
{
    int rc;
    size_t i;

    if(UTOFU_SUCCESS !=
       (rc = utofu_poll_reduce_uint64(poll_info.utf_bg_poll_ids,
                                      0,
                                      (uint64_t *)poll_info.utf_bg_poll_odata))){
        return rc;
    }
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s:vbg_ids=%016lx op=%d count=%zu type=0x%08lx odata=%lx %lx %lx %lx %lx %lx\n",
                __func__, poll_info.utf_bg_poll_ids, poll_info.utf_bg_poll_op, poll_info.utf_bg_poll_count,
                poll_info.utf_bg_poll_datatype,
                *((uint64_t *)poll_info.utf_bg_poll_odata), *((uint64_t *)poll_info.utf_bg_poll_odata+1),
                *((uint64_t *)poll_info.utf_bg_poll_odata+2), *((uint64_t *)poll_info.utf_bg_poll_odata+3),
                *((uint64_t *)poll_info.utf_bg_poll_odata+4), *((uint64_t *)poll_info.utf_bg_poll_odata+5));
#endif
    /* Set the reduction results(MAX/MIN:double) */
    UTF_BG_SET_RESULT_DOUBLE_MAX_MIN();

    return UTF_SUCCESS;
}


/**
 *
 * utf_poll_reduce_uint64_max_min
 * Confirm the completion of the reduction operation(MAX/MIN:integer).
 *
 * data         (OUT)  address of receive buffer
 */
static int utf_poll_reduce_uint64_max_min(void **data)
{
    int rc;
    size_t i;

    if(UTOFU_SUCCESS !=
       (rc = utofu_poll_reduce_uint64(poll_info.utf_bg_poll_ids,
                                      0,
                                      (uint64_t *)poll_info.utf_bg_poll_odata))){
        return rc;
    }
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s:vbg_ids=%016lx op=%d count=%zu type=0x%08lx odata=%lx %lx %lx %lx %lx %lx\n",
                __func__, poll_info.utf_bg_poll_ids, poll_info.utf_bg_poll_op, poll_info.utf_bg_poll_count,
                poll_info.utf_bg_poll_datatype,
                *((uint64_t *)poll_info.utf_bg_poll_odata), *((uint64_t *)poll_info.utf_bg_poll_odata+1),
                *((uint64_t *)poll_info.utf_bg_poll_odata+2), *((uint64_t *)poll_info.utf_bg_poll_odata+3),
                *((uint64_t *)poll_info.utf_bg_poll_odata+4), *((uint64_t *)poll_info.utf_bg_poll_odata+5));
#endif
    /* Set the reduction results(MAX/MIN:uint64_t) */
    UTF_BG_SET_RESULT_UINT64_MAX_MIN();

    return UTF_SUCCESS;
}


/**
 *
 * utf_poll_reduce_logical
 * Confirm the completion of the reduction operation(LAND/LOR/LXOR).
 *
 * data         (OUT)  address of receive buffer
 */
static int utf_poll_reduce_logical(void **data)
{
    int rc, j;
    int bit_count;
    size_t i;

    if(UTOFU_SUCCESS !=
       (rc = utofu_poll_reduce_uint64(poll_info.utf_bg_poll_ids,
                                      0,
                                      (uint64_t *)poll_info.utf_bg_poll_odata))){
        return rc;
    }
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s:vbg_ids=%016lx op=%d count=%zu type=0x%08lx odata=%lx %lx %lx %lx %lx %lx\n",
                __func__, poll_info.utf_bg_poll_ids, poll_info.utf_bg_poll_op, poll_info.utf_bg_poll_count,
                poll_info.utf_bg_poll_datatype,
                *((uint64_t *)poll_info.utf_bg_poll_odata), *((uint64_t *)poll_info.utf_bg_poll_odata+1),
                *((uint64_t *)poll_info.utf_bg_poll_odata+2), *((uint64_t *)poll_info.utf_bg_poll_odata+3),
                *((uint64_t *)poll_info.utf_bg_poll_odata+4), *((uint64_t *)poll_info.utf_bg_poll_odata+5));
#endif
    /* Set the reduction results(LAND/LOR/LXOR) */
    UTF_BG_SET_RESULT_LOGICAL();

    return UTF_SUCCESS;
}


/**
 *
 * utf_poll_reduce_bitwise
 * Confirm the completion of the reduction operation(BAND/BOR/BXOR).
 *
 * data         (OUT)  address of receive buffer
 */
static int utf_poll_reduce_bitwise(void **data)
{
    int rc;

    if(UTOFU_SUCCESS !=
       (rc = utofu_poll_reduce_uint64(poll_info.utf_bg_poll_ids,
                                      0,
                                      (uint64_t *)poll_info.utf_bg_poll_odata))){
        return rc;
    }
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s:vbg_ids=%016lx op=%d count=%zu type=0x%08lx odata=%lx %lx %lx %lx %lx %lx\n",
                __func__, poll_info.utf_bg_poll_ids, poll_info.utf_bg_poll_op, poll_info.utf_bg_poll_count,
                poll_info.utf_bg_poll_datatype,
                *((uint64_t *)poll_info.utf_bg_poll_odata), *((uint64_t *)poll_info.utf_bg_poll_odata+1),
                *((uint64_t *)poll_info.utf_bg_poll_odata+2), *((uint64_t *)poll_info.utf_bg_poll_odata+3),
                *((uint64_t *)poll_info.utf_bg_poll_odata+4), *((uint64_t *)poll_info.utf_bg_poll_odata+5));
#endif
    /* Set the reduction results(BAND/BOR/BXOR) */
    UTF_BG_SET_RESULT_BITWISE();

    return UTF_SUCCESS;
}


/**
 *
 * utf_poll_reduce_maxmin_loc
 * Confirm the completion of the reduction operation(MAXLOC/MINLOC).
 *
 * data         (OUT)  address of receive buffer
 */
static int utf_poll_reduce_maxmin_loc(void **data)
{
    int rc;
    size_t i;

    if(UTOFU_SUCCESS !=
       (rc = utofu_poll_reduce_uint64(poll_info.utf_bg_poll_ids,
                                      0,
                                      (uint64_t *)poll_info.utf_bg_poll_odata))){
        return rc;
    }
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s:vbg_ids=%016lx op=%d count=%zu type=0x%08lx odata=%lx %lx %lx %lx %lx %lx\n",
                __func__, poll_info.utf_bg_poll_ids, poll_info.utf_bg_poll_op, poll_info.utf_bg_poll_count,
                poll_info.utf_bg_poll_datatype,
                *((uint64_t *)poll_info.utf_bg_poll_odata), *((uint64_t *)poll_info.utf_bg_poll_odata+1),
                *((uint64_t *)poll_info.utf_bg_poll_odata+2), *((uint64_t *)poll_info.utf_bg_poll_odata+3),
                *((uint64_t *)poll_info.utf_bg_poll_odata+4), *((uint64_t *)poll_info.utf_bg_poll_odata+5));
#endif
    /* Set the reduction results(MAXLOC/MINLOC) */
    UTF_BG_SET_RESULT_MAXMIN_LOC();

    return UTF_SUCCESS;
}


/**
 *
 * utf_poll_bcast
 * Confirm the completion of the reduction operation(broadcast).
 *
 * data         (OUT)  address of receive buffer
 */
static int utf_poll_bcast(void **data)
{
    int rc;

    if(UTOFU_SUCCESS !=
       (rc = utofu_poll_reduce_uint64(poll_info.utf_bg_poll_ids,
                                      0,
                                      (uint64_t *)poll_info.utf_bg_poll_odata))){
        return rc;
    }
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s:vbg_ids=%016lx count:%zu type:%lu odata=%lx %lx %lx %lx %lx %lx\n",
                __func__, poll_info.utf_bg_poll_ids, poll_info.utf_bg_poll_count, poll_info.utf_bg_poll_datatype,
                *((uint64_t *)poll_info.utf_bg_poll_odata), *((uint64_t *)poll_info.utf_bg_poll_odata+1),
                *((uint64_t *)poll_info.utf_bg_poll_odata+2), *((uint64_t *)poll_info.utf_bg_poll_odata+3),
                *((uint64_t *)poll_info.utf_bg_poll_odata+4), *((uint64_t *)poll_info.utf_bg_poll_odata+5));
#endif
    /* Set the reduction results(broadcast) */
    UTF_BG_SET_RESULT_BCAST();

    return UTF_SUCCESS;
}


/**
 *
 * utf_bg_reduce_uint64
 * Performs an integer type reduction operation(SUM:integer).
 *
 * utf_bg_grp (IN)  A pointer to the structure that stores the VBG information
 * buf        (IN)  address of send buffer
 * count      (IN)  number of elements in send buffer
 * size       (IN)  data type size
 */
static inline int utf_bg_reduce_uint64(utf_coll_group_detail_t *utf_bg_grp,
                                       void *buf,
                                       size_t count,
                                       size_t size)
{
    uint64_t *idata = poll_info.utf_bg_poll_idata;
    size_t i;

    for(i=0;i<count;i++){
        /*
        ------
        memcpy
        Copy buf(user-buffer) to idata(tmp-buffer).
        ------
          size=1

            idata addr    +0  +1  +2  +3  +4  +5  +6  +7
                          +---+---+---+---+---+---+---+---+
          uint64_t idata  |   |   |   |   |   |   |   | @ |
                          +---+---+---+---+---+---+---+---+

          size=2
                          +---+---+---+---+---+---+---+---+
          uint64_t idata  |   |   |   |   |   |   | @ | @ |
                          +---+---+---+---+---+---+---+---+

          size=4
                          +---+---+---+---+---+---+---+---+
          uint64_t idata  |   |   |   |   | @ | @ | @ | @ |
                          +---+---+---+---+---+---+---+---+

        ------
        The order of memory is reversed for little-endian.
        ------

          size=1
                          +---+---+---+---+---+---+---+---+
          uint64_t idata  | @ |   |   |   |   |   |   |   |
                          +---+---+---+---+---+---+---+---+

          size=2
                          +---+---+---+---+---+---+---+---+
          uint64_t idata  | @ | @ |   |   |   |   |   |   |
                          +---+---+---+---+---+---+---+---+

          size=4
                          +---+---+---+---+---+---+---+---+
          uint64_t idata  | @ | @ | @ | @ |   |   |   |   |
                          +---+---+---+---+---+---+---+---+
        */
        *(idata + i) = (uint64_t)0;
        memcpy((char *)idata+i*sizeof(uint64_t)+sizeof(uint64_t)-size, (char *)buf+i*size, size);
    }

    /* Check the algorithm. */
    if(UTF_BG_ALGORITHM){
        /* tofu (hard barrier) */
        poll_info.utf_bg_poll_ids = *(utf_bg_grp->vbg_ids);
        utf_bg_poll_reduce_func = utf_poll_reduce_uint64;
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s:vbg_ids=%016lx count=%zu type=0x%08lx idata=%lx %lx %lx %lx %lx %lx\n",
                __func__, poll_info.utf_bg_poll_ids, poll_info.utf_bg_poll_count, poll_info.utf_bg_poll_datatype,
                *((uint64_t *)idata), *((uint64_t *)idata+1), *((uint64_t *)idata+2),
                *((uint64_t *)idata+3), *((uint64_t *)idata+4), *((uint64_t *)idata+5));
#endif
        return utofu_reduce_uint64(poll_info.utf_bg_poll_ids, UTOFU_REDUCE_OP_SUM, idata, count, 0);
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
 * Performs an floating-point types reduction operation(SUM:double).
 *
 * utf_bg_grp (IN)  A pointer to the structure that stores the VBG information.
 * buf        (IN)  address of send buffer
 * count      (IN)  number of elements in send buffer
 * size       (IN)  data type size
 */
static inline int utf_bg_reduce_double(utf_coll_group_detail_t *utf_bg_grp,
                                       void *buf,
                                       size_t count,
                                       size_t size)
{
    double *idata = poll_info.utf_bg_poll_idata;
    size_t i;

    switch((int)size){
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

    /* Check the algorithm. */
    if(UTF_BG_ALGORITHM){
        /* tofu (hard barrier) */
        poll_info.utf_bg_poll_ids = *(utf_bg_grp->vbg_ids);
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s:vbg_ids=%016lx count=%zu idata=%lf %lf %lf\n",
                __func__, poll_info.utf_bg_poll_ids, poll_info.utf_bg_poll_count, *idata, *(idata+1), *(idata+2));
#endif
        return utofu_reduce_double(poll_info.utf_bg_poll_ids, UTOFU_REDUCE_OP_BFPSUM, idata, count, 0);
    }else{
        /* sm (soft barrier) */
        utf_bg_poll_reduce_func = utf_poll_reduce_double_sm;
        UTF_BG_INIT_SM(utf_bg_grp, UTF_REDUCE_OP_BFPSUM, count);
        /* Resize for complex data type that need to adust size. */
        poll_info.utf_bg_poll_size = size;
        return UTF_SUCCESS;
    }
}


/**
 *
 * utf_bg_reduce_uint64_max_min
 * Performs an integer type reduction operation(MAX/MIN:integer).
 *
 * utf_bg_grp (IN)  A pointer to the structure that stores the VBG information.
 * buf        (IN)  address of send buffer
 * count      (IN)  number of elements in send buffer
 * size       (IN)  data type size
 * op         (IN)  reduce operation
 * is_signed  (IN)  Elements in the send buffer is sign having.
 */
static inline int utf_bg_reduce_uint64_max_min(utf_coll_group_detail_t *utf_bg_grp,
                                               void *buf,
                                               size_t count,
                                               size_t size,
                                               enum utf_reduce_op op,
                                               bool is_signed)
{
    uint64_t *idata = poll_info.utf_bg_poll_idata;
    size_t i;

    for(i=0;i<count;i++){
        memcpy((char *)idata+i*sizeof(uint64_t)+sizeof(uint64_t)-size, (char *)buf+i*size, size);

        if(is_signed){
             /* Minus number with sign */
             *((uint64_t *)idata + i) += UTF_BG_REDUCE_MASK_HB;
        }

        if(UTF_REDUCE_OP_MIN == op){
            /* Minimum value is converted into the maximum value and operates it. */
            *((uint64_t *)idata + i) = ~(*((uint64_t *)idata + i));
        }
    }

    /* Check the algorithm. */
    if(UTF_BG_ALGORITHM){
        /* tofu (hard barrier) */
        poll_info.utf_bg_poll_ids = *(utf_bg_grp->vbg_ids);
        utf_bg_poll_reduce_func = utf_poll_reduce_uint64_max_min;
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s:vbg_ids=%016lx op=%d count=%zu type=0x%08lx idata=%lx %lx %lx %lx %lx %lx\n",
                __func__, poll_info.utf_bg_poll_ids, poll_info.utf_bg_poll_op, poll_info.utf_bg_poll_count,
                poll_info.utf_bg_poll_datatype,
                *((uint64_t *)idata), *((uint64_t *)idata+1), *((uint64_t *)idata+2),
                *((uint64_t *)idata+3), *((uint64_t *)idata+4), *((uint64_t *)idata+5));
#endif
        return utofu_reduce_uint64(poll_info.utf_bg_poll_ids, UTOFU_REDUCE_OP_MAX, idata, count, 0);
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
 * Performs an floating-point types reduction operation(MAX/MIN:double).
 *
 * utf_bg_grp (IN)  A pointer to the structure that stores the VBG information.
 * buf        (IN)  address of send buffer
 * count      (IN)  number of elements in send buffer
 * size       (IN)  data type size
 * op         (IN)  reduce operation
 */
static inline int utf_bg_reduce_double_max_min(utf_coll_group_detail_t *utf_bg_grp,
                                               void *buf,
                                               size_t count,
                                               size_t size,
                                               enum utf_reduce_op op)
{
    uint64_t *idata = poll_info.utf_bg_poll_idata;
    size_t i;

    for(i=0;i<count;i++){
        memcpy((char *)idata+i*sizeof(uint64_t)+sizeof(uint64_t)-size, (char *)buf+i*size , size);

        if(op == UTF_REDUCE_OP_MAX){
            /*  Check the signed minus number. 
             *  minus number     : (1) : Perform bit inversion to invert the value.
             *  not minus number : (2) : Invert the sign.
             */
            *(idata + i) ^= (*(idata + i) & UTF_BG_REDUCE_MASK_HB)?
                            UTF_BG_REDUCE_MASK_MAX /* (1) */: UTF_BG_REDUCE_MASK_HB /* (2) */;
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

    /* Check the algorithm. */
    if(UTF_BG_ALGORITHM){
        /* tofu (hard barrier) */
        poll_info.utf_bg_poll_ids = *(utf_bg_grp->vbg_ids);
        utf_bg_poll_reduce_func = utf_poll_reduce_double_max_min;
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s:vbg_ids=%016lx op=%d count=%zu type=0x%08lx idata=%lx %lx %lx %lx %lx %lx\n",
                __func__, poll_info.utf_bg_poll_ids, poll_info.utf_bg_poll_op, poll_info.utf_bg_poll_count,
                poll_info.utf_bg_poll_datatype,
                *((uint64_t *)idata), *((uint64_t *)idata+1), *((uint64_t *)idata+2),
                *((uint64_t *)idata+3), *((uint64_t *)idata+4), *((uint64_t *)idata+5));
#endif
        return utofu_reduce_uint64(poll_info.utf_bg_poll_ids, UTOFU_REDUCE_OP_MAX, idata, count, 0);
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
 * Performs the reduction operation(LAND/LOR/LXOR).
 *
 * utf_bg_grp (IN)  A pointer to the structure that stores the VBG information.
 * buf        (IN)  starting address of buffer
 * count      (IN)  number of elements in send buffer
 * size       (IN)  data type size
 * op         (IN)  reduce operation
 */
static inline int utf_bg_reduce_logical(utf_coll_group_detail_t *utf_bg_grp,
                                        void *buf,
                                        size_t count,
                                        size_t size,
                                        enum utf_reduce_op op)
{
    uint64_t *idata = poll_info.utf_bg_poll_idata;
    int i=0, j=0;
    uint64_t conpare_buff=0;
    /* Used to convert the operation type from logical to bitwise. */
    int utf_reduce_op_type[3] = {
         UTOFU_REDUCE_OP_BAND, /* 2 */
         UTOFU_REDUCE_OP_BOR,  /* 3 */
         UTOFU_REDUCE_OP_BXOR  /* 4 */
    };

    /* Calculates the count when packed in uint64_t idata. */
    poll_info.utf_bg_poll_numcount = (count + (UTF_BG_REDUCE_ULMT_ELMS_64 -1)) / UTF_BG_REDUCE_ULMT_ELMS_64;

    /* Check the value of buf, and if it is non-zero, set up a bit.
       It is possible to pack 64 bits in each element of uint64_t type. */
   
    while(1){
        if(0 == memcmp(((char *)buf+i*size), &conpare_buff, size)){
            *(idata+j) &= ~UTF_BG_REDUCE_MASK_LB; /* 0 */
        }else{
            *(idata+j) |= UTF_BG_REDUCE_MASK_LB;  /* 1 */
        }
        if((i+1)%UTF_BG_REDUCE_ULMT_ELMS_64 == 0 && i != 0){
            j++;
        }
        if(i == ((count)-1)){
            break;
        }
        *(idata+j) <<= 1;
        i++;
    }

    /* Check the algorithm. */
    if(UTF_BG_ALGORITHM){
        /* tofu (hard barrier) */
        poll_info.utf_bg_poll_ids = *(utf_bg_grp->vbg_ids);
        utf_bg_poll_reduce_func = utf_poll_reduce_logical;
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s:vbg_ids=%016lx op=%d count=%zu type=0x%08lx idata=%lx %lx %lx %lx %lx %lx\n",
                __func__, poll_info.utf_bg_poll_ids, poll_info.utf_bg_poll_op, poll_info.utf_bg_poll_count,
                poll_info.utf_bg_poll_datatype,
                *((uint64_t *)idata), *((uint64_t *)idata+1), *((uint64_t *)idata+2),
                *((uint64_t *)idata+3), *((uint64_t *)idata+4), *((uint64_t *)idata+5));
#endif
        return utofu_reduce_uint64(poll_info.utf_bg_poll_ids,
                                   (enum utofu_reduce_op)utf_reduce_op_type[op%(UTF_REDUCE_OP_LAND-2)-2],
                                   idata, poll_info.utf_bg_poll_numcount, 0);
    }else{
        /* sm (soft barrier) */
        utf_bg_poll_reduce_func = utf_poll_reduce_uint64_sm;
        UTF_BG_INIT_SM(utf_bg_grp, utf_reduce_op_type[op%(UTF_REDUCE_OP_LAND-2)-2], poll_info.utf_bg_poll_numcount);
        return UTF_SUCCESS;
    }
}


/**
 *
 * utf_bg_reduce_bitwise
 * Performs the reduction operation(BAND/BOR/BXOR).
 *
 * utf_bg_grp (IN)  A pointer to the structure that stores the VBG information.
 * buf        (IN)  starting address of buffer
 * count      (IN)  number of elements in send buffer
 * size       (IN)  data type size
 * op         (IN)  reduce operation
 */
static inline int utf_bg_reduce_bitwise(utf_coll_group_detail_t *utf_bg_grp,
                                        void *buf,
                                        size_t count,
                                        size_t size,
                                        enum utf_reduce_op op)
{
    uint64_t *idata = poll_info.utf_bg_poll_idata;
    size_t num_count;

    /* Calculates the count when packed in uint64_t idata. */
    num_count = (count + (sizeof(uint64_t)/size - 1)) / (sizeof(uint64_t)/size);

    memcpy(idata, buf, count*size);

    /* Check the algorithm. */
    if(UTF_BG_ALGORITHM){
        /* tofu (hard barrier) */
        poll_info.utf_bg_poll_ids = *(utf_bg_grp->vbg_ids);
        utf_bg_poll_reduce_func = utf_poll_reduce_bitwise;
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s:vbg_ids=%016lx op=%d count=%zu type=0x%08lx idata=%lx %lx %lx %lx %lx %lx\n",
                __func__, poll_info.utf_bg_poll_ids, poll_info.utf_bg_poll_op, poll_info.utf_bg_poll_count,
                poll_info.utf_bg_poll_datatype,
                *((uint64_t *)idata), *((uint64_t *)idata+1), *((uint64_t *)idata+2),
                *((uint64_t *)idata+3), *((uint64_t *)idata+4), *((uint64_t *)idata+5));
#endif
        return utofu_reduce_uint64(poll_info.utf_bg_poll_ids, (enum utofu_reduce_op)op,
                                   idata, num_count, 0);
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
 * Performs the reduction operation(MAXLOC/MINLOC).
 *
 * utf_bg_grp (IN)  A pointer to the structure that stores the VBG information.
 * buf        (IN)  starting address of buffer
 * count      (IN)  number of elements in send buffer
 * size       (IN)  data type size
 * op         (IN)  reduce operation
 */
static inline int utf_bg_reduce_maxmin_loc(utf_coll_group_detail_t *utf_bg_grp,
                                           void *buf,
                                           size_t count,
                                           size_t size,
                                           enum utf_reduce_op op)
{
    uint64_t *idata = poll_info.utf_bg_poll_idata;
    size_t i;
    size_t num_count = count*2;

    /* Copy buf(user-buffer) to idata(tmp-buffer).
     *  exsample：UTF_DATATYPE_LONG_INT
     *  Process the long part and the int part separately, one element. 
     *  Consider the padding of the structure and copy it.
     *
     *         +-----------+-----------+-----------+-----------+-----------+-----------+
     *   buf   |   long    | int |  P  |   long    | int |  P  |   long    | int |  P  |
     *         +-----------------------------------------------------------+-----------+
     *          P: padding of the structure
     *       
     *          copy(long)    copy(int)
     *
     *         +-----------+-----------+-----------+-----------+-----------+-----------+
     * idata   |   long    |    int    |   long    |    int    |   long    |    int    |
     *         +-----------+-----------+-----------+-----------+-----------+-----------+
     *  adder  idata       idata+1     idata+2     idata+3     idata+4     idata+5
     *
     */
    for(i=0;i<num_count;i++){
        switch(poll_info.utf_bg_poll_datatype){
            case UTF_DATATYPE_2INT:
                memcpy((char *)idata+i*sizeof(uint64_t)+sizeof(uint64_t)-size/2, (char *)buf+i*size/2, size/2);
                break;
            case UTF_DATATYPE_LONG_INT:
                if(i%2 == 0){
                    memcpy(idata+i, (long *)buf+i, sizeof(long));
                }else{
                    memcpy((char *)idata+i*sizeof(uint64_t)+sizeof(uint64_t)-sizeof(int),
                           (char *)buf+i*sizeof(long), sizeof(int));
                }
                break;
            case UTF_DATATYPE_SHORT_INT:
                if(i%2 == 0){
                    memcpy((char *)idata+i*sizeof(uint64_t)+sizeof(uint64_t)-sizeof(short),
                           (char *)buf+i*sizeof(int), sizeof(short));
                }else{
                    memcpy((char *)idata+i*sizeof(uint64_t)+sizeof(uint64_t)-sizeof(int),
                           (char *)buf+i*sizeof(int), sizeof(int));
                }
        }

        /* Minus number with sign. */
        *(idata+i) += UTF_BG_REDUCE_MASK_HB;
    }
    if(UTF_REDUCE_OP_MINLOC == op){
        /* Minimum value is converted into the maximum value and operates it. */
        for(i=0;i<num_count;i+=2){
            *(idata + i) = ~(*(idata + i));
        }
    }

    /* Check the algorithm. */
    if(UTF_BG_ALGORITHM){
        /* tofu (hard barrier) */
        poll_info.utf_bg_poll_ids = *(utf_bg_grp->vbg_ids);
        utf_bg_poll_reduce_func = utf_poll_reduce_maxmin_loc;
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s:vbg_ids=%016lx op=%d count=%zu type=0x%08lx idata=%lx %lx %lx %lx %lx %lx\n",
                __func__, poll_info.utf_bg_poll_ids, poll_info.utf_bg_poll_op, poll_info.utf_bg_poll_count,
                poll_info.utf_bg_poll_datatype,
                *((uint64_t *)idata), *((uint64_t *)idata+1), *((uint64_t *)idata+2),
                *((uint64_t *)idata+3), *((uint64_t *)idata+4), *((uint64_t *)idata+5));
#endif
        return utofu_reduce_uint64(poll_info.utf_bg_poll_ids, UTOFU_REDUCE_OP_MAXLOC, idata, num_count, 0);
    }else{
        /* sm (soft barrier) */
        utf_bg_poll_reduce_func = utf_poll_reduce_uint64_sm;
        UTF_BG_INIT_SM(utf_bg_grp, UTF_REDUCE_OP_MAXLOC, num_count);
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
    size_t num_count;

#if defined(DEBUGLOG2)
    /* Check the arguments. */
    assert(group_struct != NULL);
    utf_bg_poll_grp_addr = group_struct;
#endif

    /* Check the arguments. */
    if((size_t)UTF_BG_REDUCE_ULMT_ELMS_48 < size){
        return UTF_ERR_NOT_AVAILABLE;
    }

    /* Calculates the count when packed in uint64_t idata. */
    num_count = (size + (sizeof(uint64_t) - 1)) / sizeof(uint64_t);

    /* Copy to tmp-buffer. */
    if(utf_bg_grp->arg_info.my_index == root){
        memcpy(idata, buf, size);
    }else{
        memset(idata, 0, size);
    }

    /* Saves information for use in the poll function. */
    UTF_BG_REDUCE_INFO_SET(buf, UTF_BG_BCAST, num_count, (uint64_t)size,
                           (utf_bg_grp->arg_info.my_index == root));

    /* Check the algorithm. */
    if(UTF_BG_ALGORITHM){
        /* tofu (hard barrier) */
        poll_info.utf_bg_poll_ids = *(utf_bg_grp->vbg_ids);
        utf_bg_poll_reduce_func = utf_poll_bcast;
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s:vbg_ids=%016lx count=%zu type=0x%08lx idata=%lx %lx %lx %lx %lx %lx\n",
                __func__, poll_info.utf_bg_poll_ids, poll_info.utf_bg_poll_count, poll_info.utf_bg_poll_datatype,
                *((uint64_t *)idata), *((uint64_t *)idata+1), *((uint64_t *)idata+2),
                *((uint64_t *)idata+3), *((uint64_t *)idata+4), *((uint64_t *)idata+5));
#endif
        return utofu_reduce_uint64(poll_info.utf_bg_poll_ids, UTOFU_REDUCE_OP_BOR, idata, num_count, 0);
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
 * Decides a reduction operation.
 *
 * utf_bg_grp  (IN)  A pointer to the structure that stores the VBG information.
 * buf         (IN)  starting address of buffer
 * count       (IN)  number of elements in send buffer
 * result      (IN)  address of receive buffer
 * datatype    (IN)  data type of elements in send buffer
 * op          (IN)  reduce operation
 * reduce_root (IN)  If true places the result in receive buffer.
 */
static inline int utf_bg_reduce_base(utf_coll_group_detail_t *utf_bg_grp,
                                     const void *buf,
                                     size_t count,
                                     void *result,
                                     enum utf_datatype datatype,
                                     enum utf_reduce_op op,
                                     bool reduce_root)
{
    size_t datatype_div = datatype >> 24;
    void *sbuf;

    /* Check the arguments. */
    if(UTF_IN_PLACE == buf){
        sbuf = result;
    }else{
        sbuf = (void *)buf;
    }

    /* Decides a reduction operation. */
    switch((int)op){
        case UTF_REDUCE_OP_SUM:
            /* Check the datatype. */
            assert(datatype_div == UTF_DATATYPE_DIV_INT ||
                   datatype_div == UTF_DATATYPE_DIV_REAL ||
                   datatype_div == UTF_DATATYPE_DIV_COMP);

            if(datatype_div == UTF_DATATYPE_DIV_REAL){
                /* The datatype is a floating-point datatype. */

                /* Check the count. */
                if(UTF_BG_REDUCE_ULMT_ELMS_3 < count){
                    return UTF_ERR_NOT_AVAILABLE;
                }
                /* Saves information for use in the poll function. */
                UTF_BG_REDUCE_INFO_SET(result, op, count, datatype, reduce_root);
                return utf_bg_reduce_double(utf_bg_grp, sbuf, count, poll_info.utf_bg_poll_size);
            }else if(datatype_div == UTF_DATATYPE_DIV_INT){
                /* The datatype is an integer datatype. */

                /* Check the count. */
                if(UTF_BG_REDUCE_ULMT_ELMS_6 < count){
                    return UTF_ERR_NOT_AVAILABLE;
                }
                /* Saves information for use in the poll function. */
                UTF_BG_REDUCE_INFO_SET(result, op, count, datatype, reduce_root);
                return utf_bg_reduce_uint64(utf_bg_grp, sbuf, count, poll_info.utf_bg_poll_size);
            }else if(datatype_div == UTF_DATATYPE_DIV_COMP){
                /* The datatype is a complex datatype. */

                /* Check the count. */
                if (UTF_BG_REDUCE_ULMT_ELMS_1 < count){
                    return UTF_ERR_NOT_AVAILABLE;
                }
                /* Saves information for use in the poll function. */
                UTF_BG_REDUCE_INFO_SET(result, op, 2, datatype, reduce_root);

                /* Adjust count and size to process the real part and 
                   the imaginary part separately for each element. */
                return utf_bg_reduce_double(utf_bg_grp, sbuf, 2, poll_info.utf_bg_poll_size/2);
            }
            break;
        case UTF_REDUCE_OP_MAX:
        case UTF_REDUCE_OP_MIN:
            /* Check the datatype. */
            assert(datatype_div == UTF_DATATYPE_DIV_INT || datatype_div == UTF_DATATYPE_DIV_REAL);

            /* Check the count. */
            if(UTF_BG_REDUCE_ULMT_ELMS_6 < count){
                return UTF_ERR_NOT_AVAILABLE;
            }
            if(datatype_div == UTF_DATATYPE_DIV_REAL){
                /* The datatype is a floating-point datatype. */

                /* Saves information for use in the poll function. */
                UTF_BG_REDUCE_INFO_SET(result, op, count, datatype, reduce_root);

                return utf_bg_reduce_double_max_min(utf_bg_grp, sbuf, count, poll_info.utf_bg_poll_size, op);
            }else if(datatype_div == UTF_DATATYPE_DIV_INT){
                /* The datatype is an integer datatype. */

                /* Saves information for use in the poll function. */
                UTF_BG_REDUCE_INFO_SET(result, op, count, datatype, reduce_root);

                /* Check if it is a signed minus number.(datatype >> 16) */
                return utf_bg_reduce_uint64_max_min(utf_bg_grp, sbuf, count,
                                                    poll_info.utf_bg_poll_size, op, datatype >> 16);
            }
            break;
        case UTF_REDUCE_OP_MAXLOC:
        case UTF_REDUCE_OP_MINLOC:
            /* Check the datatype. */
            assert(datatype_div == UTF_DATATYPE_DIV_PAIR);

            /* Check the count. */
            if(UTF_BG_REDUCE_ULMT_ELMS_3 < count){
                return UTF_ERR_NOT_AVAILABLE;
            }
            if(datatype_div == UTF_DATATYPE_DIV_PAIR){
                /* Saves information for use in the poll function. */
                UTF_BG_REDUCE_INFO_SET(result, op, count, datatype, reduce_root);

                return utf_bg_reduce_maxmin_loc(utf_bg_grp, sbuf, count, poll_info.utf_bg_poll_size, op);
            }else{
                return UTF_ERR_NOT_AVAILABLE;
            }
            break;
        case UTF_REDUCE_OP_BAND:
        case UTF_REDUCE_OP_BOR:
        case UTF_REDUCE_OP_BXOR:
            /* Check the datatype. */
            assert(datatype_div == UTF_DATATYPE_DIV_INT);

            /* Check the count. */
            if(UTF_BG_REDUCE_ULMT_ELMS_48 < (int)(datatype & UTF_DATATYPE_SIZE) * count){
                return UTF_ERR_NOT_AVAILABLE;
            }
            /* Saves information for use in the poll function. */
            UTF_BG_REDUCE_INFO_SET(result, op, count, datatype, reduce_root);

            return utf_bg_reduce_bitwise(utf_bg_grp, sbuf, count, poll_info.utf_bg_poll_size, op);
        case UTF_REDUCE_OP_LAND:
        case UTF_REDUCE_OP_LOR:
        case UTF_REDUCE_OP_LXOR:
            /* Check the datatype. */
            assert(datatype_div == UTF_DATATYPE_DIV_INT);

            /* Check the count. */
            if (UTF_BG_REDUCE_ULMT_ELMS_384 < count){
                return UTF_ERR_NOT_AVAILABLE;
            }
            /* Saves information for use in the poll function. */
            UTF_BG_REDUCE_INFO_SET(result, op, count, datatype, reduce_root);

            return utf_bg_reduce_logical(utf_bg_grp, sbuf, count, poll_info.utf_bg_poll_size, op);
    }
    return UTF_ERR_NOT_AVAILABLE;
}


/**
 *
 * utf_allreduce
 * Starts the reduction operation of utf_allreduce.
 *
 * group_struct (IN)  A pointer to the structure that stores the VBG information.
 * buf          (IN)  starting address of buffer
 * count        (IN)  number of elements in send buffer
 * desc         (IN)  A pointer variable inherited from the libfabric function(unuesd).
 * result       (IN)  address of receive buffer
 * result_desc  (IN)  A pointer variable inherited from the libfabric function(unuesd).
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
#if defined(DEBUGLOG2)
    /* Check the arguments. */
    assert(group_struct != NULL);
    utf_bg_poll_grp_addr = group_struct;
#endif
    return utf_bg_reduce_base((utf_coll_group_detail_t *)group_struct,
                              buf, count, result, datatype, op,
                              true);
}


/**
 *
 * utf_reduce
 * Starts the reduction operation of utf_reduce.
 *
 * group_struct (IN)  A pointer to the structure that stores the VBG information.
 * buf          (IN)  starting address of buffer
 * count        (IN)  number of elements in send buffer
 * desc         (IN)  A pointer variable inherited from the libfabric function(unuesd).
 * result       (IN)  address of receive buffer
 * result_desc  (IN)  A pointer variable inherited from the libfabric function(unuesd).
 * datatype     (IN)  data type of elements in send buffer
 * op           (IN)  reduce operation
 * root         (IN)  The position of the root process in the rankset array.
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
#if defined(DEBUGLOG2)
    /* Check the arguments. */
    assert(group_struct != NULL);
    utf_bg_poll_grp_addr = group_struct;
#endif
    return utf_bg_reduce_base((utf_coll_group_detail_t *)group_struct,
                              buf, count, result, datatype, op,
                              (((utf_coll_group_detail_t *)group_struct)->arg_info.my_index == root));
}


/**
 *
 * utf_poll_reduce
 * Confirm the completion of the reduction operation.
 *
 * group_struct (IN)   A pointer to the structure that stores the VBG information.
 * data         (OUT)  address of receive buffer
 */
int utf_poll_reduce(utf_coll_group_t group_struct, void **data)
{
#if defined(DEBUGLOG2)
    assert(group_struct == utf_bg_poll_grp_addr);
#endif
    return utf_bg_poll_reduce_func(data);
}
