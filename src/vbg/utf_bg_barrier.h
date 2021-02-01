/*
 * Copyright (C) 2020 RIKEN, Japan. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 */

/**
 * @file   utf_bg_barrier.h
 * @brief  Macro and structure definitions used in collective communication
 *         functions using barrier gates
 */


/** For a data type judgement in reduction */
enum utf_datatype_div{
    /* 整数 */
    UTF_DATATYPE_DIV_INT,
    /* 実数 */
    UTF_DATATYPE_DIV_REAL,
    /* 複素数 */
    UTF_DATATYPE_DIV_COMP,
    /* MAXLOC, MINLOC */
    UTF_DATATYPE_DIV_PAIR = 4
};

/**
 * save information
 *
 */
#define UTF_BG_REDUCE_INFO_SET(buf, op, count, datatype, reduce_root)     \
{                                                                         \
    poll_info.utf_bg_poll_result = (buf);                                 \
    poll_info.utf_bg_poll_op = (int)(op);                                 \
    poll_info.utf_bg_poll_count = (count);                                \
    poll_info.utf_bg_poll_datatype = (datatype);                          \
    poll_info.utf_bg_poll_root = (reduce_root);                           \
    poll_info.utf_bg_poll_size= (size_t)((datatype) & UTF_DATATYPE_SIZE); \
}

/** utf bg macro */
#define UTF_DATATYPE_SIZE  0x000000FF

/** utf_broadcast */
#define UTF_BG_BCAST       512

/** Maximum number of elements in reduction operations */
#define UTF_BG_REDUCE_ULMT_ELMS_1       1
#define UTF_BG_REDUCE_ULMT_ELMS_3       3
#define UTF_BG_REDUCE_ULMT_ELMS_6       6
#define UTF_BG_REDUCE_ULMT_ELMS_12     12
#define UTF_BG_REDUCE_ULMT_ELMS_24     24
#define UTF_BG_REDUCE_ULMT_ELMS_48     48
#define UTF_BG_REDUCE_ULMT_ELMS_64     64
#define UTF_BG_REDUCE_ULMT_ELMS_384   384

/** MAX/MIN operation mask */
#define UTF_BG_REDUCE_MASK_HB         0x8000000000000000UL
#define UTF_BG_REDUCE_MASK_LB         0x0000000000000001UL
#define UTF_BG_REDUCE_MASK_MAX        0xFFFFFFFFFFFFFFFFUL
#define UTF_BG_REDUCE_MASK_MIN        0x7FFFFFFFFFFFFFFFUL
#define UTF_BG_REDUCE_MASK_NAN_FP16   0x03FF000000000000UL
#define UTF_BG_REDUCE_MASK_NAN_FP32   0x007FFFFF00000000UL
#define UTF_BG_REDUCE_MASK_NAN_FP64   0x000FFFFFFFFFFFFFUL

/** Initialization for sm */
#define UTF_BG_INIT_SM_COMMON(p)                                          \
{                                                                         \
    poll_info.utf_bg_poll_ids =                                           \
        (UTF_BG_INTRA_NODE_MANAGER == (p)->intra_node_info->state ?       \
         *((p)->vbg_ids) : -1);                                           \
    poll_info.sm_numproc = (p)->intra_node_info->size;                    \
    poll_info.sm_intra_index = (p)->intra_node_info->intra_index;         \
    poll_info.sm_mmap_buf = (p)->intra_node_info->mmap_buf;               \
    poll_info.sm_seq_val = ++((p)->intra_node_info->curr_seq);            \
    poll_info.sm_flg_comm_start = false;                                  \
    poll_info.sm_loop_counter = 1;                                        \
    poll_info.sm_loop_rank = (p)->intra_node_info->intra_index;           \
}

/** Initialization for sm(reduce) */
#define UTF_BG_INIT_SM(p, uop, ucnt)                                      \
{                                                                         \
    UTF_BG_INIT_SM_COMMON(p);                                             \
    poll_info.sm_utofu_op = (uop);                                        \
    poll_info.utf_bg_poll_numcount = (ucnt);                              \
}

/** Sequence buffer in shared memory */
#define UTF_BG_MMAP_SEQ(x)                                                     \
    (uint64_t *)((char *)poll_info.sm_mmap_buf + (x) * UTF_BG_CACHE_LINE_SIZE)

/** Data buffer in shared memory */
#define UTF_BG_MMAP_BUF(x)                                                     \
    (UTF_BG_MMAP_SEQ(x) + UTF_BG_MMAP_NUM_SEQ)

/** Memory barreir */
#if defined(__aarch64__)
#define utf_bg_rmb()    __asm__ __volatile__ ("dmb ld" : : : "memory")
#define utf_bg_wmb()    __asm__ __volatile__ ("dmb st" : : : "memory")
#elif defined(__x86_64__)
#define utf_bg_rmb()    __asm__ __volatile__ ("" : : : "memory")
#define utf_bg_wmb()    __asm__ __volatile__ ("" : : : "memory")
#endif

/** Set the reduction results */
#define UTF_BG_SET_RESULT_COMMON()                                   \
{                                                                    \
    if(data){                                                        \
        *data = poll_info.utf_bg_poll_result;                        \
    }                                                                \
    if(!poll_info.utf_bg_poll_root){                                 \
        return UTF_SUCCESS;                                          \
    }                                                                \
}

/** Set the reduction results(SUM:double) */
#define UTF_BG_SET_RESULT_DOUBLE()                                                                    \
{                                                                                                     \
    UTF_BG_SET_RESULT_COMMON()                                                                        \
    for(i = 0; i < poll_info.utf_bg_poll_count; i++){                                                 \
        *((double *)poll_info.utf_bg_poll_result + i) = *((double *)poll_info.utf_bg_poll_odata + i); \
    }                                                                                                 \
}

/** Set the reduction results(SUM:float) */
#define UTF_BG_SET_RESULT_FLOAT()                                                                           \
{                                                                                                           \
    UTF_BG_SET_RESULT_COMMON()                                                                              \
    for(i = 0; i < poll_info.utf_bg_poll_count; i++){                                                       \
        *((float *)poll_info.utf_bg_poll_result + i) = (float)*((double *)poll_info.utf_bg_poll_odata + i); \
    }                                                                                                       \
}

/** Set the reduction results(SUM:_Float16) */
#define UTF_BG_SET_RESULT_FLOAT16()                                                                               \
{                                                                                                                 \
    UTF_BG_SET_RESULT_COMMON()                                                                                    \
    for(i = 0; i < poll_info.utf_bg_poll_count; i++){                                                             \
        *((_Float16 *)poll_info.utf_bg_poll_result + i) = (_Float16)*((double *)poll_info.utf_bg_poll_odata + i); \
    }                                                                                                             \
}

/** Set the reduction results(SUM:uint64_t) */
#define UTF_BG_SET_RESULT_UINT64()                                                                  \
{                                                                                                   \
    UTF_BG_SET_RESULT_COMMON()                                                                      \
    for(i = 0; i < poll_info.utf_bg_poll_count; i++){                                               \
        memcpy((char *)poll_info.utf_bg_poll_result + i * poll_info.utf_bg_poll_size,               \
               (char *)poll_info.utf_bg_poll_odata + i * sizeof(uint64_t) + sizeof(uint64_t) -      \
                                                                        poll_info.utf_bg_poll_size, \
               poll_info.utf_bg_poll_size);                                                         \
    }                                                                                               \
}

/** Set the reduction results(MAX/MIN:double) */
#define UTF_BG_SET_RESULT_DOUBLE_MAX_MIN()                                                          \
{                                                                                                   \
    UTF_BG_SET_RESULT_COMMON()                                                                      \
    for(i = 0; i < poll_info.utf_bg_poll_count; i++){                                               \
        switch(poll_info.utf_bg_poll_size){                                                         \
            case sizeof(double):                                                                    \
                *((uint64_t *)poll_info.utf_bg_poll_odata + i) -= UTF_BG_REDUCE_MASK_NAN_FP64;      \
                break;                                                                              \
            case sizeof(float):                                                                     \
                *((uint64_t *)poll_info.utf_bg_poll_odata + i) -= UTF_BG_REDUCE_MASK_NAN_FP32;      \
                break;                                                                              \
            case sizeof(_Float16):                                                                  \
                *((uint64_t *)poll_info.utf_bg_poll_odata + i) -= UTF_BG_REDUCE_MASK_NAN_FP16;      \
        }                                                                                           \
        if(UTF_REDUCE_OP_MAX == poll_info.utf_bg_poll_op){                                          \
            *((uint64_t *)poll_info.utf_bg_poll_odata + i) ^=                                       \
                (*((uint64_t *)poll_info.utf_bg_poll_odata + i) & UTF_BG_REDUCE_MASK_HB ?           \
                 UTF_BG_REDUCE_MASK_HB : UTF_BG_REDUCE_MASK_MAX);                                   \
        }else{                                                                                      \
            if(!(*((uint64_t *)poll_info.utf_bg_poll_odata + i) & UTF_BG_REDUCE_MASK_HB)){          \
                *((uint64_t *)poll_info.utf_bg_poll_odata + i) ^= UTF_BG_REDUCE_MASK_MIN;           \
            }                                                                                       \
        }                                                                                           \
        memcpy((char *)poll_info.utf_bg_poll_result + i * poll_info.utf_bg_poll_size,               \
               (char *)poll_info.utf_bg_poll_odata + i * sizeof(uint64_t) + sizeof(uint64_t) -      \
                                                                        poll_info.utf_bg_poll_size, \
               poll_info.utf_bg_poll_size);                                                         \
    }                                                                                               \
}

/** Set the reduction results(MAX/MIN:uint64_t) */
#define UTF_BG_SET_RESULT_UINT64_MAX_MIN()                                                                      \
{                                                                                                               \
    UTF_BG_SET_RESULT_COMMON()                                                                                  \
    for(i = 0; i < poll_info.utf_bg_poll_count; i++){                                                           \
        if(UTF_REDUCE_OP_MIN == poll_info.utf_bg_poll_op){                                                      \
            *((uint64_t *)poll_info.utf_bg_poll_odata + i) = ~(*((uint64_t *)poll_info.utf_bg_poll_odata + i)); \
        }                                                                                                       \
        if(poll_info.utf_bg_poll_datatype >> 16){                                                               \
            *((uint64_t *)poll_info.utf_bg_poll_odata + i) -= UTF_BG_REDUCE_MASK_HB;                            \
        }                                                                                                       \
        memcpy((char *)poll_info.utf_bg_poll_result + i * poll_info.utf_bg_poll_size,                           \
               (char *)poll_info.utf_bg_poll_odata + i * sizeof(uint64_t) + sizeof(uint64_t) -                  \
                                                                        poll_info.utf_bg_poll_size,             \
               poll_info.utf_bg_poll_size);                                                                     \
    }                                                                                                           \
}

/** Set the reduction results(LAND/LOR/LXOR) */
#define UTF_BG_SET_RESULT_LOGICAL()                                                                      \
{                                                                                                        \
    UTF_BG_SET_RESULT_COMMON()                                                                           \
    for(i=0, j=0; i < poll_info.utf_bg_poll_numcount; i++){                                              \
        if( UTF_BG_REDUCE_ULMT_ELMS_64 >= poll_info.utf_bg_poll_count ){                                 \
            bit_count = poll_info.utf_bg_poll_count;                                                     \
        } else {                                                                                         \
            bit_count = UTF_BG_REDUCE_ULMT_ELMS_64;                                                      \
        }                                                                                                \
        poll_info.utf_bg_poll_count -= bit_count;                                                        \
        while(bit_count != 0){                                                                           \
            if((*((uint64_t *)poll_info.utf_bg_poll_odata + i)>>(bit_count-1)) & UTF_BG_REDUCE_MASK_LB){ \
                switch (poll_info.utf_bg_poll_size){                                                     \
                    case sizeof(int64_t):                                                                \
                        *((int64_t *)poll_info.utf_bg_poll_result + j) = 1;                              \
                        break;                                                                           \
                    case sizeof(int):                                                                    \
                        *((int *)poll_info.utf_bg_poll_result + j) = 1;                                  \
                        break;                                                                           \
                    case sizeof(short):                                                                  \
                        *((short *)poll_info.utf_bg_poll_result + j) = 1;                                \
                        break;                                                                           \
                    case sizeof(char):                                                                   \
                        *((char *)poll_info.utf_bg_poll_result + j) = 1;                                 \
                }                                                                                        \
            }else{                                                                                       \
                switch (poll_info.utf_bg_poll_size){                                                     \
                    case sizeof(int64_t):                                                                \
                        *((int64_t *)poll_info.utf_bg_poll_result + j) = 0;                              \
                        break;                                                                           \
                    case sizeof(int):                                                                    \
                        *((int *)poll_info.utf_bg_poll_result + j) = 0;                                  \
                        break;                                                                           \
                    case sizeof(short):                                                                  \
                        *((short *)poll_info.utf_bg_poll_result + j) = 0;                                \
                        break;                                                                           \
                    case sizeof(char):                                                                   \
                        *((char *)poll_info.utf_bg_poll_result + j) = 0;                                 \
                }                                                                                        \
            }                                                                                            \
            bit_count--;                                                                                 \
            j++;                                                                                         \
        }                                                                                                \
    }                                                                                                    \
}

/** Set the reduction results(BAND/BOR/BXOR) */
#define UTF_BG_SET_RESULT_BITWISE()                                                       \
{                                                                                         \
    UTF_BG_SET_RESULT_COMMON()                                                            \
    memcpy(poll_info.utf_bg_poll_result, (uint64_t *)poll_info.utf_bg_poll_odata,         \
           poll_info.utf_bg_poll_count * poll_info.utf_bg_poll_size);                     \
}

/** Set the reduction results(MAXLOC/MINLOC) */
#define UTF_BG_SET_RESULT_MAXMIN_LOC()                                                                          \
{                                                                                                               \
    UTF_BG_SET_RESULT_COMMON()                                                                                  \
    if(UTF_REDUCE_OP_MINLOC == poll_info.utf_bg_poll_op){                                                       \
        for(i=0; i < poll_info.utf_bg_poll_count*2; i+=2){                                                      \
            *((uint64_t *)poll_info.utf_bg_poll_odata + i) = ~(*((uint64_t *)poll_info.utf_bg_poll_odata + i)); \
        }                                                                                                       \
    }                                                                                                           \
    for(i=0; i < poll_info.utf_bg_poll_count*2; i++){                                                           \
        *((uint64_t *)poll_info.utf_bg_poll_odata + i) -= UTF_BG_REDUCE_MASK_HB;                                \
        switch(poll_info.utf_bg_poll_datatype){                                                                 \
            case UTF_DATATYPE_2INT:                                                                             \
                  memcpy((char *)poll_info.utf_bg_poll_result + i * poll_info.utf_bg_poll_size/2,               \
                         (char *)poll_info.utf_bg_poll_odata + i * sizeof(uint64_t) + sizeof(uint64_t)          \
                                                                              - poll_info.utf_bg_poll_size/2,   \
                         poll_info.utf_bg_poll_size/2);                                                         \
                  break;                                                                                        \
            case UTF_DATATYPE_LONG_INT:                                                                         \
                if(i%2 == 0){                                                                                   \
                    memcpy((long *)poll_info.utf_bg_poll_result + i,                                            \
                           (uint64_t *)poll_info.utf_bg_poll_odata + i,                                         \
                           sizeof(long));                                                                       \
                }else{                                                                                          \
                    memcpy((char *)poll_info.utf_bg_poll_result + i * sizeof(long),                             \
                           (char *)poll_info.utf_bg_poll_odata + i * sizeof(uint64_t) + sizeof(uint64_t)        \
                                                                                             - sizeof(int),     \
                           sizeof(int));                                                                        \
                }                                                                                               \
                break;                                                                                          \
            case UTF_DATATYPE_SHORT_INT:                                                                        \
                if(i%2 == 0){                                                                                   \
                    memcpy((char *)poll_info.utf_bg_poll_result + i * sizeof(int),                              \
                           (char *)poll_info.utf_bg_poll_odata + i * sizeof(uint64_t) + sizeof(uint64_t)        \
                                                                                             - sizeof(short),   \
                         sizeof(short));                                                                        \
                }else{                                                                                          \
                    memcpy((char *)poll_info.utf_bg_poll_result + i * sizeof(int),                              \
                           (char *)poll_info.utf_bg_poll_odata + i * sizeof(uint64_t) + sizeof(uint64_t)        \
                                                                                             - sizeof(int),     \
                           sizeof(int));                                                                        \
                }                                                                                               \
        }                                                                                                       \
    }                                                                                                           \
}

/** Copy the message from the buffer of the root(broadcast) */
#define UTF_BG_SET_RESULT_BCAST()                                                         \
{                                                                                         \
    if(data){                                                                             \
        *data = poll_info.utf_bg_poll_result;                                             \
    }                                                                                     \
    if(!poll_info.utf_bg_poll_root){                                                      \
        memcpy(poll_info.utf_bg_poll_result, poll_info.utf_bg_poll_odata,                 \
               poll_info.utf_bg_poll_size);                                               \
    }                                                                                     \
}


/**
 * Internal interface declarations
 */
extern int utf_poll_barrier_sm(void);
extern int utf_poll_reduce_double_sm(void **);
extern int utf_poll_reduce_uint64_sm(void **);
