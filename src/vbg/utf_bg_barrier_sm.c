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


/**
 * SM_REDUCE_OP
 *   Performs the reduction operation in multiple processes in a node.
 * @param(IN/OUT) data1        A pointer to the operand 1 of operation.
 * @param(IN)     data2        A pointer to the operand 2 of operation.
 */
#define SM_REDUCE_OP(data_1, data_2) {                                              \
    switch(poll_info.sm_utofu_op) {                                                 \
        case UTF_REDUCE_OP_SUM:                                                     \
            for(n = 0; n < count; n++){                                             \
                *((data_1) + n) += *((data_2) + n);                                 \
            }                                                                       \
            break;                                                                  \
        case UTF_REDUCE_OP_BAND:                                                    \
            for(n = 0; n < count; n++){                                             \
                *((data_1) + n) &= *((data_2) + n);                                 \
            }                                                                       \
            break;                                                                  \
        case UTF_REDUCE_OP_BOR:                                                     \
            for(n = 0; n < count; n++){                                             \
                *((data_1) + n) |= *((data_2) + n);                                 \
            }                                                                       \
            break;                                                                  \
        case UTF_REDUCE_OP_BXOR:                                                    \
            for(n = 0; n < count; n++){                                             \
                *((data_1) + n) ^= *((data_2) + n);                                 \
            }                                                                       \
            break;                                                                  \
        case UTF_REDUCE_OP_MAX:                                                     \
            for(n = 0; n < count; n++){                                             \
                if(*((data_1) + n) < *((data_2) + n)){                              \
                    *((data_1) + n) = *((data_2) + n);                              \
                }                                                                   \
            }                                                                       \
            break;                                                                  \
        case UTF_REDUCE_OP_MAXLOC:                                                  \
            for(n = 0; n < count; n+=2){                                            \
                if(*((data_1) + n) < *((data_2) + n)){                              \
                    *((data_1) + n) = *((data_2) + n);                              \
                    *((data_1) + n + 1) = *((data_2) + n + 1);                      \
                }                                                                   \
                else if(*((data_1) + n) == *((data_2) + n) &&                       \
                        *((data_1) + n + 1) > *((data_2) + n + 1)){                 \
                    *((data_1) + n + 1) = *((data_2) + n + 1);                      \
                }                                                                   \
            }                                                                       \
            break;                                                                  \
        default:                                                                    \
            utf_printf("%s(SM_REDUCE_OP): Invalid reduction operation:"             \
                       " intra_index=%zu op=%d\n",                                  \
                       __func__, poll_info.sm_intra_index, poll_info.sm_utofu_op);  \
            return UTF_ERR_INTERNAL;                                                \
    }                                                                               \
}


/**
 * utf_poll_barrier_sm
 *   In the soft barrier process, the leader process do the barrier
 *   communication.
 * @param  none.
 * @return UTF_SUCCESS, UTF_ERR_NOT_COMPLETED, or other return codes of uTofu
 *         functions.
 */
int utf_poll_barrier_sm(void)
{
    int      i, rc;
    size_t   num_proc = poll_info.sm_numproc;
    size_t   rank = poll_info.sm_loop_rank;
    uint64_t seq_val = poll_info.sm_seq_val;
    int      loop_counter = poll_info.sm_loop_counter;

    if(!poll_info.sm_flg_comm_start){
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s: node rank=%zu(%zu) mmap_seq(%zu)=%p(%zu) seq_val=%zu\n",
                __func__, rank, poll_info.sm_intra_index, poll_info.sm_intra_index,
                UTF_BG_MMAP_SEQ(poll_info.sm_intra_index), *(UTF_BG_MMAP_SEQ(poll_info.sm_intra_index)), seq_val);
#endif
        for(i = loop_counter; i < num_proc; i <<= 1){
            if(rank % 2 || (rank + 1) * i >= num_proc){
                /* Odd rank, or even rank without pair stores the value. */
                *(UTF_BG_MMAP_SEQ(rank * i)) = seq_val;
#if defined(DEBUGLOG2)
                fprintf(stderr, "%s: rank=%zu(%zu) set mmap_seq(%zu)=%p(%zu)\n",
                        __func__, rank, poll_info.sm_intra_index, rank*i, UTF_BG_MMAP_SEQ(rank*i),
                        *(UTF_BG_MMAP_SEQ(rank*i)));
#endif
                break;
            }else{
                /* Even rank with a pair checks that the value has been stored. */
                if(*UTF_BG_MMAP_SEQ((rank + 1) * i) != seq_val){
                    /* The value has not been stored yet.
                       Since it may resume from the middle of the loop, save the
                       loop counter and rank and return.  */
                    poll_info.sm_loop_counter = i;
                    poll_info.sm_loop_rank = rank;
                    return UTF_ERR_NOT_COMPLETED;
                }
#if defined(DEBUGLOG2)
                fprintf(stderr, "%s: rank=%zu(%zu) ref mmap_seq(%zu)=%p(%zu)\n",
                        __func__, rank, poll_info.sm_intra_index, (rank+1)*i, UTF_BG_MMAP_SEQ((rank+1)*i),
                        *UTF_BG_MMAP_SEQ((rank+1)*i));
#endif
                rank >>= 1;
            }
        }

        if(poll_info.utf_bg_poll_ids != -1){
#if defined(DEBUGLOG2)
            fprintf(stderr, "%s: Call utofu_barrier: node rank=%zu vbg_ids=%016lx\n",
                    __func__, poll_info.sm_intra_index, poll_info.utf_bg_poll_ids);
#endif
            /* The leader process starts the barrier communication */
            if(UTOFU_SUCCESS != (rc = utofu_barrier(poll_info.utf_bg_poll_ids, 0))){
                return rc;
            }
        }
        poll_info.sm_flg_comm_start = true;
    }

    if(poll_info.utf_bg_poll_ids != -1){
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s: Call utofu_poll_barrier: node rank=%zu vbg_ids=%016lx\n",
                __func__, poll_info.sm_intra_index, poll_info.utf_bg_poll_ids);
#endif
        /* The leader process waits for the communication to complete. */
        if(UTOFU_SUCCESS != (rc = utofu_poll_barrier(poll_info.utf_bg_poll_ids, 0))){
            return rc;
        }
        /* Notify other processes that the communication is completed. */
        *(UTF_BG_MMAP_SEQ(0)) = seq_val;

    }else{
        /* The worker processes waits for the leader's communication to complete. */
        if(*(UTF_BG_MMAP_SEQ(0)) != seq_val){
            return UTF_ERR_NOT_COMPLETED;
        }
    }
#if defined(DEBUGLOG2)
    fprintf(stderr, "%s: node rank=%zu mmap_seq(0)=%p(%zu)\n",
            __func__, poll_info.sm_intra_index, UTF_BG_MMAP_SEQ(0), *(UTF_BG_MMAP_SEQ(0)));
#endif /* DEBUGLOG2 */

    return UTF_SUCCESS;
}


/**
 * utf_bg_reduce_double_sm
 *   In the soft barrier process, values for all proccesses in a node are
 *   collected to the leader process, and the leader process do the barrier
 *   communication.
 *   The worker processes confirm that the barrier communication of the 
 *   leader process is completed and get the result of the operation.
 * @param(OUT) data     address of receive buffer
 * @return UTF_SUCCESS, UTF_ERR_NOT_COMPLETED, or other return codes of uTofu
 *         functions.
 */
int utf_poll_reduce_double_sm(void **data)
{
    int      rc;
    size_t   i, n;
    size_t   count = poll_info.utf_bg_poll_numcount;
    size_t   num_proc = poll_info.sm_numproc;
    size_t   rank = poll_info.sm_loop_rank;
    size_t   loop_counter = poll_info.sm_loop_counter;
    uint64_t seq_val = poll_info.sm_seq_val;

    if(!poll_info.sm_flg_comm_start){
        /* Collect the values of all processes in node into the leader process. */
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s: rank=%zu(%zu) op=%d idata=%p count=%zu mmap_seq(%zu)=%p(%zu), seq_val=%zu\n",
                __func__, rank, poll_info.sm_intra_index, poll_info.utf_bg_poll_op, poll_info.utf_bg_poll_idata,
                count, poll_info.sm_intra_index, UTF_BG_MMAP_SEQ(poll_info.sm_intra_index),
                *(UTF_BG_MMAP_SEQ(poll_info.sm_intra_index)), seq_val);
#endif
        for(i = loop_counter; i < num_proc; i <<= 1){
            if(rank % 2 || (rank + 1) * i >= num_proc){
                /* Odd rank, or even rank without pair stores the value. */
                for(n = 0; n < count; n++){
                   *(UTF_BG_MMAP_BUF(rank * i) + n)  = *((uint64_t *)poll_info.utf_bg_poll_idata + n);
                }
                utf_bg_wmb();
                *(UTF_BG_MMAP_SEQ(rank * i)) = seq_val;
#if defined(DEBUGLOG2)
                fprintf(stderr, "%s: rank=%zu(%zu) set mmap_seq(%zu)=%p(%zu)\n",
                        __func__, rank, poll_info.sm_intra_index, (rank*i), UTF_BG_MMAP_SEQ(rank*i),
                        *(UTF_BG_MMAP_SEQ(rank*i)));
#endif
                break;
            }else{
                /* Even rank with a pair checks that the value has been stored. */
                if(*UTF_BG_MMAP_SEQ((rank + 1) * i) != seq_val){
                    /* The value has not been stored yet.
                       Since it may resume from the middle of the loop, save the
                       loop counter and rank and return.  */
                    poll_info.sm_loop_counter = i;
                    poll_info.sm_loop_rank = rank;
                    return UTF_ERR_NOT_COMPLETED;
                }
                utf_bg_rmb();
#if defined(DEBUGLOG2)
                for(n = 0; n < count; n++){
                    fprintf(stderr, "%s: rank=%zu(%zu) pair=%zu op=%d type=0x%lx count=%zu/%zu before idata=%lf MMAP_BUF=%lf\n",
                            __func__, rank, poll_info.sm_intra_index, (rank+1)*i, poll_info.utf_bg_poll_op,
                            poll_info.utf_bg_poll_datatype, n+1, count,
                            *((double *)poll_info.utf_bg_poll_idata + n), *((double *)(UTF_BG_MMAP_BUF((rank+1)*i)) + n));
                }
#endif /* DEBUGLOG2 */
                /* Operate the own value and the value of next rank. */
                for(n = 0; n < count; n++){
                    *((double *)poll_info.utf_bg_poll_idata + n) += 
                            *((double *)(UTF_BG_MMAP_BUF((rank + 1)*i)) + n);
                }
#if defined(DEBUGLOG2)
                for(n = 0; n < count; n++){
                    fprintf(stderr, "%s: rank=%zu(%zu) op=%d count=%zu/%zu after idata=%lf\n",
                            __func__, rank, poll_info.sm_intra_index, poll_info.utf_bg_poll_op, n+1, count,
                            *((double *)poll_info.utf_bg_poll_idata + n));
                }
#endif /* DEBUGLOG2 */
                rank >>= 1;
            }
        }

        if(poll_info.utf_bg_poll_ids != -1){
#if defined(DEBUGLOG2)
            fprintf(stderr, "%s: Call utofu_reduce_double: node rank=%zu vbg_ids=%016lx op=%d, count=%zu\n",
                    __func__, poll_info.sm_intra_index, poll_info.utf_bg_poll_ids, poll_info.sm_utofu_op, count);
            for(n = 0; n < count; n++){
                fprintf(stderr, "%s: node rank=%zu vbg_ids=%016lx count=%zu/%zu arg data[%zu]=%lf\n",
                        __func__, poll_info.sm_intra_index, poll_info.utf_bg_poll_ids, n+1, 
                        count, n, *((double *)poll_info.utf_bg_poll_idata+n));
            }
#endif /* DEBUGLOG2 */
            /* The leader process starts the barrier communication */
            if(UTOFU_SUCCESS !=
               (rc = utofu_reduce_double(poll_info.utf_bg_poll_ids, poll_info.sm_utofu_op,
                                         poll_info.utf_bg_poll_idata, count, 0))){
                return rc;
            }
        }
        poll_info.sm_flg_comm_start = true;
    }

    if(poll_info.utf_bg_poll_ids != -1){
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s: Call utofu_poll_reduce_double: node rank=%zu vbg_ids=%016lx odata=%p\n",
                __func__, poll_info.sm_intra_index, poll_info.utf_bg_poll_ids, poll_info.utf_bg_poll_odata);
#endif
        /* The leader process waits for the communication to complete. */
        if(UTOFU_SUCCESS !=
           (rc = utofu_poll_reduce_double(poll_info.utf_bg_poll_ids, 0, (double *)poll_info.utf_bg_poll_odata))){
            return rc;
        }
        /* Store the calculation results in shared memory. */
        for(n = 0; n < count; n++){
            *((double *)UTF_BG_MMAP_BUF(0) + n) = *((double *)poll_info.utf_bg_poll_odata + n);
        }
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s: node rank=%zu vbg_ids=%016lx now mmap_seq(0)=%p(%zu)\n",
                __func__, poll_info.sm_intra_index, poll_info.utf_bg_poll_ids, UTF_BG_MMAP_SEQ(0), *(UTF_BG_MMAP_SEQ(0)));
        for(n = 0; n < count; n++){
            fprintf(stderr, "%s: node rank=%zu vbg_ids=%016lx count=%zu/%zu odata[%zu]=%lf\n",
                    __func__, poll_info.sm_intra_index, poll_info.utf_bg_poll_ids, n+1,
                    count, n, *((double *)poll_info.utf_bg_poll_odata + n));
        }
#endif /* DEBUGLOG2 */
        utf_bg_wmb();

        /* Notify other processes that the communication is completed. */
        *(UTF_BG_MMAP_SEQ(0)) = seq_val;
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s: node rank=%zu vbg_ids=%016lx set mmap_seq(0)=%p(%zu)\n",
                __func__, poll_info.sm_intra_index, poll_info.utf_bg_poll_ids,
                UTF_BG_MMAP_SEQ(0), *(UTF_BG_MMAP_SEQ(0)));
#endif

    }else{
        /* The worker processes waits for the leader's communication to complete. */
        if(*(UTF_BG_MMAP_SEQ(0)) != seq_val){
            return UTF_ERR_NOT_COMPLETED;
        }
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s: node rank=%zu vbg_ids=%016lx ref mmap_seq(0)=%p(%zu)\n",
                __func__, poll_info.sm_intra_index, poll_info.utf_bg_poll_ids, UTF_BG_MMAP_SEQ(0), *(UTF_BG_MMAP_SEQ(0)));
#endif /* DEBUGLOG2 */

        utf_bg_rmb();
        /* Gets the leader's result from shared memory. */
        for(n = 0; n < count; n++){
            *((double *)poll_info.utf_bg_poll_odata + n) = *((double *)UTF_BG_MMAP_BUF(0) + n);
#if defined(DEBUGLOG2)
            fprintf(stderr, "%s: node rank=%zu vbg_ids=%016lx count=%zu/%zu odata[%zu]=%lf\n",
                    __func__, poll_info.sm_intra_index, poll_info.utf_bg_poll_ids, n+1, 
                    count, n, *((double *)poll_info.utf_bg_poll_odata + n));
#endif /* DEBUGLOG2 */
        }
    }

#if defined(DEBUGLOG2)
    fprintf(stderr, "%s: node rank=%zu vbg_ids=%016lx data=%p poll_result=%p root=%d count=%zu size=%zu\n",
            __func__, poll_info.sm_intra_index, poll_info.utf_bg_poll_ids, data, 
            poll_info.utf_bg_poll_result, poll_info.utf_bg_poll_root, poll_info.utf_bg_poll_count,
            poll_info.utf_bg_poll_size);
#endif
    /* Set the result in the user buffer. */
    switch(poll_info.utf_bg_poll_size){
        case sizeof(double):
            UTF_BG_SET_RESULT_DOUBLE();
            break;
        case sizeof(float):
            UTF_BG_SET_RESULT_FLOAT();
            break;
        case sizeof(_Float16):
            UTF_BG_SET_RESULT_FLOAT16();
            break;
        default:
            utf_printf("%s: Invalid data size: intra_index=%zu size=%zu\n",
                       __func__, poll_info.sm_intra_index, poll_info.utf_bg_poll_size);
            return UTF_ERR_INTERNAL;
    }

    return UTF_SUCCESS;
}


/**
 * utf_bg_reduce_uint64_sm
 *   In the soft barrier process, values for all proccesses in a node are
 *   collected to the leader process, and the leader process do the barrier
 *   communication.
 *   The worker processes confirm that the barrier communication of the 
 *   leader process is completed and get the result of the operation.
 * @param(OUT) data     address of receive buffer
 * @return UTF_SUCCESS, UTF_ERR_NOT_COMPLETED, or other return codes of uTofu
 *         functions.
 */
int utf_poll_reduce_uint64_sm(void **data)
{
    int      rc;
    size_t   i, j, n;
    size_t   count = poll_info.utf_bg_poll_numcount;
    size_t   num_proc = poll_info.sm_numproc;
    size_t   rank = poll_info.sm_loop_rank;
    size_t   loop_counter = poll_info.sm_loop_counter;
    uint64_t seq_val = poll_info.sm_seq_val;
    int      bit_count;

    if(!poll_info.sm_flg_comm_start){
        /* Collect the values of all processes in node into the leader process. */
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s: rank=%zu(%zu) op=%d utofu_op=%d idata=%p count=%zu mmap_buf=%p mmap_seq(%zu)=%p(%zu) seq_val=%zu\n",
                __func__, rank, poll_info.sm_intra_index, poll_info.utf_bg_poll_op, poll_info.sm_utofu_op,
                poll_info.utf_bg_poll_idata,  count, poll_info.sm_mmap_buf,
                poll_info.sm_intra_index, UTF_BG_MMAP_SEQ(poll_info.sm_intra_index), 
                *(UTF_BG_MMAP_SEQ(poll_info.sm_intra_index)), seq_val);
#endif
        for(i = loop_counter; i < num_proc; i <<= 1){
            if(rank % 2 || (rank + 1) * i >= num_proc){
                /* Odd rank, or even rank without pair stores the value. */
                for(n = 0; n < count; n++){
                   *(UTF_BG_MMAP_BUF(rank * i) + n)  = *((uint64_t *)poll_info.utf_bg_poll_idata + n);
                }
                utf_bg_wmb();
                *(UTF_BG_MMAP_SEQ(rank * i)) = seq_val;
#if defined(DEBUGLOG2)
                fprintf(stderr, "%s: rank=%zu(%zu) set mmap_seq(%zu)=%p(%zu)\n",
                        __func__, rank, poll_info.sm_intra_index, rank*i, UTF_BG_MMAP_SEQ(rank*i),
                        *(UTF_BG_MMAP_SEQ(rank*i)));
#endif
                break;
            }else{
                /* Even rank with a pair checks that the value has been stored. */
                if(*UTF_BG_MMAP_SEQ((rank + 1) * i) != seq_val){
                    /* The value has not been stored yet.
                       Since it may resume from the middle of the loop, save the
                       loop counter and rank and return.  */
                    poll_info.sm_loop_counter = i;
                    poll_info.sm_loop_rank = rank;
                    return UTF_ERR_NOT_COMPLETED;
                }
                utf_bg_rmb();
#if defined(DEBUGLOG2)
                for(n = 0; n < count; n++){
                    fprintf(stderr, "%s: rank=%zu(%zu) pair=%zu utofu_op=%d type=0x%lx count=%zu/%zu"
                                    " before idata=0x%lx MMAP_BUF=0x%lx\n",
                            __func__, rank, poll_info.sm_intra_index, (rank+1)*i, poll_info.sm_utofu_op,
                            poll_info.utf_bg_poll_datatype, n+1, count,
                            *((uint64_t *)poll_info.utf_bg_poll_idata + n), *(UTF_BG_MMAP_BUF((rank+1)*i) + n));
                }
#endif /* DEBUGLOG2 */
                /* Operate the own value and the value of next rank. */
                SM_REDUCE_OP((uint64_t *)poll_info.utf_bg_poll_idata, UTF_BG_MMAP_BUF((rank + 1)*i));
#if defined(DEBUGLOG2)
                for(n = 0; n < count; n++){
                    fprintf(stderr, "%s: rank=%zu(%zu) utofu_op=%d count=%zu/%zu after idata=0x%lx\n",
                            __func__, rank, poll_info.sm_intra_index, poll_info.sm_utofu_op, n+1, count,
                            *((uint64_t *)poll_info.utf_bg_poll_idata + n));
                }
#endif /* DEBUGLOG2 */
                rank >>= 1;
            }
        }

        if(poll_info.utf_bg_poll_ids != -1){
#if defined(DEBUGLOG2)
            fprintf(stderr, "%s: Call utofu_reduce_uint64: node rank=%zu vbg_ids=%016lx op=%d, count=%zu\n",
                    __func__, poll_info.sm_intra_index, poll_info.utf_bg_poll_ids, poll_info.sm_utofu_op, count);
            for(n = 0; n < count; n++){
                fprintf(stderr, "%s: node rank=%zu vbg_ids=%016lx count=%zu/%zu arg data[%zu]=0x%lx\n",
                        __func__, poll_info.sm_intra_index, poll_info.utf_bg_poll_ids, n+1,
                        count, n, *((uint64_t *)poll_info.utf_bg_poll_idata+n));
            }
#endif /* DEBUGLOG2 */
            /* The leader process starts the barrier communication */
            if(UTOFU_SUCCESS !=
               (rc = utofu_reduce_uint64(poll_info.utf_bg_poll_ids, poll_info.sm_utofu_op,
                                         poll_info.utf_bg_poll_idata, count, 0))){
                return rc;
            }
        }
        poll_info.sm_flg_comm_start = true;
    }

    if(poll_info.utf_bg_poll_ids != -1){
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s: Call utofu_poll_reduce_uint64: node rank=%zu vbg_ids=%016lx odata=%p\n",
                __func__, poll_info.sm_intra_index, poll_info.utf_bg_poll_ids, poll_info.utf_bg_poll_odata);
#endif
        /* The leader process waits for the communication to complete. */
        if(UTOFU_SUCCESS !=
           (rc = utofu_poll_reduce_uint64(poll_info.utf_bg_poll_ids, 0, (uint64_t *)poll_info.utf_bg_poll_odata))){
            return rc;
        }
        /* Store the calculation results in shared memory. */
        for(n = 0; n < count; n++){
            *(UTF_BG_MMAP_BUF(0) + n) = *((uint64_t *)poll_info.utf_bg_poll_odata + n);
        }
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s: node rank=%zu vbg_ids=%016lx now mmap_seq(0)=%p(%zu)\n",
                __func__, poll_info.sm_intra_index, poll_info.utf_bg_poll_ids,
                UTF_BG_MMAP_SEQ(0), *(UTF_BG_MMAP_SEQ(0)));
        for(n = 0; n < count; n++){
            fprintf(stderr, "%s: node rank=%zu vbg_ids=%016lx count=%zu/%zu odata[%zu]=0x%lx\n",
                    __func__, poll_info.sm_intra_index, poll_info.utf_bg_poll_ids, n+1,
                    count, n, *((uint64_t *)poll_info.utf_bg_poll_odata + n));
        }
#endif /* DEBUGLOG2 */
        utf_bg_wmb();

        /* Notify other processes that the communication is completed. */
        *(UTF_BG_MMAP_SEQ(0)) = seq_val;
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s: node rank=%zu vbg_ids=%016lx set mmap_seq(0)=%p(%zu)\n",
                __func__, poll_info.sm_intra_index, poll_info.utf_bg_poll_ids,
                UTF_BG_MMAP_SEQ(0), *(UTF_BG_MMAP_SEQ(0)));
#endif

    }else{
        /* The worker processes waits for the leader's communication to complete. */
        if(*(UTF_BG_MMAP_SEQ(0)) != seq_val){
            return UTF_ERR_NOT_COMPLETED;
        }
#if defined(DEBUGLOG2)
        fprintf(stderr, "%s: node rank=%zu vbg_ids=%016lx ref mmap_seq(0)=%p(%zu)\n",
                __func__, poll_info.sm_intra_index, poll_info.utf_bg_poll_ids,
                UTF_BG_MMAP_SEQ(0), *(UTF_BG_MMAP_SEQ(0)));
#endif /* DEBUGLOG2 */

        utf_bg_rmb();
        /* Gets the leader's result from shared memory. */
        for(n = 0; n < count; n++){
            *((uint64_t *)poll_info.utf_bg_poll_odata + n) = *(UTF_BG_MMAP_BUF(0) + n);
#if defined(DEBUGLOG2)
            fprintf(stderr, "%s: node rank=%zu vbg_ids=%016lx count=%zu/%zu odata[%zu]=0x%lx\n",
                    __func__, poll_info.sm_intra_index, poll_info.utf_bg_poll_ids, n+1, 
                    count, n, *((uint64_t *)poll_info.utf_bg_poll_odata + n));
#endif /* DEBUGLOG2 */
        }
    }

#if defined(DEBUGLOG2)
    fprintf(stderr, "%s: node rank=%zu vbg_ids=%016lx data=%p poll_result=%p root=%d count=%zu op=%d datatype=0x%lx\n",
            __func__, poll_info.sm_intra_index, poll_info.utf_bg_poll_ids, data, 
            poll_info.utf_bg_poll_result, poll_info.utf_bg_poll_root, poll_info.utf_bg_poll_count,
            poll_info.utf_bg_poll_op, poll_info.utf_bg_poll_datatype);
#endif
    /* Set the result in the user buffer. */
    switch(poll_info.utf_bg_poll_op){
        case UTF_REDUCE_OP_SUM:
            UTF_BG_SET_RESULT_UINT64();
            break;
        case UTF_BG_BCAST:
            UTF_BG_SET_RESULT_BCAST();
            break;
        case UTF_REDUCE_OP_MAX:
        case UTF_REDUCE_OP_MIN:
            if(UTF_DATATYPE_DIV_REAL == poll_info.utf_bg_poll_datatype >> 24){
                UTF_BG_SET_RESULT_DOUBLE_MAX_MIN();
            }else{
                UTF_BG_SET_RESULT_UINT64_MAX_MIN();
            }
            break;
        case UTF_REDUCE_OP_MAXLOC:
        case UTF_REDUCE_OP_MINLOC:
            UTF_BG_SET_RESULT_MAXMIN_LOC();
            break;
        case UTF_REDUCE_OP_BAND:
        case UTF_REDUCE_OP_BOR:
        case UTF_REDUCE_OP_BXOR:
            UTF_BG_SET_RESULT_BITWISE();
            break;
        case UTF_REDUCE_OP_LAND:
        case UTF_REDUCE_OP_LOR:
        case UTF_REDUCE_OP_LXOR:
            UTF_BG_SET_RESULT_LOGICAL();
            break;
        default:
            utf_printf("%s: Invalid reduction operation: intra_index=%zu op=%d\n",
                       __func__, poll_info.sm_intra_index, poll_info.utf_bg_poll_op);
            return UTF_ERR_INTERNAL;
    }

    return UTF_SUCCESS;
}

