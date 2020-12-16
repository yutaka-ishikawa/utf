/*
 * Copyright (c) 2020 RIKEN, Japan.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

/**
 * @file
 *
 * Definitions of the interfaces for utf barrier communication.
 * These perform high-speed collective communication by using
 * the barrier gate functions that uTofu library provides.
 * MPI_Barrier, MPI_Bcast, MPI_Reduce, or MPI_Allreduce can be
 * implemented using these.
 */

#ifndef UTF_BG_COLL_H
#define UTF_BG_COLL_H

#include <utofu.h>

/* UTF_IN_PLACE */
#define UTF_IN_PLACE  (void *) -1

/** Operations used in reduction */
enum utf_reduce_op {
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

/** Data type used in reduction */
enum utf_datatype {
    /* Unsigned integer */
    UTF_DATATYPE_UINT8_T         = 0x00000001,
    UTF_DATATYPE_UINT16_T        = 0x00000002,
    UTF_DATATYPE_UINT32_T        = 0x00000004,
    UTF_DATATYPE_UINT64_T        = 0x00000008,
    /* Signed integer */
    UTF_DATATYPE_INT8_T          = 0x00010001,
    UTF_DATATYPE_INT16_T         = 0x00010002,
    UTF_DATATYPE_INT32_T         = 0x00010004,
    UTF_DATATYPE_INT64_T         = 0x00010008,
    /* Real number */
    UTF_DATATYPE_FLOAT16         = 0x01000002,
    UTF_DATATYPE_FLOAT           = 0x01000004,
    UTF_DATATYPE_DOUBLE          = 0x01000008,
    /* Complex number */
    UTF_DATATYPE_FLOAT16_COMPLEX = 0x02000004,
    UTF_DATATYPE_FLOAT_COMPLEX   = 0x02000008,
    UTF_DATATYPE_DOUBLE_COMPLEX  = 0x02000010,
    /* Specially for MPI_MAXLOC, MPI_MINLOC */
    UTF_DATATYPE_SHORT_INT       = 0x04010006,
    UTF_DATATYPE_2INT            = 0x04010008,
    UTF_DATATYPE_LONG_INT        = 0x0401000C
};

/** Communication method used for intra-node communication */
enum utf_intra_node_comm {
    UTF_BG_TOFU, /* Tofu */
    UTF_BG_SM    /* Shared memory */
};

/** For a barrier gate setting information */
typedef uint64_t utf_bg_info_t;

/** Pointer to the VBG group settings used
    for each barrier network ( == for each communicator). */
typedef void * utf_coll_group_t;


/*
 ** Basic functions using barrier gates
 */
/* Allocation of a barrier network */
extern int utf_bg_alloc(uint32_t rankset[],
                        size_t len,
                        size_t my_index,
                        int max_ppn,
                        int comm_type,
                        utf_bg_info_t *bginfo);

/* Initialization of a barrier network  */
extern int utf_bg_init(uint32_t rankset[],
                       size_t len,
                       size_t my_index,
                       utf_bg_info_t bginfo[],
                       utf_coll_group_t *group_struct);

/* Free of a barrier network */
extern int utf_bg_free(utf_coll_group_t group_struct);


/*
 ** Collective communication functions using barrier gates
 */
/* Barrier */
extern int utf_barrier(utf_coll_group_t group_struct);

/* Broadcast */
extern int utf_broadcast(utf_coll_group_t group_struct,
                         void *buf,
                         size_t size,
                         void *desc,
                         int root);

/* Allreduce */
extern int utf_allreduce(utf_coll_group_t group_struct,
                         const void *buf,
                         size_t count,
                         void *desc,
                         void *result,
                         void *result_desc,
                         enum utf_datatype datatype,
                         enum utf_reduce_op op);

/* Reduce */
extern int utf_reduce(utf_coll_group_t,
                      const void *buf,
                      size_t count,
                      void *desc,
                      void *result,
                      void *result_desc,
                      enum utf_datatype datatype,
                      enum utf_reduce_op op,
                      int root);

/* Polling a barrier completion */
extern int utf_poll_barrier(utf_coll_group_t group_struct);

/* Polling a reduce completion */
extern int utf_poll_reduce(utf_coll_group_t group_struct,
                           void **data);


#endif /* UTF_BG_COLL_H */
