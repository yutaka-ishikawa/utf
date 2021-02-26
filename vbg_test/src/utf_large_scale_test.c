 /**
 * MPICH-tofu Large-scale verification test program
 *
 * Copyright (C) 2021 RIKEN, Japan. All rights reserved.
 *
 */

/*
 * header files
 */
#include <complex.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utf_large_scale_test.h"

/*
 * Variables
 */
static int  wrank;                  // Rank(for MPI_COMM_WORLD)
static int  wprocs;                 // Number of processes(for MPI_COMM_WORLD)
static int  comm_rank;              // Rank(for communicators other than MPI_COMM_WORLD)
static int  comm_procs;             // Number of processes(for communicators other than MPI_COMM_WORLD)
static MPI_Comm tcomm;              // Target communicator
static MPI_Group tgroup;            // Target group(used only when test_comm is COMM_CREATE)
static int  root_rank;              // Root rank when testing

static int  test_func;              // Factor A: mpi function
static int  test_comm;              // Factor C: communicator
static bool flg_measure;            // Factor D: Flags for performance measurement
                                    //   true: measure, false: do not measure
static MPI_Op test_op;              // Factor E: operation
static MPI_Datatype test_datatype;  // Factor F: datatype
static int  test_op_n;              // Operation type in int format(for Open MPI)
static int  test_datatype_n;        // Datatype in int format(for Open MPI)
static int  test_count;             // Factor G: Element count
static bool flg_use_in_place;       // Factor H: Flag for MPI_IN_PLACE
                                    //   true: use MPI_IN_PLACE, false: do not use MPI_IN_PLACE
static bool flg_diff_type;          // Factor I: Flag for using different datatypes
                                    //   true: use different datatypes, false: use same datatype

static int  iter;                   // Number of iterations for the target MPI function call
static int  comm_count;             // Number of repetitions for communicator operation
static bool flg_print_info;         // Flag for detailed information output
                                    //   true: output, false: do not output
static bool flg_check_vbg;          // Flag for checking VBG calls
                                    //   true: check, false: do not check
static int  split_size;             // Split size

static int  vbg_ret;                // Result of VBG call
static char msgtmp[128];            // Work buffer for test results
static char msgtmp2[128];           // Work buffer for test results
#if !USE_WTIME
uint64_t freq;
#endif
#if defined(MPICH_API_PUBLIC)
uint64_t utf_bg_counter MPICH_API_PUBLIC;
                                    // VBG call counter
#else /* FJMPI */
//unsinged long long mca_coll_tbi_exec_counter __attribute__((visibility("default")));
unsigned long long mca_coll_tbi_exec_counter OMPI_DECLSPEC;
#define utf_bg_counter   mca_coll_tbi_exec_counter;
#endif
double      call_elapse;


/*
 * Initialize valiables.
 */
static void init_val(void)
{
    test_func        = FUNC_MPI_BARRIER;
    test_comm        = COMM_WORLD;
    test_op          = MPI_SUM;
    test_op_n        = OP_SUM;
    test_datatype    = MPI_LONG;
    test_datatype_n  = DT_LONG;
    test_count       = 1;
    iter             = 1;
    comm_count       = 3;
    root_rank        = 0;
    flg_measure      = true;
    flg_use_in_place = false;
    flg_diff_type    = false;
    flg_print_info   = false;
    flg_check_vbg    = true;
    split_size       = 2;
    vbg_ret          = VBG_NOT_PASS;
}


/*
 * Check the arguments and set the test patterns.
 */
static int check_args(int argc, char **argv)
{
    int i, j;

    for (i=1; i<argc;) {
        if (strcmp("--func", argv[i]) == 0) {
            if (i+1 >= argc) {
                fprintf(stderr, "[%s] argument %s with no value was ignored.\n", __func__, argv[i]);
                break;
            }
            for (j=0; j<MAX_FUNC; j++) {
                if (strcmp(FUNC_STR[j], argv[i+1]) == 0) {
                    test_func = j;
                    break;
                }
            }
            if (j == MAX_FUNC) {
                fprintf(stderr, "[%s] invalid arguments: --func %s\n", __func__, argv[i+1]);
                return FUNC_ERROR;
            }
            i+=2;
        }
        else if (strcmp("--comm", argv[i]) == 0) {
            if (i+1 >= argc) {
                fprintf(stderr, "[%s] argument %s with no value was ignored.\n", __func__, argv[i]);
                break;
            }
            for (j=0; j<MAX_COMM; j++) {
                if (strcmp(COMM_STR[j], argv[i+1]) == 0) {
                    test_comm = j;
                    break;
                }
            }
            if (j == MAX_COMM) {
                fprintf(stderr, "[%s] invalid arguments: --comm %s\n", __func__, argv[i+1]);
                return FUNC_ERROR;
            }
            i+=2;
        }
        else if (strcmp("--no_measure", argv[i]) == 0) {
            flg_measure = false;
            i++;
        }
        else if (strcmp("--op", argv[i]) == 0) {
            if (i+1 >= argc) {
                fprintf(stderr, "[%s] argument %s with no value was ignored.\n", __func__, argv[i]);
                break;
            }
            for (j=0; j<MAX_OP; j++) {
                if (strcmp(OP_STR[j], argv[i+1]) == 0) {
                    test_op = test_op_tbl[j];
                    test_op_n = j;
                    break;
                }
            }
            if (j == MAX_OP) {
                fprintf(stderr, "[%s] invalid arguments: --op %s\n", __func__, argv[i+1]);
                return FUNC_ERROR;
            }
            i+=2;
        }
        else if (strcmp("--datatype", argv[i]) == 0) {
            if (i+1 >= argc) {
                fprintf(stderr, "[%s] argument %s with no value was ignored.\n", __func__, argv[i]);
                break;
            }
            for (j=0; j<MAX_DATATYPE; j++) {
                if (strcmp(DATATYPE_STR[j], argv[i+1]) == 0) {
                    test_datatype = test_datatype_tbl[j];
                    test_datatype_n = j;
                    break;
                }
            }
            if (j == MAX_DATATYPE) {
                fprintf(stderr, "[%s] invalid arguments: --datatype %s\n", __func__, argv[i+1]);
                return FUNC_ERROR;
            }
            i+=2;
        }
        else if (strcmp("--count", argv[i]) == 0) {
            if (i+1 >= argc) {
                fprintf(stderr, "[%s] argument %s with no value was ignored.\n", __func__, argv[i]);
                break;
            }
            test_count = atoi(argv[i+1]);
            if (test_count < 0) {
                fprintf(stderr, "[%s] invalid arguments: --count %s\n", __func__, argv[i+1]);
                return FUNC_ERROR;
            }
            i+=2;
        }
        else if (strcmp("--use_in_place", argv[i]) == 0) {
            flg_use_in_place = true;
            i++;
        }
        else if (strcmp("--diff_type", argv[i]) == 0) {
            flg_diff_type = true;
            i++;
        }
        else if (strcmp("--split_size", argv[i]) == 0) {
            if (i+1 >= argc) {
                fprintf(stderr, "[%s] argument %s with no value was ignored.\n", __func__, argv[i]);
                break;
            }
            split_size = atoi(argv[i+1]);
            if (split_size < 2 || split_size > wprocs) {
                fprintf(stderr, "[%s] invalid arguments: --split_size %d\n", __func__, split_size);
                return FUNC_ERROR;
            }
            i+=2;
        }
        else if (strcmp("--print_info", argv[i]) == 0) {
            flg_print_info = true;
            i++;
        }
        else if (strcmp("--no_check_vbg", argv[i]) == 0) {
            flg_check_vbg = false;
            i++;
        }
        else if (strcmp("--iter", argv[i]) == 0) {
            if (i+1 >= argc) {
                fprintf(stderr, "[%s] argument %s with no value was ignored.\n", __func__, argv[i]);
                break;
            }
            iter = atoi(argv[i+1]);
            i+=2;
        }
        else if (strcmp("--comm_count", argv[i]) == 0) {
            if (i+1 >= argc) {
                fprintf(stderr, "[%s] argument %s with no value was ignored.\n", __func__, argv[i]);
                break;
            }
            comm_count = atoi(argv[i+1]);
            i+=2;
        }
        else {
            fprintf(stderr, "[%s] ignored arguments: %s\n", __func__, argv[i]);
            i++;
        }
    }
    return FUNC_SUCCESS;
}


/*
 * Print informations.
 */
static void print_infos(void) {
    fprintf(stdout, "[info] A: MPI function =%s\n", FUNC_STR[test_func]);
    fprintf(stdout, "[info] C: communicator =%s\n", COMM_STR[test_comm]);
    fprintf(stdout, "[info] D: measure      =%s\n", (flg_measure?"yes":"no"));
    if (test_func != FUNC_MPI_BARRIER) {
        if (test_func != FUNC_MPI_BCAST) {
            fprintf(stdout, "[info] E: OP           =%s\n", OP_STR[test_op_n]);
        }
        fprintf(stdout, "[info] F: datatype     =%s\n", DATATYPE_STR[test_datatype_n]);
        fprintf(stdout, "[info] G: element count=%d\n", test_count);
        if (test_func != FUNC_MPI_BCAST) {
            fprintf(stdout, "[info] H: MPI_IN_PLACE =%s\n", (flg_use_in_place?"use":"do not use"));
        }
        else {
            fprintf(stdout, "[info] I: diff type    =%s\n", (flg_diff_type?"use":"do not use"));
        }
    }
    if (test_comm == COMM_SPLIT ||
        test_comm == COMM_SPLIT_FREE ||
        test_comm == COMM_SPLIT_NO_FREE) {
        fprintf(stdout, "[info] split size      =%d\n", split_size);
    }
    fprintf(stdout, "[info] iter            =%d\n", iter);
    fprintf(stdout, "[info] comm_count      =%d\n", comm_count);
    fprintf(stdout, "[info] check_vbg       =%s\n", (flg_measure?"yes":"no"));
    fflush(stdout);
}


/*
 * Create the communicator.
 */
static int create_comm(MPI_Comm base_comm, int base_rank, int base_procs, MPI_Group base_group)
{
    int ret = MPI_SUCCESS;

    switch (test_comm) {
        case COMM_WORLD:
            tcomm = MPI_COMM_WORLD;
            comm_rank = wrank;
            comm_procs = wprocs;
            return FUNC_SUCCESS;

        case COMM_SPLIT:
        case COMM_SPLIT_FREE:
        case COMM_SPLIT_NO_FREE:
            {
                int color;

                color = base_rank % split_size;
#if TPDEBUG
                fprintf(stderr, "[%s:%d] call MPI_Comm_split: base_comm=%s rank(w/c)=%d/%d\n",
                        __func__, __LINE__, (base_comm==MPI_COMM_WORLD?"WORLD":"other"), wrank, base_rank);
#endif
                ret = MPI_Comm_split(base_comm, color, base_rank, &tcomm);
                if (ret != MPI_SUCCESS) {
                    fprintf(stderr, "[%s:%d] MPI_Comm_split error: ret=%d base_comm=%d rank(w/c)=%d/%d"
                            " color=%d split_size=%d\n",
                            __func__, __LINE__, ret, (int)base_comm, wrank, base_rank, color, split_size);
                    return FUNC_ERROR;
                }
            }
            break;

        case COMM_DUP:
        case COMM_DUP_FREE:
        case COMM_DUP_NO_FREE:
            {
                ret = MPI_Comm_dup(MPI_COMM_WORLD, &tcomm);
                if (ret != MPI_SUCCESS) {
                    fprintf(stderr, "[%s:%d] MPI_Comm_dup error: ret=%d wrank=%d\n",
                            __func__, __LINE__, ret, wrank);
                    return FUNC_ERROR;
                }
#if TPDEBUG
                fprintf(stderr, "[%s:%d] MPI_Comm_dup success: wrank=%d\n",
                        __func__, __LINE__, wrank);
#endif
            }
            break;

        case COMM_CREATE:
        case COMM_CREATE_FREE:
        case COMM_CREATE_NO_FREE:
            {
                int *ranks, *wp, i, gsize=0;

                ranks = malloc(sizeof(int) * (base_procs/2+1));
                if (ranks == NULL) {
                    fprintf(stderr, "[%s:%d] cannot allocate memory: rank(w/c)=%d/%d\n",
                            __func__, __LINE__, wrank, base_rank);
                    return FUNC_ERROR;
                }
                // Divide into groups by odd rank and even rank.
                wp = ranks;
                for (i=(base_rank%2 ? 1:0); i<base_procs; i+=2, wp++) {
                    *wp = i;
                    gsize++;
                }
                // Create the new group
                ret = MPI_Group_incl(base_group, gsize, ranks, &tgroup);
                if (ret != MPI_SUCCESS) {
                    fprintf(stderr, "[%s:%d] MPI_Group_incl error: ret=%d rank(w/c)=%d/%d"
                            " base_group=%d base_procs=%d gsize=%d\n",
                            __func__, __LINE__, ret, wrank, base_rank, (int)base_group, base_procs, gsize);
                    free(ranks);
                    return FUNC_ERROR;
                }
                // Create the new communicator
#if TPDEBUG
                fprintf(stderr, "[%s:%d] call MPI_Comm_create: rank(w/c)=%d/%d\n",
                        __func__, __LINE__, wrank, base_rank);
#endif
                ret = MPI_Comm_create(base_comm, tgroup, &tcomm);
                if (ret != MPI_SUCCESS) {
                    fprintf(stderr, "[%s:%d] MPI_Comm_create error: ret=%d rank(w/c)=%d/%d\n"
                            " base_comm=%d tgroup=%d",
                            __func__, __LINE__, ret, wrank, base_rank, (int)base_comm, (int)tgroup);
                    free(ranks);
                    return FUNC_ERROR;
                }
                free(ranks);
            }
            break;

        default:
            fprintf(stderr, "[%s] Invalid communicator type: rank(w/c)=%d/%d test_comm=%d\n",
                    __func__, wrank, base_rank, test_comm);
            return FUNC_ERROR;
    }

    ret = MPI_Comm_size(tcomm, &comm_procs);
    if (ret != MPI_SUCCESS) {
        fprintf(stderr, "[%s:%d] MPI_Comm_size error: ret=%d rank(w/c)=%d/%d test_comm=%d tcomm=%d\n",
                __func__, __LINE__, ret, wrank, base_rank, test_comm, (int)tcomm);
        return FUNC_ERROR;
    }
    ret = MPI_Comm_rank(tcomm, &comm_rank);
    if (ret != MPI_SUCCESS) {
        fprintf(stderr, "[%s:%d] MPI_Comm_rank error: ret=%d rank(w/c)=%d/%d test_comm=%d tcomm=%d\n",
                __func__, __LINE__, ret, wrank, base_rank, test_comm, (int)tcomm);
        return FUNC_ERROR;
    }
    return FUNC_SUCCESS;
}


/*
 * Free the comunicator (and the group in the case of COMM_CREATE*).
 */
static int free_comm(void)
{
    int ret = MPI_SUCCESS;

#if TPDEBUG
    fprintf(stderr, "[%s:%d] start: rank(w/c)=%d/%d test_comm=%d tcomm=%d\n",
            __func__, __LINE__, wrank, comm_rank, test_comm, (int)tcomm);
#endif
    if (test_comm == COMM_WORLD) {
        return FUNC_SUCCESS;
    }

    if (tcomm != 0) {
#if TPDEBUG
        fprintf(stderr, "[%s:%d] call MPI_Comm_free: rank(w/c)=%d/%d tcomm=%d\n",
                __func__, __LINE__, wrank, comm_rank, (int)tcomm);
#endif
        ret = MPI_Comm_free(&tcomm);
        if (ret != MPI_SUCCESS) {
            fprintf(stderr, "[%s] MPI_Comm_free error: ret=%d rank(w/c)=%d/%d tcomm=%d\n",
                    __func__, ret, wrank, comm_rank, (int)tcomm);
            return FUNC_ERROR;
        }
        tcomm = 0;
    }

    if (tgroup != 0) {
#if TPDEBUG
        fprintf(stderr, "[%s:%d] call MPI_Group_free: rank(w/c)=%d/%d\n",
                __func__, __LINE__, wrank, comm_rank);
#endif
        ret = MPI_Group_free(&tgroup);
        if (ret != MPI_SUCCESS) {
            fprintf(stderr, "[%s] MPI_Group_free error: ret=%d rank(w/c)=%d/%d tgroup=%d\n",
                    __func__, ret, wrank, comm_rank, (int)tgroup);
            return FUNC_ERROR;
        }
        tgroup = 0;
    }
    return FUNC_SUCCESS;
}


/*
 * Functions for time measurement
 */
#if !USE_WTIME
static inline uint64_t get_freq(void)
{
    uint64_t freq;

    __asm__ __volatile__ ("mrs %0,  CNTFRQ_EL0" : "=r" (freq));
    return freq;
}
#endif

static inline utf_timer_t get_time(void)
{

#if USE_WTIME
    return MPI_Wtime();
#else
    utf_timer_t wt;

    __asm__ __volatile__ ("isb" ::: "memory");
    __asm__ __volatile__ ("mrs %0,  CNTVCT_EL0" : "=r" (wt));
    return wt;
#endif
}

static inline double get_elapse(utf_timer_t start, utf_timer_t end)
{
    double elapse, maxtime = 0.0;
    int ret = MPI_SUCCESS;

#if USE_WTIME
    elapse = (end-start)*1000000.0/iter;
#else
    elapse = (((double)end-start)/freq)*1000000.0/iter;
#endif
    ret = MPI_Reduce(&elapse, &maxtime, 1, MPI_DOUBLE, MPI_MAX, root_rank, tcomm);
    if (ret != MPI_SUCCESS) {
        fprintf(stderr, "[%s] MPI_Reduce error: ret=%d rank(w/c)=%d/%d\n",
                __func__, ret, wrank, comm_rank);
    }
    return maxtime;
}

static inline double get_call_elapse(utf_timer_t elapse_sum)
{
    double elapse, maxtime = 0.0;
    int ret = MPI_SUCCESS;

#if USE_WTIME
    elapse = elapse_sum*1000000.0/iter;
#else
    elapse = (((double)elapse_sum)/freq)*1000000.0/iter;
#endif
    ret = MPI_Reduce(&elapse, &maxtime, 1, MPI_DOUBLE, MPI_MAX, root_rank, tcomm);
    if (ret != MPI_SUCCESS) {
        fprintf(stderr, "[%s] MPI_Reduce error: ret=%d rank(w/c)=%d/%d\n",
                __func__, ret, wrank, comm_rank);
    }
    return maxtime;
}


/*
 * Allocate the data buffers.
 */
static inline int allocate_buffer(void **buf_p, void **rbuf_p, void **ansbuf_p, long buf_size) {

    void *wp;

    if (buf_p == NULL || ansbuf_p == NULL) {
        fprintf(stderr, "[%s] no arguments specified: rank(w/c)=%d/%d buf_p=%p ansbuf_p=%p\n",
                __func__, wrank, comm_rank, buf_p, ansbuf_p);
        return FUNC_ERROR;
    }

    // Allocate and initialize the data buffer.
    //   (Bcast: send/receive, Reduce/Allreduce: send)
    wp = malloc(buf_size);
    if (wp == NULL) {
        fprintf(stderr, "[%s:%d] cannot allocate memory: rank(w/c)=%d/%d\n",
                __func__, __LINE__, wrank, comm_rank);
        return FUNC_ERROR;
    }
    *buf_p = wp;

    // Allocate and initialize the receive data buffer (only for Reduce/Allreduce).
    if (rbuf_p != NULL) {
        if (test_func == FUNC_MPI_REDUCE && comm_rank != root_rank) {
            // The case where the receive data buffer is not used.
            *rbuf_p = NULL;
        }
        else {
            wp = malloc(buf_size);
            if (wp == NULL) {
                fprintf(stderr, "[%s:%d] cannot allocate memory: rank(w/c)=%d/%d\n",
                        __func__, __LINE__, wrank, comm_rank);
                return FUNC_ERROR;
            }
            *rbuf_p = wp;
        }
    }

    // Allocate and initialize the buffer to store the answer.
    wp = malloc(buf_size);
    if (wp == NULL) {
        fprintf(stderr, "[%s:%d] cannot allocate memory: rank(w/c)=%d/%d\n",
                __func__, __LINE__, wrank, comm_rank);
        return FUNC_ERROR;
    }
    *ansbuf_p = wp;

    return FUNC_SUCCESS;
}


/*
 * Set the test data and the answer for MPI_Bcast.
 */
static int set_data_for_bcast(void *sbuf, void *ansbuf, long buf_size) {

    int n;

    if (ansbuf == NULL || (comm_rank == root_rank && sbuf == NULL)) {
        fprintf(stderr,
                "[%s] no buffer specified: rank(w/c)=%d/%d sbuf=%p ansbuf=%p\n",
                __func__, wrank, comm_rank, sbuf, ansbuf);
        return FUNC_ERROR;
    }
    memset(sbuf, 0x00, buf_size);
    memset(ansbuf, 0x00, buf_size);

    switch (test_datatype_n) {
        case DT_SIGNED_CHAR:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((signed char *)sbuf+n) = (signed char)n;
                }
                *((signed char *)ansbuf+n) = (signed char)n;
            }
            break;
        case DT_SHORT:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((short *)sbuf+n) = (short)n;
                }
                *((short *)ansbuf+n) = (short)n;
            }
            break;
        case DT_INT:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((int *)sbuf+n) = n;
                }
                *((int *)ansbuf+n) = n;
            }
            break;
        case DT_LONG:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((long *)sbuf+n) = n + 1;
                }
                *((long *)ansbuf+n) = n + 1;
            }
            break;
        case DT_LONG_LONG:
        case DT_LONG_LONG_INT:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((long long *)sbuf+n) = n + 1;
                }
                *((long long *)ansbuf+n) = n + 1;
            }
            break;
        case DT_BYTE:
        case DT_UNSIGNED_CHAR:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((unsigned char *)sbuf+n) = (unsigned char)n;
                }
                *((unsigned char *)ansbuf+n) = (unsigned char)n;
            }
            break;
        case DT_UNSIGNED_SHORT:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((unsigned short *)sbuf+n) = (unsigned short)n;
                }
                *((unsigned short *)ansbuf+n) = (unsigned short)n;
            }
            break;
        case DT_UNSIGNED:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((unsigned int *)sbuf+n) = (unsigned int)n;
                }
                *((unsigned int *)ansbuf+n) = (unsigned int)n;
            }
            break;
        case DT_UNSIGNED_LONG:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((unsigned long *)sbuf+n) = n + 1;
                }
                *((unsigned long *)ansbuf+n) = n + 1;
            }
            break;
        case DT_UNSIGNED_LONG_LONG:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((unsigned long long *)sbuf+n) = n + 1;
                }
                *((unsigned long long *)ansbuf+n) = n + 1;
            }
            break;
        case DT_INT8_T:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((int8_t *)sbuf+n) = (int8_t)n;
                }
                *((int8_t *)ansbuf+n) = (int8_t)n;
            }
            break;
        case DT_INT16_T:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((int16_t *)sbuf+n) = (int16_t)n;
                }
                *((int16_t *)ansbuf+n) = (int16_t)n;
            }
            break;
        case DT_INT32_T:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((int32_t *)sbuf+n) = (int32_t)n;
                }
                *((int32_t *)ansbuf+n) = (int32_t)n;
            }
            break;
        case DT_INT64_T:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((int64_t *)sbuf+n) = (int64_t)n + 1;
                }
                *((int64_t *)ansbuf+n) = (int64_t)n + 1;
            }
            break;
        case DT_UINT8_T:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((uint8_t *)sbuf+n) = (uint8_t)n;
                }
                *((uint8_t *)ansbuf+n) = (uint8_t)n;
            }
            break;
        case DT_UINT16_T:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((uint16_t *)sbuf+n) = (uint16_t)n;
                }
                *((uint16_t *)ansbuf+n) = (uint16_t)n;
            }
            break;
        case DT_UINT32_T:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((uint32_t *)sbuf+n) = (uint32_t)n;
                }
                *((uint32_t *)ansbuf+n) = (uint32_t)n;
            }
            break;
        case DT_UINT64_T:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((uint64_t *)sbuf+n) = (uint64_t)n + 1;
                }
                *((uint64_t *)ansbuf+n) = (uint64_t)n + 1;
            }
            break;
        case DT_WCHAR:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((wchar_t *)sbuf+n) = (wchar_t)n;
                }
                *((wchar_t *)ansbuf+n) = (wchar_t)n;
            }
            break;
        case DT_C_BOOL:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((_Bool *)sbuf+n) = true;
                }
                *((_Bool *)ansbuf+n) = true;
            }
            break;
        case DT_FLOAT:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((float *)sbuf+n) = (float)n;
                }
                *((float *)ansbuf+n) = (float)n;
            }
            break;
        case DT_DOUBLE:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((double *)sbuf+n) = (double)n;
                }
                *((double *)ansbuf+n) = (double)n;
            }
            break;
        case DT_LONG_DOUBLE:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((long double *)sbuf+n) = (long double)n;
                }
                *((long double *)ansbuf+n) = (long double)n;
            }
            break;
#if defined(MPIX_C_FLOAT16)
        case DT_C_FLOAT16:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((_Float16 *)sbuf+n) = (_Float16)n;
                }
                *((_Float16 *)ansbuf+n) = (_Float16)n;
            }
            break;
#endif
        case DT_C_COMPLEX:
        case DT_C_FLOAT_COMPLEX:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((float _Complex *)sbuf+n) = n + 1.0i;
                }
                *((float _Complex *)ansbuf+n) = n + 1.0i;
            }
            break;
        case DT_C_DOUBLE_COMPLEX:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((double _Complex *)sbuf+n) = n + 1.0i;
                }
                *((double _Complex *)ansbuf+n) = n + 1.0i;
            }
            break;
        case DT_C_LONG_DOUBLE_COMPLEX:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    *((long double _Complex *)sbuf+n) = n + 1.0i;
                }
                *((long double _Complex *)ansbuf+n) = n + 1.0i;
            }
            break;
        case DT_SHORT_INT:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    ((short_int *)sbuf+n)->val = (short)n;  // value part
                    ((short_int *)sbuf+n)->ind = 1;         // index part
                }
                ((short_int *)ansbuf+n)->val = (short)n;
                ((short_int *)ansbuf+n)->ind = 1;
            }
            break;
        case DT_2INT:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    ((int_int *)sbuf+n)->val = (int)n;     // value part
                    ((int_int *)sbuf+n)->ind = 1;          // index part
                }
                ((int_int *)ansbuf+n)->val = (int)n;
                ((int_int *)ansbuf+n)->ind = 1;
            }
            break;
        case DT_LONG_INT:
            for (n=0; n<test_count; n++) {
                if (comm_rank == root_rank) {
                    ((long_int *)sbuf+n)->val = n;  // value part
                    ((long_int *)sbuf+n)->ind = 1;  // index part
                }
                ((long_int *)ansbuf+n)->val = n;
                ((long_int *)ansbuf+n)->ind = 1;
            }
            break;
        default:
            fprintf(stderr,
                    "[%s] invalid datatype: rank(w/c)=%d/%d func=%s datatype=%d\n",
                    __func__, wrank, comm_rank, FUNC_STR[test_func], test_datatype_n);
            return FUNC_ERROR;
    }
    return FUNC_SUCCESS;
}


/*
 * Set the test data and the answer for MPI_Reduce/MPI_Allreduce(MPI_SUM).
 */
static int set_data_for_reduce_sum(void *sbuf, void *ansbuf, long buf_size) {

    int n, cn, ans_n;

    if (sbuf == NULL || ansbuf == NULL) {
        fprintf(stderr, "[%s] no buffer specified: rank(w/c)=%d/%d sbuf=%p ansbuf=%p\n",
                __func__, wrank, comm_rank, sbuf, ansbuf);
        return FUNC_ERROR;
    }
    memset(sbuf, 0x00, buf_size);
    memset(ansbuf, 0x00, buf_size);

    switch (test_datatype_n) {
        case DT_DOUBLE:
            for (n=0; n<test_count; n++) {
                *((double *)sbuf+n) = (double)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        *((double *)ansbuf+n) += (double)cn;
                    }
                }
            }
            break;
        case DT_SIGNED_CHAR:
            for (n=0; n<test_count; n++) {
                *((signed char *)sbuf+n) = (signed char)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        *((signed char *)ansbuf+n) += (signed char)cn;
                    }
                }
            }
            break;
        case DT_SHORT:
            for (n=0; n<test_count; n++) {
                *((short *)sbuf+n) = (short)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        *((short *)ansbuf+n) += (short)cn;
                    }
                }
            }
            break;
        case DT_INT:
            for (n=0; n<test_count; n++) {
                *((int *)sbuf+n) = (int)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        *((int *)ansbuf+n) += cn;
                    }
                }
            }
            break;
        case DT_LONG:
            for (n=0; n<test_count; n++) {
                *((long *)sbuf+n) = comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        *((long *)ansbuf+n) += cn;
                    }
                }
            }
            break;
        case DT_LONG_LONG:
        case DT_LONG_LONG_INT:
            for (n=0; n<test_count; n++) {
                *((long long *)sbuf+n) = comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        *((long long *)ansbuf+n) += cn;
                    }
                }
            }
            break;
        case DT_BYTE:
        case DT_UNSIGNED_CHAR:
            for (n=0; n<test_count; n++) {
                *((unsigned char *)sbuf+n) = (unsigned char)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        *((unsigned char *)ansbuf+n) += (unsigned char)cn;
                    }
                }
            }
            break;
        case DT_UNSIGNED_SHORT:
            for (n=0; n<test_count; n++) {
                *((unsigned short *)sbuf+n) = (unsigned short)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        *((unsigned short *)ansbuf+n) += (unsigned short)cn;
                    }
                }
            }
            break;
        case DT_UNSIGNED:
            for (n=0; n<test_count; n++) {
                *((unsigned int *)sbuf+n) = (unsigned int)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        *((unsigned int *)ansbuf+n) += (unsigned int)cn;
                    }
                }
            }
            break;
        case DT_UNSIGNED_LONG:
            for (n=0; n<test_count; n++) {
                *((unsigned long *)sbuf+n) = comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        *((unsigned long *)ansbuf+n) += cn;
                    }
                }
            }
            break;
        case DT_UNSIGNED_LONG_LONG:
            for (n=0; n<test_count; n++) {
                *((unsigned long long *)sbuf+n) = comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        *((unsigned long long *)ansbuf+n) += cn;
                    }
                }
            }
            break;
        case DT_INT8_T:
            for (n=0; n<test_count; n++) {
                *((int8_t *)sbuf+n) = (int8_t)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        *((int8_t *)ansbuf+n) += (int8_t)cn;
                    }
                }
            }
            break;
        case DT_INT16_T:
            for (n=0; n<test_count; n++) {
                *((int16_t *)sbuf+n) = (int16_t)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        *((int16_t *)ansbuf+n) += (int16_t)cn;
                    }
                }
            }
            break;
        case DT_INT32_T:
            for (n=0; n<test_count; n++) {
                *((int32_t *)sbuf+n) = (int32_t)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        *((int32_t *)ansbuf+n) += (int32_t)cn;
                    }
                }
            }
            break;
        case DT_INT64_T:
            for (n=0; n<test_count; n++) {
                *((int64_t *)sbuf+n) = comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        *((int64_t *)ansbuf+n) += cn;
                    }
                }
            }
            break;
        case DT_UINT8_T:
            for (n=0; n<test_count; n++) {
                *((uint8_t *)sbuf+n) = (uint8_t)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        *((uint8_t *)ansbuf+n) += (uint8_t)cn;
                    }
                }
            }
            break;
        case DT_UINT16_T:
            for (n=0; n<test_count; n++) {
                *((uint16_t *)sbuf+n) = (uint16_t)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        *((uint16_t *)ansbuf+n) += (uint16_t)cn;
                    }
                }
            }
            break;
        case DT_UINT32_T:
            for (n=0; n<test_count; n++) {
                *((uint32_t *)sbuf+n) = (uint32_t)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        *((uint32_t *)ansbuf+n) += (uint32_t)cn;
                    }
                }
            }
            break;
        case DT_UINT64_T:
            for (n=0; n<test_count; n++) {
                *((uint64_t *)sbuf+n) = comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        *((uint64_t *)ansbuf+n) += cn;
                    }
                }
            }
            break;
        case DT_FLOAT:
            for (n=0; n<test_count; n++) {
                *((float *)sbuf+n) = (float)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0, ans_n=0; cn<comm_procs; cn++) {
                        ans_n += cn;
                    }
                    *((float *)ansbuf+n) = (float)ans_n;
                }
            }
            break;
        case DT_LONG_DOUBLE:
            for (n=0; n<test_count; n++) {
                *((long double *)sbuf+n) = (long double)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        *((long double *)ansbuf+n) += (long double)cn;
                    }
                }
            }
            break;
#if defined(MPIX_C_FLOAT16)
        case DT_C_FLOAT16:
            for (n=0; n<test_count; n++) {
                *((_Float16 *)sbuf+n) = (_Float16)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0, ans_n=0; cn<comm_procs; cn++) {
                        ans_n += cn;
                    }
                    *((_Float16 *)ansbuf+n) = (_Float16)ans_n;
#if defined(TPDEBUG)
                    fprintf(stderr, "[%s] rank(w/c)=%d/%d ans_n=%d, ansbuf[%d]=%f\n",
                            __func__, wrank, comm_rank, ans_n, n, (double)*((_Float16 *)ansbuf+n));
#endif
                }
            }
            break;
#endif
        case DT_C_COMPLEX:
        case DT_C_FLOAT_COMPLEX:
            for (n=0; n<test_count; n++) {
                *((float _Complex *)sbuf+n) = comm_rank + 1.0i;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0, ans_n=0; cn<comm_procs; cn++) {
                        ans_n += cn;
                    }
                    *((float _Complex *)ansbuf+n) = (float _Complex)(ans_n + comm_procs * I);
                }
            }
            break;
        case DT_C_DOUBLE_COMPLEX:
            for (n=0; n<test_count; n++) {
                *((double _Complex *)sbuf+n) = comm_rank + 1.0i;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        *((double _Complex *)ansbuf+n) += cn + 1.0i;
                    }
                }
            }
            break;
        case DT_C_LONG_DOUBLE_COMPLEX:
            for (n=0; n<test_count; n++) {
                *((long double _Complex *)sbuf+n) = comm_rank + 1.0i;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        *((long double _Complex *)ansbuf+n) += cn + 1.0i;
                    }
                }
            }
            break;

        // Following types cannot be used in these reduction operations.
        case DT_WCHAR:
            for (n=0; n<test_count; n++) {
                *((wchar_t *)sbuf+n) = (wchar_t)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        *((wchar_t *)ansbuf+n) += (wchar_t)comm_rank;
                    }
                }
            }
            break;
        case DT_C_BOOL:
            for (n=0; n<test_count; n++) {
                *((_Bool *)sbuf+n) = true;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        *((_Bool *)ansbuf+n) = true;
                    }
                }
            }
            break;
        case DT_SHORT_INT:
            for (n=0; n<test_count; n++) {
                ((short_int *)sbuf+n)->val = (short)comm_rank;  // value part
                ((short_int *)sbuf+n)->ind = comm_rank;         // index part
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        ((short_int *)ansbuf+n)->val += (short)comm_rank;
                        ((short_int *)ansbuf+n)->ind += cn;
                    }
                }
            }
            break;
        case DT_2INT:
            for (n=0; n<test_count; n++) {
                ((int_int *)sbuf+n)->val = comm_rank;   // value part
                ((int_int *)sbuf+n)->ind = comm_rank;   // index part
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        ((int_int *)ansbuf+n)->val += (int)comm_rank;
                        ((int_int *)ansbuf+n)->ind += cn;
                    }
                }
            }
            break;
        case DT_LONG_INT:
            for (n=0; n<test_count; n++) {
                ((long_int *)sbuf+n)->val = n;          // value part
                ((long_int *)sbuf+n)->ind = comm_rank;  // index part
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (cn=0; cn<comm_procs; cn++) {
                        ((long_int *)ansbuf+n)->val += n;
                        ((long_int *)ansbuf+n)->ind += cn;
                    }
                }
            }
            break;
        default:
            fprintf(stderr, "[%s] invalid datatype: rank(w/c)=%d/%d func=%s datatype=%d\n",
                    __func__, wrank, comm_rank, FUNC_STR[test_func], test_datatype_n);
            return FUNC_ERROR;
    }
    return FUNC_SUCCESS;
}


/*
 * Set the test data and the answer for MPI_Reduce/MPI_Allreduce(MPI_BAND/MPI_BOR/MPI_BXOR).
 */
static int set_data_for_reduce_bit(void *sbuf, void *ansbuf, long buf_size) {

    int n, cn;

    if (sbuf == NULL || ansbuf == NULL) {
        fprintf(stderr, "[%s] no buffer specified: rank(w/c)=%d/%d sbuf=%p ansbuf=%p\n",
                __func__, wrank, comm_rank, sbuf, ansbuf);
        return FUNC_ERROR;
    }
    memset(sbuf, 0x00, buf_size);
    memset(ansbuf, 0x00, buf_size);

    switch (test_datatype_n) {
        case DT_SIGNED_CHAR:
            for (n=0; n<test_count; n++) {
                *((signed char *)sbuf+n) = (signed char)comm_rank;
            }
            if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                if (test_op_n == OP_BAND) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((char *)ansbuf+n) &= (char)cn;
                        }
                    }
                }
                else if (test_op_n == OP_BOR) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((char *)ansbuf+n) |= (char)cn;
                        }
                    }
                }
                else { // OP_BXOR
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((char *)ansbuf+n) ^= (char)cn;
                        }
                    }
                }
            }
            break;
        case DT_SHORT:
            for (n=0; n<test_count; n++) {
                *((short *)sbuf+n) = (short)comm_rank;
            }
            if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                if (test_op_n == OP_BAND) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((short *)ansbuf+n) &= (short)cn;
                        }
                    }
                }
                else if (test_op_n == OP_BOR) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((short *)ansbuf+n) |= (short)cn;
                        }
                    }
                }
                else { // OP_BXOR
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((short *)ansbuf+n) ^= (short)cn;
                        }
                    }
                }
            }
            break;
        case DT_INT:
            for (n=0; n<test_count; n++) {
                *((int *)sbuf+n) = comm_rank;
            }
            if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                if (test_op_n == OP_BAND) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((int *)ansbuf+n) &= cn;
                        }
                    }
                }
                else if (test_op_n == OP_BOR) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((int *)ansbuf+n) |= cn;
                        }
                    }
                }
                else { // OP_BXOR
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((int *)ansbuf+n) ^= cn;
                        }
                    }
                }
            }
            break;
        case DT_LONG:
            for (n=0; n<test_count; n++) {
                *((long *)sbuf+n) = comm_rank;
            }
            if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                if (test_op_n == OP_BAND) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((long *)ansbuf+n) &= cn;
                        }
                    }
                }
                else if (test_op_n == OP_BOR) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((long *)ansbuf+n) |= cn;
                        }
                    }
                }
                else { // OP_BXOR
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((long *)ansbuf+n) ^= cn;
                        }
                    }
                }
            }
            break;
        case DT_LONG_LONG:
        case DT_LONG_LONG_INT:
            for (n=0; n<test_count; n++) {
                *((long long *)sbuf+n) = comm_rank;
            }
            if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                if (test_op_n == OP_BAND) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((long long *)ansbuf+n) &= cn;
                        }
                    }
                }
                else if (test_op_n == OP_BOR) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((long long *)ansbuf+n) |= cn;
                        }
                    }
                }
                else { // OP_BXOR
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((long long *)ansbuf+n) ^= cn;
                        }
                    }
                }
            }
            break;
        case DT_BYTE:
        case DT_UNSIGNED_CHAR:
            for (n=0; n<test_count; n++) {
                *((unsigned char *)sbuf+n) = (unsigned char)comm_rank;
            }
            if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                if (test_op_n == OP_BAND) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((unsigned char *)ansbuf+n) &= (unsigned char)cn;
                        }
                    }
                }
                else if (test_op_n == OP_BOR) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((unsigned char *)ansbuf+n) |= (unsigned char)cn;
                        }
                    }
                }
                else { // OP_BXOR
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((unsigned char *)ansbuf+n) ^= (unsigned char)cn;
                        }
                    }
                }
            }
            break;
        case DT_UNSIGNED_SHORT:
            for (n=0; n<test_count; n++) {
                *((unsigned short *)sbuf+n) = (unsigned short)comm_rank;
            }
            if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                if (test_op_n == OP_BAND) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((unsigned short *)ansbuf+n) &= (unsigned short)cn;
                        }
                    }
                }
                else if (test_op_n == OP_BOR) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((unsigned short *)ansbuf+n) |= (unsigned short)cn;
                        }
                    }
                }
                else { // OP_BXOR
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((unsigned short *)ansbuf+n) ^= (unsigned short)cn;
                        }
                    }
                }
            }
            break;
        case DT_UNSIGNED:
            for (n=0; n<test_count; n++) {
                *((unsigned int *)sbuf+n) = (unsigned int)comm_rank;
            }
            if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                if (test_op_n == OP_BAND) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((unsigned int *)ansbuf+n) &= (unsigned int)cn;
                        }
                    }
                }
                else if (test_op_n == OP_BOR) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((unsigned int *)ansbuf+n) |= (unsigned int)cn;
                        }
                    }
                }
                else { // OP_BXOR
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((unsigned int *)ansbuf+n) ^= (unsigned int)cn;
                        }
                    }
                }
            }
            break;
        case DT_UNSIGNED_LONG:
            for (n=0; n<test_count; n++) {
                *((unsigned long *)sbuf+n) = comm_rank;
            }
            if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                if (test_op_n == OP_BAND) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((unsigned long *)ansbuf+n) &= cn;
                        }
                    }
                }
                else if (test_op_n == OP_BOR) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((unsigned long *)ansbuf+n) |= cn;
                        }
                    }
                }
                else { // OP_BXOR
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((unsigned long *)ansbuf+n) ^= cn;
                        }
                    }
                }
            }
            break;
        case DT_UNSIGNED_LONG_LONG:
            for (n=0; n<test_count; n++) {
                *((unsigned long long *)sbuf+n) = comm_rank;
            }
            if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                if (test_op_n == OP_BAND) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((unsigned long long *)ansbuf+n) &= cn;
                        }
                    }
                }
                else if (test_op_n == OP_BOR) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((unsigned long long *)ansbuf+n) |= cn;
                        }
                    }
                }
                else { // OP_BXOR
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((unsigned long long *)ansbuf+n) ^= cn;
                        }
                    }
                }
            }
            break;
        case DT_INT8_T:
            for (n=0; n<test_count; n++) {
                *((int8_t *)sbuf+n) = (int8_t)comm_rank;
            }
            if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                if (test_op_n == OP_BAND) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((int8_t *)ansbuf+n) &= (int8_t)cn;
                        }
                    }
                }
                else if (test_op_n == OP_BOR) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((int8_t *)ansbuf+n) |= (int8_t)cn;
                        }
                    }
                }
                else { // OP_BXOR
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((int8_t *)ansbuf+n) ^= (int8_t)cn;
                        }
                    }
                }
            }
            break;
        case DT_INT16_T:
            for (n=0; n<test_count; n++) {
                *((int16_t *)sbuf+n) = (int16_t)comm_rank;
            }
            if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                if (test_op_n == OP_BAND) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((int16_t *)ansbuf+n) &= (int16_t)cn;
                        }
                    }
                }
                else if (test_op_n == OP_BOR) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((int16_t *)ansbuf+n) |= (int16_t)cn;
                        }
                    }
                }
                else { // OP_BXOR
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((int16_t *)ansbuf+n) ^= (int16_t)cn;
                        }
                    }
                }
            }
            break;
        case DT_INT32_T:
            for (n=0; n<test_count; n++) {
                *((int32_t *)sbuf+n) = (int32_t)comm_rank;
            }
            if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                if (test_op_n == OP_BAND) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((int32_t *)ansbuf+n) &= (int32_t)cn;
                        }
                    }
                }
                else if (test_op_n == OP_BOR) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((int32_t *)ansbuf+n) |= (int32_t)cn;
                        }
                    }
                }
                else { // OP_BXOR
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((int32_t *)ansbuf+n) ^= (int32_t)cn;
                        }
                    }
                }
            }
            break;
        case DT_INT64_T:
            for (n=0; n<test_count; n++) {
                *((int64_t *)sbuf+n) = comm_rank;
            }
            if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                if (test_op_n == OP_BAND) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((int64_t *)ansbuf+n) &= cn;
                        }
                    }
                }
                else if (test_op_n == OP_BOR) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((int64_t *)ansbuf+n) |= cn;
                        }
                    }
                }
                else { // OP_BXOR
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((int64_t *)ansbuf+n) ^= cn;
                        }
                    }
                }
            }
            break;
        case DT_UINT8_T:
            for (n=0; n<test_count; n++) {
                *((uint8_t *)sbuf+n) = (uint8_t)comm_rank;
            }
            if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                if (test_op_n == OP_BAND) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((uint8_t *)ansbuf+n) &= (uint8_t)cn;
                        }
                    }
                }
                else if (test_op_n == OP_BOR) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((uint8_t *)ansbuf+n) |= (uint8_t)cn;
                        }
                    }
                }
                else { // OP_BXOR
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((uint8_t *)ansbuf+n) ^= (uint8_t)cn;
                        }
                    }
                }
            }
            break;
        case DT_UINT16_T:
            for (n=0; n<test_count; n++) {
                *((uint16_t *)sbuf+n) = (uint16_t)comm_rank;
            }
            if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                if (test_op_n == OP_BAND) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((uint16_t *)ansbuf+n) &= (uint16_t)cn;
                        }
                    }
                }
                else if (test_op_n == OP_BOR) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((uint16_t *)ansbuf+n) |= (uint16_t)cn;
                        }
                    }
                }
                else { // OP_BXOR
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((uint16_t *)ansbuf+n) ^= (uint16_t)cn;
                        }
                    }
                }
            }
            break;
        case DT_UINT32_T:
            for (n=0; n<test_count; n++) {
                *((uint32_t *)sbuf+n) = (uint32_t)comm_rank;
            }
            if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                if (test_op_n == OP_BAND) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((uint32_t *)ansbuf+n) &= (uint32_t)cn;
                        }
                    }
                }
                else if (test_op_n == OP_BOR) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((uint32_t *)ansbuf+n) |= (uint32_t)cn;
                        }
                    }
                }
                else { // OP_BXOR
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((uint32_t *)ansbuf+n) ^= (uint32_t)cn;
                        }
                    }
                }
            }
            break;
        case DT_UINT64_T:
            for (n=0; n<test_count; n++) {
                *((uint64_t *)sbuf+n) = comm_rank;
            }
            if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                if (test_op_n == OP_BAND) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((uint64_t *)ansbuf+n) &= cn;
                        }
                    }
                }
                else if (test_op_n == OP_BOR) {
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((uint64_t *)ansbuf+n) |= cn;
                        }
                    }
                }
                else { // OP_BXOR
                    for (n=0; n<test_count; n++) {
                        for (cn=1; cn<comm_procs; cn++) {
                            *((uint64_t *)ansbuf+n) ^= cn;
                        }
                    }
                }
            }
            break;

        // Following types cannot be used in these reduction operations.
        case DT_WCHAR:
            for (n=0; n<test_count; n++) {
                *((wchar_t *)sbuf+n) = (wchar_t)comm_rank;
            }
            break;
        case DT_C_BOOL:
            for (n=0; n<test_count; n++) {
                *((bool *)sbuf+n) = (bool)comm_rank;
            }
            break;
        case DT_FLOAT:
            for (n=0; n<test_count; n++) {
                *((float *)sbuf+n) = (float)comm_rank;
            }
            break;
        case DT_DOUBLE:
            for (n=0; n<test_count; n++) {
                *((double *)sbuf+n) = (double)comm_rank;
            }
            break;
        case DT_LONG_DOUBLE:
            for (n=0; n<test_count; n++) {
                *((long double *)sbuf+n) = (long double)comm_rank;
            }
            break;
#if defined(MPIX_C_FLOAT16)
        case DT_C_FLOAT16:
            for (n=0; n<test_count; n++) {
                *((_Float16 *)sbuf+n) = (_Float16)comm_rank;
            }
            break;
#endif
        case DT_C_COMPLEX:
        case DT_C_FLOAT_COMPLEX:
            for (n=0; n<test_count; n++) {
                *((float _Complex *)sbuf+n) = comm_rank + 1.0i;
            }
            break;
        case DT_C_DOUBLE_COMPLEX:
            for (n=0; n<test_count; n++) {
                *((double _Complex *)sbuf+n) = comm_rank + 1.0i;
            }
            break;
        case DT_C_LONG_DOUBLE_COMPLEX:
            for (n=0; n<test_count; n++) {
                *((long double _Complex *)sbuf+n) = comm_rank + 1.0i;
            }
            break;
        case DT_SHORT_INT:
            for (n=0; n<test_count; n++) {
                ((short_int *)sbuf+n)->val = (short)n;  // value part
                ((short_int *)sbuf+n)->ind = 1;         // index part
            };
            break;
        case DT_2INT:
            for (n=0; n<test_count; n++) {
                ((int_int *)sbuf+n)->val = n;   // value part
                ((int_int *)sbuf+n)->ind = 1;   // index part
            };
            break;
        case DT_LONG_INT:
            for (n=0; n<test_count; n++) {
                ((long_int *)sbuf+n)->val = n;  // value part
                ((long_int *)sbuf+n)->ind = 1;  // index part
            };
            break;
        default:
            fprintf(stderr, "[%s] invalid datatype: rank(w/c)=%d/%d func=%s datatype=%d\n",
                    __func__, wrank, comm_rank, FUNC_STR[test_func], test_datatype_n);
            return FUNC_ERROR;
    }
    return FUNC_SUCCESS;
}


/*
 * Set the test data and the answer for MPI_Reduce/MPI_Allreduce(MPI_LAND/MPI_LOR/MPI_LXOR).
 */
static int set_data_for_reduce_logical(void *sbuf, void *ansbuf, long buf_size) {

    int n;

    if (sbuf == NULL || ansbuf == NULL) {
        fprintf(stderr, "[%s] no buffer specified: rank(w/c)=%d/%d sbuf=%p ansbuf=%p\n",
                __func__, wrank, comm_rank, sbuf, ansbuf);
        return FUNC_ERROR;
    }
    memset(sbuf, 0x00, buf_size);
    memset(ansbuf, 0x00, buf_size);

    //     test data      answer
    //   n  root other     LXOR
    //  ---+------------+---------
    //  [0]    1     0        1
    //  [1]    0     0        0
    //   : <repeat>
    //
    //   n  root other   LAND  LOR
    //  ---+------------+---------
    //  [0]    1     1      1    1
    //  [1]    1     0      0    1
    //  [2]    0     1      0    1
    //  [3]    0     0      0    0
    //   : <repeat>

    switch (test_datatype_n) {
        case DT_SIGNED_CHAR:
            if (test_op_n == OP_LXOR) {
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((signed char *)sbuf+n) = (n%2==0 ? 1:0);
                    }
                    else {
                        *((signed char *)sbuf+n) = 0;
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (n=0; n<test_count; n++) {
                        *((signed char *)ansbuf+n) = ((n%2==0) ? 1:0);
                    }
                }
            }
            else { // OP_LAND, OP_LOR
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((signed char *)sbuf+n) = ((n%4==0 || (n-1)%4==0) ? 1:0);
                    }
                    else {
                        *((signed char *)sbuf+n) = (signed char)((n+1)%2);
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_LAND) {
                        for (n=0; n<test_count; n++) {
                            *((signed char *)ansbuf+n) = ((n%4==0) ? 1:0);
                        }
                    }
                    else { // OP_LOR
                        for (n=0; n<test_count; n++) {
                            *((signed char *)ansbuf+n) = ((n!=0 && (n+1)%4==0) ? 0:1);
                        }
                    }
                }
            }
            break;
        case DT_SHORT:
            if (test_op_n == OP_LXOR) {
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((short *)sbuf+n) = (n%2==0 ? 1:0);
                    }
                    else {
                        *((short *)sbuf+n) = 0;
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (n=0; n<test_count; n++) {
                        *((short *)ansbuf+n) = ((n%2==0) ? 1:0);
                    }
                }
            }
            else { // OP_LAND, OP_LOR
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((short *)sbuf+n) = ((n%4==0 || (n-1)%4==0) ? 1:0);
                    }
                    else {
                        *((short *)sbuf+n) = (short)((n+1)%2);
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_LAND) {
                        for (n=0; n<test_count; n++) {
                            *((short *)ansbuf+n) = ((n%4==0) ? 1:0);
                        }
                    }
                    else { // OP_LOR
                        for (n=0; n<test_count; n++) {
                            *((short *)ansbuf+n) = ((n!=0 && (n+1)%4==0) ? 0:1);
                        }
                    }
                }
            }
            break;
        case DT_INT:
            if (test_op_n == OP_LXOR) {
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((int *)sbuf+n) = (n%2==0 ? 1:0);
                    }
                    else {
                        *((int *)sbuf+n) = 0;
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (n=0; n<test_count; n++) {
                        *((int *)ansbuf+n) = ((n%2==0) ? 1:0);
                    }
                }
            }
            else { // OP_LAND, OP_LOR
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((int *)sbuf+n) = ((n%4==0 || (n-1)%4==0) ? 1:0);
                    }
                    else {
                        *((int *)sbuf+n) = ((n+1)%2);
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_LAND) {
                        for (n=0; n<test_count; n++) {
                            *((int *)ansbuf+n) = ((n%4==0) ? 1:0);
                        }
                    }
                    else { // OP_LOR
                        for (n=0; n<test_count; n++) {
                            *((int *)ansbuf+n) = ((n!=0 && (n+1)%4==0) ? 0:1);
                        }
                    }
                }
            }
            break;
        case DT_LONG:
            if (test_op_n == OP_LXOR) {
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((long *)sbuf+n) = (n%2==0 ? 1:0);
                    }
                    else {
                        *((long *)sbuf+n) = 0;
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (n=0; n<test_count; n++) {
                        *((long *)ansbuf+n) = ((n%2==0) ? 1:0);
                    }
                }
            }
            else { // OP_LAND, OP_LOR
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((long *)sbuf+n) = ((n%4==0 || (n-1)%4==0) ? 1:0);
                    }
                    else {
                        *((long *)sbuf+n) = ((n+1)%2);
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_LAND) {
                        for (n=0; n<test_count; n++) {
                            *((long *)ansbuf+n) = ((n%4==0) ? 1:0);
                        }
                    }
                    else { // OP_LOR
                        for (n=0; n<test_count; n++) {
                            *((long *)ansbuf+n) = ((n!=0 && (n+1)%4==0) ? 0:1);
                        }
                    }
                }
            }
            break;
        case DT_LONG_LONG:
        case DT_LONG_LONG_INT:
            if (test_op_n == OP_LXOR) {
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((long long *)sbuf+n) = (n%2==0 ? 1:0);
                    }
                    else {
                        *((long long *)sbuf+n) = 0;
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (n=0; n<test_count; n++) {
                        *((long long *)ansbuf+n) = ((n%2==0) ? 1:0);
                    }
                }
            }
            else { // OP_LAND, OP_LOR
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((long long *)sbuf+n) = ((n%4==0 || (n-1)%4==0) ? 1:0);
                    }
                    else {
                        *((long long *)sbuf+n) = ((n+1)%2);
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_LAND) {
                        for (n=0; n<test_count; n++) {
                            *((long long *)ansbuf+n) = ((n%4==0) ? 1:0);
                        }
                    }
                    else { // OP_LOR
                        for (n=0; n<test_count; n++) {
                            *((long long *)ansbuf+n) = ((n!=0 && (n+1)%4==0) ? 0:1);
                        }
                    }
                }
            }
            break;
        case DT_BYTE:
        case DT_UNSIGNED_CHAR:
            if (test_op_n == OP_LXOR) {
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((unsigned char *)sbuf+n) = (n%2==0 ? 1:0);
                    }
                    else {
                        *((unsigned char *)sbuf+n) = 0;
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (n=0; n<test_count; n++) {
                        *((unsigned char *)ansbuf+n) = ((n%2==0) ? 1:0);
                    }
                }
            }
            else { // OP_LAND, OP_LOR
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((unsigned char *)sbuf+n) = ((n%4==0 || (n-1)%4==0) ? 1:0);
                    }
                    else {
                        *((unsigned char *)sbuf+n) = (unsigned char)((n+1)%2);
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_LAND) {
                        for (n=0; n<test_count; n++) {
                            *((unsigned char *)ansbuf+n) = ((n%4==0) ? 1:0);
                        }
                    }
                    else { // OP_LOR
                        for (n=0; n<test_count; n++) {
                            *((unsigned char *)ansbuf+n) = ((n!=0 && (n+1)%4==0) ? 0:1);
                        }
                    }
                }
            }
            break;
        case DT_UNSIGNED_SHORT:
            if (test_op_n == OP_LXOR) {
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((unsigned short *)sbuf+n) = (n%2==0 ? 1:0);
                    }
                    else {
                        *((unsigned short *)sbuf+n) = 0;
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (n=0; n<test_count; n++) {
                        *((unsigned short *)ansbuf+n) = ((n%2==0) ? 1:0);
                    }
                }
            }
            else { // OP_LAND, OP_LOR
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((unsigned short *)sbuf+n) = ((n%4==0 || (n-1)%4==0) ? 1:0);
                    }
                    else {
                        *((unsigned short *)sbuf+n) = (unsigned short)((n+1)%2);
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_LAND) {
                        for (n=0; n<test_count; n++) {
                            *((unsigned short *)ansbuf+n) = ((n%4==0) ? 1:0);
                        }
                    }
                    else { // OP_LOR
                        for (n=0; n<test_count; n++) {
                            *((unsigned short *)ansbuf+n) = ((n!=0 && (n+1)%4==0) ? 0:1);
                        }
                    }
                }
            }
            break;
        case DT_UNSIGNED:
            if (test_op_n == OP_LXOR) {
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((unsigned int *)sbuf+n) = (n%2==0 ? 1:0);
                    }
                    else {
                        *((unsigned int *)sbuf+n) = 0;
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (n=0; n<test_count; n++) {
                        *((unsigned int *)ansbuf+n) = ((n%2==0) ? 1:0);
                    }
                }
            }
            else { // OP_LAND, OP_LOR
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((unsigned int *)sbuf+n) = ((n%4==0 || (n-1)%4==0) ? 1:0);
                    }
                    else {
                        *((unsigned int *)sbuf+n) = ((n+1)%2);
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_LAND) {
                        for (n=0; n<test_count; n++) {
                            *((unsigned int *)ansbuf+n) = ((n%4==0) ? 1:0);
                        }
                    }
                    else { // OP_LOR
                        for (n=0; n<test_count; n++) {
                            *((unsigned int *)ansbuf+n) = ((n!=0 && (n+1)%4==0) ? 0:1);
                        }
                    }
                }
            }
            break;
        case DT_UNSIGNED_LONG:
            if (test_op_n == OP_LXOR) {
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((unsigned long *)sbuf+n) = (n%2==0 ? 1:0);
                    }
                    else {
                        *((unsigned long *)sbuf+n) = 0;
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (n=0; n<test_count; n++) {
                        *((unsigned long *)ansbuf+n) = ((n%2==0) ? 1:0);
                    }
                }
            }
            else { // OP_LAND, OP_LOR
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((unsigned long *)sbuf+n) = ((n%4==0 || (n-1)%4==0) ? 1:0);
                    }
                    else {
                        *((unsigned long *)sbuf+n) = ((n+1)%2);
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_LAND) {
                        for (n=0; n<test_count; n++) {
                            *((unsigned long *)ansbuf+n) = ((n%4==0) ? 1:0);
                        }
                    }
                    else { // OP_LOR
                        for (n=0; n<test_count; n++) {
                            *((unsigned long *)ansbuf+n) = ((n!=0 && (n+1)%4==0) ? 0:1);
                        }
                    }
                }
            }
            break;
        case DT_UNSIGNED_LONG_LONG:
            if (test_op_n == OP_LXOR) {
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((unsigned long long *)sbuf+n) = (n%2==0 ? 1:0);
                    }
                    else {
                        *((unsigned long long *)sbuf+n) = 0;
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (n=0; n<test_count; n++) {
                        *((unsigned long long *)ansbuf+n) = ((n%2==0) ? 1:0);
                    }
                }
            }
            else { // OP_LAND, OP_LOR
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((unsigned long long *)sbuf+n) = ((n%4==0 || (n-1)%4==0) ? 1:0);
                    }
                    else {
                        *((unsigned long long *)sbuf+n) = ((n+1)%2);
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_LAND) {
                        for (n=0; n<test_count; n++) {
                            *((unsigned long long *)ansbuf+n) = ((n%4==0) ? 1:0);
                        }
                    }
                    else { // OP_LOR
                        for (n=0; n<test_count; n++) {
                            *((unsigned long long *)ansbuf+n) = ((n!=0 && (n+1)%4==0) ? 0:1);
                        }
                    }
                }
            }
            break;
        case DT_INT8_T:
            if (test_op_n == OP_LXOR) {
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((int8_t *)sbuf+n) = (n%2==0 ? 1:0);
                    }
                    else {
                        *((int8_t *)sbuf+n) = 0;
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (n=0; n<test_count; n++) {
                        *((int8_t *)ansbuf+n) = ((n%2==0) ? 1:0);
                    }
                }
            }
            else { // OP_LAND, OP_LOR
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((int8_t *)sbuf+n) = ((n%4==0 || (n-1)%4==0) ? 1:0);
                    }
                    else {
                        *((int8_t *)sbuf+n) = (int8_t)((n+1)%2);
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_LAND) {
                        for (n=0; n<test_count; n++) {
                            *((int8_t *)ansbuf+n) = ((n%4==0) ? 1:0);
                        }
                    }
                    else { // OP_LOR
                        for (n=0; n<test_count; n++) {
                            *((int8_t *)ansbuf+n) = ((n!=0 && (n+1)%4==0) ? 0:1);
                        }
                    }
                }
            }
            break;
        case DT_INT16_T:
            if (test_op_n == OP_LXOR) {
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((int16_t *)sbuf+n) = (n%2==0 ? 1:0);
                    }
                    else {
                        *((int16_t *)sbuf+n) = 0;
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (n=0; n<test_count; n++) {
                        *((int16_t *)ansbuf+n) = ((n%2==0) ? 1:0);
                    }
                }
            }
            else { // OP_LAND, OP_LOR
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((int16_t *)sbuf+n) = ((n%4==0 || (n-1)%4==0) ? 1:0);
                    }
                    else {
                        *((int16_t *)sbuf+n) = (int16_t)((n+1)%2);
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_LAND) {
                        for (n=0; n<test_count; n++) {
                            *((int16_t *)ansbuf+n) = ((n%4==0) ? 1:0);
                        }
                    }
                    else { // OP_LOR
                        for (n=0; n<test_count; n++) {
                            *((int16_t *)ansbuf+n) = ((n!=0 && (n+1)%4==0) ? 0:1);
                        }
                    }
                }
            }
            break;
        case DT_INT32_T:
            if (test_op_n == OP_LXOR) {
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((int32_t *)sbuf+n) = (n%2==0 ? 1:0);
                    }
                    else {
                        *((int32_t *)sbuf+n) = 0;
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (n=0; n<test_count; n++) {
                        *((int32_t *)ansbuf+n) = ((n%2==0) ? 1:0);
                    }
                }
            }
            else { // OP_LAND, OP_LOR
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((int32_t *)sbuf+n) = ((n%4==0 || (n-1)%4==0) ? 1:0);
                    }
                    else {
                        *((int32_t *)sbuf+n) = ((n+1)%2);
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_LAND) {
                        for (n=0; n<test_count; n++) {
                            *((int32_t *)ansbuf+n) = ((n%4==0) ? 1:0);
                        }
                    }
                    else { // OP_LOR
                        for (n=0; n<test_count; n++) {
                            *((int32_t *)ansbuf+n) = ((n!=0 && (n+1)%4==0) ? 0:1);
                        }
                    }
                }
            }
            break;
        case DT_INT64_T:
            if (test_op_n == OP_LXOR) {
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((int64_t *)sbuf+n) = (n%2==0 ? 1:0);
                    }
                    else {
                        *((int64_t *)sbuf+n) = 0;
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (n=0; n<test_count; n++) {
                        *((int64_t *)ansbuf+n) = ((n%2==0) ? 1:0);
                    }
                }
            }
            else { // OP_LAND, OP_LOR
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((int64_t *)sbuf+n) = ((n%4==0 || (n-1)%4==0) ? 1:0);
                    }
                    else {
                        *((int64_t *)sbuf+n) = ((n+1)%2);
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_LAND) {
                        for (n=0; n<test_count; n++) {
                            *((int64_t *)ansbuf+n) = ((n%4==0) ? 1:0);
                        }
                    }
                    else { // OP_LOR
                        for (n=0; n<test_count; n++) {
                            *((int64_t *)ansbuf+n) = ((n!=0 && (n+1)%4==0) ? 0:1);
                        }
                    }
                }
            }
            break;
        case DT_UINT8_T:
            if (test_op_n == OP_LXOR) {
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((uint8_t *)sbuf+n) = (n%2==0 ? 1:0);
                    }
                    else {
                        *((uint8_t *)sbuf+n) = 0;
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (n=0; n<test_count; n++) {
                        *((uint8_t *)ansbuf+n) = ((n%2==0) ? 1:0);
                    }
                }
            }
            else { // OP_LAND, OP_LOR
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((uint8_t *)sbuf+n) = (uint8_t)((n%4==0 || (n-1)%4==0) ? 1:0);
                    }
                    else {
                        *((uint8_t *)sbuf+n) = (uint8_t)((n+1)%2);
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_LAND) {
                        for (n=0; n<test_count; n++) {
                            *((uint8_t *)ansbuf+n) = ((n%4==0) ? 1:0);
                        }
                    }
                    else { // OP_LOR
                        for (n=0; n<test_count; n++) {
                            *((uint8_t *)ansbuf+n) = ((n!=0 && (n+1)%4==0) ? 0:1);
                        }
                    }
                }
            }
            break;
        case DT_UINT16_T:
            if (test_op_n == OP_LXOR) {
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((uint16_t *)sbuf+n) = (n%2==0 ? 1:0);
                    }
                    else {
                        *((uint16_t *)sbuf+n) = 0;
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (n=0; n<test_count; n++) {
                        *((uint16_t *)ansbuf+n) = ((n%2==0) ? 1:0);
                    }
                }
            }
            else { // OP_LAND, OP_LOR
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((uint16_t *)sbuf+n) = ((n%4==0 || (n-1)%4==0) ? 1:0);
                    }
                    else {
                        *((uint16_t *)sbuf+n) = (uint16_t)((n+1)%2);
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_LAND) {
                        for (n=0; n<test_count; n++) {
                            *((uint16_t *)ansbuf+n) = ((n%4==0) ? 1:0);
                        }
                    }
                    else { // OP_LOR
                        for (n=0; n<test_count; n++) {
                            *((uint16_t *)ansbuf+n) = ((n!=0 && (n+1)%4==0) ? 0:1);
                        }
                    }
                }
            }
            break;
        case DT_UINT32_T:
            if (test_op_n == OP_LXOR) {
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((uint32_t *)sbuf+n) = (n%2==0 ? 1:0);
                    }
                    else {
                        *((uint32_t *)sbuf+n) = 0;
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (n=0; n<test_count; n++) {
                        *((uint32_t *)ansbuf+n) = ((n%2==0) ? 1:0);
                    }
                }
            }
            else { // OP_LAND, OP_LOR
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((uint32_t *)sbuf+n) = ((n%4==0 || (n-1)%4==0) ? 1:0);
                    }
                    else {
                        *((uint32_t *)sbuf+n) = ((n+1)%2);
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_LAND) {
                        for (n=0; n<test_count; n++) {
                            *((uint32_t *)ansbuf+n) = ((n%4==0) ? 1:0);
                        }
                    }
                    else { // OP_LOR
                        for (n=0; n<test_count; n++) {
                            *((uint32_t *)ansbuf+n) = ((n!=0 && (n+1)%4==0) ? 0:1);
                        }
                    }
                }
            }
            break;
        case DT_UINT64_T:
            if (test_op_n == OP_LXOR) {
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((uint64_t *)sbuf+n) = (n%2==0 ? 1:0);
                    }
                    else {
                        *((uint64_t *)sbuf+n) = 0;
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (n=0; n<test_count; n++) {
                        *((uint64_t *)ansbuf+n) = ((n%2==0) ? 1:0);
                    }
                }
            }
            else { // OP_LAND, OP_LOR
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((uint64_t *)sbuf+n) = ((n%4==0 || (n-1)%4==0) ? 1:0);
                    }
                    else {
                        *((uint64_t *)sbuf+n) = ((n+1)%2);
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_LAND) {
                        for (n=0; n<test_count; n++) {
                            *((uint64_t *)ansbuf+n) = ((n%4==0) ? 1:0);
                        }
                    }
                    else { // OP_LOR
                        for (n=0; n<test_count; n++) {
                            *((uint64_t *)ansbuf+n) = ((n!=0 && (n+1)%4==0) ? 0:1);
                        }
                    }
                }
            }
            break;
        case DT_C_BOOL:
            if (test_op_n == OP_LXOR) {
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((bool *)sbuf+n) = (n%2==0 ? true:false);
                    }
                    else {
                        *((bool *)sbuf+n) = false;
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    for (n=0; n<test_count; n++) {
                        *((bool *)ansbuf+n) = ((n%2==0) ? true:false);
                    }
                }
            }
            else { // OP_LAND, OP_LOR
                for (n=0; n<test_count; n++) {
                    if (comm_rank == root_rank) {
                        *((bool *)sbuf+n) = ((n%4==0 || (n-1)%4==0) ? true:false);
                    }
                    else {
                        *((bool *)sbuf+n) = (bool)((n+1)%2);
                    }
                }
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_LAND) {
                        for (n=0; n<test_count; n++) {
                            *((bool *)ansbuf+n) = ((n%4==0) ? true:false);
                        }
                    }
                    else { // OP_LOR
                        for (n=0; n<test_count; n++) {
                            *((bool *)ansbuf+n) = ((n!=0 && (n+1)%4==0) ? false:true);
                        }
                    }
                }
            }
            break;

        // Following types cannot be used in these reduction operations.
        case DT_WCHAR:
            for (n=0; n<test_count; n++) {
                *((wchar_t *)sbuf+n) = (wchar_t)comm_rank;
            }
            break;
        case DT_FLOAT:
            for (n=0; n<test_count; n++) {
                *((float *)sbuf+n) = (float)comm_rank;
            }
            break;
        case DT_DOUBLE:
            for (n=0; n<test_count; n++) {
                *((double *)sbuf+n) = (double)comm_rank;
            }
            break;
        case DT_LONG_DOUBLE:
            for (n=0; n<test_count; n++) {
                *((long double *)sbuf+n) = (long double)comm_rank;
            }
            break;
#if defined(MPIX_C_FLOAT16)
        case DT_C_FLOAT16:
            for (n=0; n<test_count; n++) {
                *((_Float16 *)sbuf+n) = (_Float16)comm_rank;
            }
            break;
#endif
        case DT_C_COMPLEX:
        case DT_C_FLOAT_COMPLEX:
            for (n=0; n<test_count; n++) {
                *((float _Complex *)sbuf+n) = comm_rank + 1.0i;
            }
            break;
        case DT_C_DOUBLE_COMPLEX:
            for (n=0; n<test_count; n++) {
                *((double _Complex *)sbuf+n) = comm_rank + 1.0i;
            }
            break;
        case DT_C_LONG_DOUBLE_COMPLEX:
            for (n=0; n<test_count; n++) {
                *((long double _Complex *)sbuf+n) = comm_rank + 1.0i;
            }
            break;
        case DT_SHORT_INT:
            for (n=0; n<test_count; n++) {
                ((short_int *)sbuf+n)->val = (short)comm_rank;  // value part
                ((short_int *)sbuf+n)->ind = 1;                 // index part
            };
            break;
        case DT_2INT:
            for (n=0; n<test_count; n++) {
                ((int_int *)sbuf+n)->val = comm_rank;   // value part
                ((int_int *)sbuf+n)->ind = 1;           // index part
            };
            break;
        case DT_LONG_INT:
            for (n=0; n<test_count; n++) {
                ((long_int *)sbuf+n)->val = comm_rank;  // value part
                ((long_int *)sbuf+n)->ind = 1;          // index part
            };
            break;
        default:
            fprintf(stderr, "[%s] invalid datatype: rank(w/c)=%d/%d func=%s datatype=%d\n",
                    __func__, wrank, comm_rank, FUNC_STR[test_func], test_datatype_n);
            return FUNC_ERROR;
    }
    return FUNC_SUCCESS;
}


/*
 * Set the test data and the answer for MPI_Reduce/MPI_Allreduce(MPI_MAX/MPI_MIN).
 */
static int set_data_for_reduce_max_min(void *sbuf, void *ansbuf, long buf_size) {

    int n;

    if (sbuf == NULL || ansbuf == NULL) {
        fprintf(stderr, "[%s] no buffer specified: rank(w/c)=%d/%d sbuf=%p ansbuf=%p\n",
                __func__, wrank, comm_rank, sbuf, ansbuf);
        return FUNC_ERROR;
    }
    memset(sbuf, 0x00, buf_size);
    memset(ansbuf, 0x00, buf_size);

    switch (test_datatype_n) {
        case DT_SIGNED_CHAR:
            for (n=0; n<test_count; n++) {
                *((signed char *)sbuf+n) = (signed char)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_MAX) {
                        *((signed char *)ansbuf+n) =
                            (signed char)(comm_procs>SCHAR_MAX ? SCHAR_MAX:(comm_procs-1));
                    }
                    else {
                        *((signed char *)ansbuf+n) = (signed char)(comm_procs>SCHAR_MAX ? SCHAR_MIN:0);
                    }
#if defined(TPDEBUG)
                    fprintf(stderr, "[%s] rank(w/c)=%d/%d comm_procs=%d ansbuf[%d]=%d\n",
                            __func__, wrank, comm_rank, comm_procs, n, *((signed char *)ansbuf+n));
#endif
                }
            }
            break;
        case DT_SHORT:
            for (n=0; n<test_count; n++) {
                *((short *)sbuf+n) = (short)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_MAX) {
                        *((short *)ansbuf+n) = (short)(comm_procs>SHRT_MAX ? SHRT_MAX:(comm_procs-1));
                    }
                    else {
                        *((short *)ansbuf+n) = (short)(comm_procs>SHRT_MAX ? SHRT_MIN:0);
                    }
                }
            }
            break;
        case DT_INT:
            for (n=0; n<test_count; n++) {
                *((int *)sbuf+n) = comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    *((int *)ansbuf+n) = (test_op_n==OP_MAX ? (comm_procs-1) : 0);
                }
            }
            break;
        case DT_LONG:
            for (n=0; n<test_count; n++) {
                *((long *)sbuf+n) = comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    *((long *)ansbuf+n) = (test_op_n==OP_MAX ? (comm_procs-1) : 0);
                }
            }
            break;
        case DT_LONG_LONG:
        case DT_LONG_LONG_INT:
            for (n=0; n<test_count; n++) {
                *((long long *)sbuf+n) = comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    *((long long *)ansbuf+n) = (test_op_n==OP_MAX ? (comm_procs-1) : 0);
                }
            }
            break;
        case DT_BYTE:
        case DT_UNSIGNED_CHAR:
            for (n=0; n<test_count; n++) {
                *((unsigned char *)sbuf+n) = (unsigned char)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_MAX) {
                        *((unsigned char *)ansbuf+n) =
                            (unsigned char)(comm_procs>UCHAR_MAX ? UCHAR_MAX:(comm_procs-1));
                    }
                    else {
                        *((unsigned char *)ansbuf+n) = 0;
                    }
                }
            }
            break;
        case DT_UNSIGNED_SHORT:
            for (n=0; n<test_count; n++) {
                *((unsigned short *)sbuf+n) = (unsigned short)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_MAX) {
                        *((unsigned short *)ansbuf+n) =
                            (unsigned short)(comm_procs>USHRT_MAX ? USHRT_MAX:(comm_procs-1));
                    }
                    else {
                        *((unsigned short *)ansbuf+n) = 0;
                    }
                }
            }
            break;
        case DT_UNSIGNED:
            for (n=0; n<test_count; n++) {
                *((unsigned int *)sbuf+n) = comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    *((unsigned int *)ansbuf+n) = (test_op_n==OP_MAX ? (comm_procs-1) : 0);
                }
            }
            break;
        case DT_UNSIGNED_LONG:
            for (n=0; n<test_count; n++) {
                *((unsigned long *)sbuf+n) = comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    *((unsigned long *)ansbuf+n) = (test_op_n==OP_MAX ? (comm_procs-1) : 0);
                }
            }
            break;
        case DT_UNSIGNED_LONG_LONG:
            for (n=0; n<test_count; n++) {
                *((unsigned long long *)sbuf+n) = comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    *((unsigned long long *)ansbuf+n) = (test_op_n==OP_MAX ? (comm_procs-1) : 0);
                }
            }
            break;
        case DT_INT8_T:
            for (n=0; n<test_count; n++) {
                *((int8_t *)sbuf+n) = (int8_t)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_MAX) {
                        *((int8_t *)ansbuf+n) = (int8_t)(comm_procs>INT8_MAX ? INT8_MAX:(comm_procs-1));
                    }
                    else {
                        *((int8_t *)ansbuf+n) = (int8_t)(comm_procs>INT8_MAX ? INT8_MIN:0);
                    }
                }
            }
            break;
        case DT_INT16_T:
            for (n=0; n<test_count; n++) {
                *((int16_t *)sbuf+n) = (int16_t)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_MAX) {
                        *((int16_t *)ansbuf+n) = (int16_t)(comm_procs>INT16_MAX ? INT16_MAX:(comm_procs-1));
                    }
                    else {
                        *((int16_t *)ansbuf+n) = (int16_t)(comm_procs>INT16_MAX ? INT16_MIN:0);
                    }
                }
            }
            break;
        case DT_INT32_T:
            for (n=0; n<test_count; n++) {
                *((int32_t *)sbuf+n) = comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    *((int32_t *)ansbuf+n) = (test_op_n==OP_MAX ? (comm_procs-1) : 0);
                }
            }
            break;
        case DT_INT64_T:
            for (n=0; n<test_count; n++) {
                *((int64_t *)sbuf+n) = comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    *((int64_t *)ansbuf+n) = (test_op_n==OP_MAX ? (comm_procs-1) : 0);
                }
            }
            break;
        case DT_UINT8_T:
            for (n=0; n<test_count; n++) {
                *((uint8_t *)sbuf+n) = (uint8_t)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_MAX) {
                        *((uint8_t *)ansbuf+n) = (uint8_t)(comm_procs>UINT8_MAX ? UINT8_MAX:(comm_procs-1));
                    }
                    else {
                        *((uint8_t *)ansbuf+n) = 0;
                    }
                }
            }
            break;
        case DT_UINT16_T:
            for (n=0; n<test_count; n++) {
                *((uint16_t *)sbuf+n) = (uint16_t)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_MAX) {
                        *((uint16_t *)ansbuf+n) = (uint16_t)(comm_procs>UINT16_MAX ? UINT16_MAX:(comm_procs-1));
                    }
                    else {
                        *((uint16_t *)ansbuf+n) = 0;
                    }
                }
            }
            break;
        case DT_UINT32_T:
            for (n=0; n<test_count; n++) {
                *((uint32_t *)sbuf+n) = comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    *((uint32_t *)ansbuf+n) = (test_op_n==OP_MAX ? (comm_procs-1) : 0);
                }
            }
            break;
        case DT_UINT64_T:
            for (n=0; n<test_count; n++) {
                *((uint64_t *)sbuf+n) = comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    *((uint64_t *)ansbuf+n) = (test_op_n==OP_MAX ? (comm_procs-1) : 0);
                }
            }
            break;
        case DT_FLOAT:
            for (n=0; n<test_count; n++) {
                *((float *)sbuf+n) = (float)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    *((float *)ansbuf+n) = (float)(test_op_n==OP_MAX ? (comm_procs-1) : 0);
                }
            }
            break;
        case DT_DOUBLE:
            for (n=0; n<test_count; n++) {
                *((double *)sbuf+n) = (double)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    *((double *)ansbuf+n) = (double)(test_op_n==OP_MAX ? (comm_procs-1) : 0);
                }
            }
            break;
        case DT_LONG_DOUBLE:
            for (n=0; n<test_count; n++) {
                *((long double *)sbuf+n) = (long double)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    *((long double *)ansbuf+n) = (long double)(test_op_n==OP_MAX ? (comm_procs-1) : 0);
                }
#if defined(TPDEBUG)
                fprintf(stderr, "[%s] rank(w/c)=%d/%d sbuf[%d]=%Lf\n",
                        __func__, wrank, comm_rank, n, *((long double *)sbuf+n));
#endif
            }
            break;
#if defined(MPIX_C_FLOAT16)
        case DT_C_FLOAT16:
            for (n=0; n<test_count; n++) {
                *((_Float16 *)sbuf+n) = (_Float16)comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    *((_Float16 *)ansbuf+n) = (_Float16)(test_op_n==OP_MAX ? (comm_procs-1) : 0);
                }
            }
            break;
#endif

        // Following types cannot be used in these reduction operations.
        case DT_WCHAR:
            for (n=0; n<test_count; n++) {
                *((wchar_t *)sbuf+n) = (wchar_t)comm_rank;
            }
            break;
        case DT_C_BOOL:
            for (n=0; n<test_count; n++) {
                *((bool *)sbuf+n) = (bool)comm_rank;
            }
            break;
        case DT_C_COMPLEX:
        case DT_C_FLOAT_COMPLEX:
            for (n=0; n<test_count; n++) {
                *((float _Complex *)sbuf+n) = comm_rank + 1.0i;
            }
            break;
        case DT_C_DOUBLE_COMPLEX:
            for (n=0; n<test_count; n++) {
                *((double _Complex *)sbuf+n) = comm_rank + 1.0i;
            }
            break;
        case DT_C_LONG_DOUBLE_COMPLEX:
            for (n=0; n<test_count; n++) {
                *((long double _Complex *)sbuf+n) = comm_rank + 1.0i;
            }
            break;
        case DT_SHORT_INT:
            for (n=0; n<test_count; n++) {
                ((short_int *)sbuf+n)->val = (short)n;  // value part
                ((short_int *)sbuf+n)->ind = 1;         // index part
            };
            break;
        case DT_2INT:
            for (n=0; n<test_count; n++) {
                ((int_int *)sbuf+n)->val = (int)n;    // value part
                ((int_int *)sbuf+n)->ind = 1;         // index part
            };
            break;
        case DT_LONG_INT:
            for (n=0; n<test_count; n++) {
                ((long_int *)sbuf+n)->val = n;  // value part
                ((long_int *)sbuf+n)->ind = 1;  // index part
            };
            break;
        default:
            fprintf(stderr, "[%s] invalid datatype: rank(w/c)=%d/%d func=%s datatype=%d\n",
                    __func__, wrank, comm_rank, FUNC_STR[test_func], test_datatype_n);
            return FUNC_ERROR;
    }
#if defined(TPDEBUG)
    fprintf(stderr, "[%s] rank(w/c)=%d/%d datatype=%s buf_size=%ld, data=\'",
            __func__, wrank, comm_rank, DATATYPE_STR[test_datatype_n], buf_size);
    for (n=0; n<buf_size; n++) {
        fprintf(stderr, "%02x ", *((unsigned char*)sbuf+n));
    }
    fprintf(stderr, "'\n");
#endif
    return FUNC_SUCCESS;
}


/*
 * Set the test data and the answer for MPI_Reduce/MPI_Allreduce(MPI_MAX_LOC/MPI_MIN_LOC).
 */
static int set_data_for_reduce_max_min_loc(void *sbuf, void *ansbuf, long buf_size) {

    int n;

    if (sbuf == NULL || ansbuf == NULL) {
        fprintf(stderr, "[%s] no buffer specified: rank(w/c)=%d/%d sbuf=%p ansbuf=%p\n",
                __func__, wrank, comm_rank, sbuf, ansbuf);
        return FUNC_ERROR;
    }
    memset(sbuf, 0x00, buf_size);
    memset(ansbuf, 0x00, buf_size);

    switch (test_datatype_n) {
        case DT_SHORT_INT:
            for (n=0; n<test_count; n++) {
                ((short_int *)sbuf+n)->val = (short)comm_rank;
                ((short_int *)sbuf+n)->ind = comm_rank;
#if TPDEBUG
                fprintf(stderr, "[%s] rank(w/c)=%d/%d sbuf  =%p sbuf  +n=%p (sbuf  +n)->ind=%p\n",
                        __func__, wrank, comm_rank, sbuf, (short_int *)sbuf+n, &(((short_int *)sbuf+n)->ind));
#endif
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    if (test_op_n == OP_MAXLOC) {
                        ((short_int *)ansbuf+n)->val =
                            (short)(comm_procs>SHRT_MAX ? SHRT_MAX:(comm_procs-1));
                    }
                    else {
                        ((short_int *)ansbuf+n)->val = 0;
                    }
                    ((short_int *)ansbuf+n)->ind = (test_op_n==OP_MAXLOC ? (comm_procs-1) : 0);
#if TPDEBUG
                    fprintf(stderr, "[%s] rank(w/c)=%d/%d ansbuf=%p ansbuf+n=%p (ansbuf+n)->ind=%p\n",
                            __func__, wrank, comm_rank, ansbuf, (short_int *)ansbuf+n,
                            &(((short_int *)ansbuf+n)->ind));
#endif
                }
            }
            break;
        case DT_2INT:
            for (n=0; n<test_count; n++) {
                ((int_int *)sbuf+n)->val = comm_rank;
                ((int_int *)sbuf+n)->ind = comm_rank;
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    ((int_int *)ansbuf+n)->val = (test_op_n==OP_MAXLOC ? (comm_procs-1) : 0);
                    ((int_int *)ansbuf+n)->ind = (test_op_n==OP_MAXLOC ? (comm_procs-1) : 0);
                }
            }
            break;
        case DT_LONG_INT:
            for (n=0; n<test_count; n++) {
                ((long_int *)sbuf+n)->val = comm_rank;
                ((long_int *)sbuf+n)->ind = comm_rank;
#if TPDEBUG
                fprintf(stderr, "[%s] rank(w/c)=%d/%d sbuf  =%p sbuf  +n=%p (sbuf  +n)->ind=%p\n",
                        __func__, wrank, comm_rank, sbuf, (long_int *)sbuf+n, &(((long_int *)sbuf+n)->ind));
#endif
                if (test_func == FUNC_MPI_ALLREDUCE || comm_rank == root_rank) {
                    ((long_int *)ansbuf+n)->val = (test_op_n==OP_MAXLOC ? (comm_procs-1) : 0);
                    ((long_int *)ansbuf+n)->ind = (test_op_n==OP_MAXLOC ? (comm_procs-1) : 0);
#if TPDEBUG
                    fprintf(stderr, "[%s] rank(w/c)=%d/%d ansbuf=%p ansbuf+n=%p (ansbuf+n)->ind=%p\n",
                            __func__, wrank, comm_rank, ansbuf, (long_int *)ansbuf+n,
                            &(((long_int *)ansbuf+n)->ind));
#endif
                }
            }
            break;

        // Following types cannot be used in these reduction operations.
        case DT_SIGNED_CHAR:
            for (n=0; n<test_count; n++) {
                *((char *)sbuf+n) = (char)comm_rank;
            }
            break;
        case DT_SHORT:
            for (n=0; n<test_count; n++) {
                *((short *)sbuf+n) = (short)comm_rank;
            }
            break;
        case DT_INT:
            for (n=0; n<test_count; n++) {
                *((int *)sbuf+n) = (int)comm_rank;
            }
            break;
        case DT_LONG:
            for (n=0; n<test_count; n++) {
                *((long *)sbuf+n) = (long)comm_rank;
            }
            break;
        case DT_LONG_LONG:
        case DT_LONG_LONG_INT:
            for (n=0; n<test_count; n++) {
                *((long long *)sbuf+n) = (long long)comm_rank;
            }
            break;
        case DT_BYTE:
        case DT_UNSIGNED_CHAR:
            for (n=0; n<test_count; n++) {
                *((unsigned char *)sbuf+n) = (unsigned char)comm_rank;
            }
            break;
        case DT_UNSIGNED_SHORT:
            for (n=0; n<test_count; n++) {
                *((unsigned short *)sbuf+n) = (unsigned short)comm_rank;
            }
            break;
        case DT_UNSIGNED:
            for (n=0; n<test_count; n++) {
                *((unsigned int *)sbuf+n) = (unsigned int)comm_rank;
            }
            break;
        case DT_UNSIGNED_LONG:
            for (n=0; n<test_count; n++) {
                *((unsigned long *)sbuf+n) = (unsigned long)comm_rank;
            }
            break;
        case DT_UNSIGNED_LONG_LONG:
            for (n=0; n<test_count; n++) {
                *((unsigned long long *)sbuf+n) = (unsigned long long)comm_rank;
            }
            break;
        case DT_INT8_T:
            for (n=0; n<test_count; n++) {
                *((int8_t *)sbuf+n) = (int8_t)comm_rank;
            }
            break;
        case DT_INT16_T:
            for (n=0; n<test_count; n++) {
                *((int16_t *)sbuf+n) = (int16_t)comm_rank;
            }
            break;
        case DT_INT32_T:
            for (n=0; n<test_count; n++) {
                *((int32_t *)sbuf+n) = (int32_t)comm_rank;
            }
            break;
        case DT_INT64_T:
            for (n=0; n<test_count; n++) {
                *((int64_t *)sbuf+n) = (int64_t)comm_rank;
            }
            break;
        case DT_UINT8_T:
            for (n=0; n<test_count; n++) {
                *((uint8_t *)sbuf+n) = (uint8_t)comm_rank;
            }
            break;
        case DT_UINT16_T:
            for (n=0; n<test_count; n++) {
                *((uint16_t *)sbuf+n) = (uint16_t)comm_rank;
            }
            break;
        case DT_UINT32_T:
            for (n=0; n<test_count; n++) {
                *((uint32_t *)sbuf+n) = (uint32_t)comm_rank;
            }
            break;
        case DT_UINT64_T:
            for (n=0; n<test_count; n++) {
                *((uint64_t *)sbuf+n) = (uint64_t)comm_rank;
            }
            break;
        case DT_WCHAR:
            for (n=0; n<test_count; n++) {
                *((wchar_t *)sbuf+n) = (wchar_t)comm_rank;
            }
            break;
        case DT_C_BOOL:
            for (n=0; n<test_count; n++) {
                *((bool *)sbuf+n) = (bool)comm_rank;
            }
            break;
        case DT_FLOAT:
            for (n=0; n<test_count; n++) {
                *((float *)sbuf+n) = (float)comm_rank;
            }
            break;
        case DT_DOUBLE:
            for (n=0; n<test_count; n++) {
                *((double *)sbuf+n) = (double)comm_rank;
            }
            break;
        case DT_LONG_DOUBLE:
            for (n=0; n<test_count; n++) {
                *((long double *)sbuf+n) = (long double)comm_rank;
            }
            break;
#if defined(MPIX_C_FLOAT16)
        case DT_C_FLOAT16:
            for (n=0; n<test_count; n++) {
                *((_Float16 *)sbuf+n) = (_Float16)comm_rank;
            }
            break;
#endif
        case DT_C_COMPLEX:
        case DT_C_FLOAT_COMPLEX:
            for (n=0; n<test_count; n++) {
                *((float _Complex *)sbuf+n) = comm_rank + 1.0i;
            }
            break;
        case DT_C_DOUBLE_COMPLEX:
            for (n=0; n<test_count; n++) {
                *((double _Complex *)sbuf+n) = comm_rank + 1.0i;
            }
            break;
        case DT_C_LONG_DOUBLE_COMPLEX:
            for (n=0; n<test_count; n++) {
                *((long double _Complex *)sbuf+n) = comm_rank + 1.0i;
            }
            break;
        default:
            fprintf(stderr, "[%s] invalid datatype: rank(w/c)=%d/%d func=%s datatype=%d\n",
                    __func__, wrank, comm_rank, FUNC_STR[test_func], test_datatype_n);
            return FUNC_ERROR;
    }
    return FUNC_SUCCESS;
}


/*
 * MPI_Barrier test
 */
static int test_barrier(void)
{
    int ret=MPI_SUCCESS, i;
    uint64_t cnt_start=0, cnt_end, cnt;
    utf_timer_t call_start=0, call_end=0;

    // Get the VBG counter.
    if (flg_check_vbg) {
        cnt_start = utf_bg_counter;
    }

    if (flg_measure) {
        call_start = get_time();
    }

    for (i = 0; i < iter; i++) {
        ret = MPI_Barrier(tcomm);
        if (ret != MPI_SUCCESS) {
            fprintf(stderr, "[%s] MPI_Barrier error: ret=%d rank(w/c)=%d/%d\n",
                    __func__, ret, wrank, comm_rank);
            break;
        }
    }

    if (flg_measure) {
        call_end = get_time();
    }

    // Get and check the VBG counter.
    if (flg_check_vbg) {
        cnt_end = utf_bg_counter;
        if (cnt_end >= cnt_start) {
            cnt = cnt_end - cnt_start;
        }
        else {
            cnt = (UINT64_MAX - cnt_start + 1) + cnt_end;
        }
        if (cnt == iter) {
            // VBG was called correctly.
            vbg_ret = VBG_PASS;
        }
        else {
            vbg_ret = VBG_NOT_PASS;
        }
    }

    if (flg_measure) {
        call_elapse = get_elapse(call_start, call_end);
    }
    return ret;
}


/*
 * Compare the recieve buffer and the answer buffer.
 */
static int comp_data(void *buf, void *ansbuf)
{
    int n;

    switch (test_datatype_n) {
        case DT_SIGNED_CHAR:
            for (n = 0; n < test_count; n++) {
                if (*((char *)buf+n) != *((char *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%hhd ansbuf[%d]=%hhd: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, *((char *)buf+n), n, *((char *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_SHORT:
            for (n = 0; n < test_count; n++) {
                if (*((short *)buf+n) != *((short *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%hd ansbuf[%d]=%hd: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, *((short *)buf+n), n, *((short *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_INT:
            for (n = 0; n < test_count; n++) {
                if (*((int *)buf+n) != *((int *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%d ansbuf[%d]=%d: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, *((int *)buf+n), n, *((int *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_LONG:
            for (n = 0; n < test_count; n++) {
                if (*((long *)buf+n) != *((long *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%ld ansbuf[%d]=%ld: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, *((long *)buf+n), n, *((long *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_LONG_LONG:
        case DT_LONG_LONG_INT:
            for (n = 0; n < test_count; n++) {
                if (*((long long *)buf+n) != *((long long *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%lld ansbuf[%d]=%lld: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, *((long long *)buf+n), n, *((long long *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_BYTE:
        case DT_UNSIGNED_CHAR:
            for (n = 0; n < test_count; n++) {
                if (*((unsigned char *)buf+n) != *((unsigned char *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%hhd ansbuf[%d]=%hhd: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, *((unsigned char *)buf+n), n, *((unsigned char *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_UNSIGNED_SHORT:
            for (n = 0; n < test_count; n++) {
                if (*((unsigned short *)buf+n) != *((unsigned short *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%hd ansbuf[%d]=%hd: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, *((unsigned short *)buf+n), n, *((unsigned short *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_UNSIGNED:
            for (n = 0; n < test_count; n++) {
                if (*((unsigned int *)buf+n) != *((unsigned int *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%d ansbuf[%d]=%d: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, *((unsigned int *)buf+n), n, *((unsigned int *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_UNSIGNED_LONG:
            for (n = 0; n < test_count; n++) {
                if (*((unsigned long *)buf+n) != *((unsigned long *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%ld ansbuf[%d]=%ld: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, *((unsigned long *)buf+n), n, *((unsigned long *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_UNSIGNED_LONG_LONG:
            for (n = 0; n < test_count; n++) {
                if (*((unsigned long long *)buf+n) != *((unsigned long long *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%lld ansbuf[%d]=%lld: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, *((unsigned long long *)buf+n), n, *((unsigned long long *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_INT8_T:
            for (n = 0; n < test_count; n++) {
                if (*((int8_t *)buf+n) != *((int8_t *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%hhd ansbuf[%d]=%hhd: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, *((int8_t *)buf+n), n, *((int8_t *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_INT16_T:
            for (n = 0; n < test_count; n++) {
                if (*((int16_t *)buf+n) != *((int16_t *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%hd ansbuf[%d]=%hd: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, *((int16_t *)buf+n), n, *((int16_t *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_INT32_T:
            for (n = 0; n < test_count; n++) {
                if (*((int32_t *)buf+n) != *((int32_t *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%d ansbuf[%d]=%d: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, *((int32_t *)buf+n), n, *((int32_t *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_INT64_T:
            for (n = 0; n < test_count; n++) {
                if (*((int64_t *)buf+n) != *((int64_t *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%ld ansbuf[%d]=%ld: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, *((int64_t *)buf+n), n, *((int64_t *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_UINT8_T:
            for (n = 0; n < test_count; n++) {
                if (*((uint8_t *)buf+n) != *((uint8_t *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%hhd ansbuf[%d]=%hhd: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, *((uint8_t *)buf+n), n, *((uint8_t *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_UINT16_T:
            for (n = 0; n < test_count; n++) {
                if (*((uint16_t *)buf+n) != *((uint16_t *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%hd ansbuf[%d]=%hd: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, *((uint16_t *)buf+n), n, *((uint16_t *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_UINT32_T:
            for (n = 0; n < test_count; n++) {
                if (*((uint32_t *)buf+n) != *((uint32_t *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%d ansbuf[%d]=%d: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, *((uint32_t *)buf+n), n, *((uint32_t *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_UINT64_T:
            for (n = 0; n < test_count; n++) {
                if (*((uint64_t *)buf+n) != *((uint64_t *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%ld ansbuf[%d]=%ld: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, *((uint64_t *)buf+n), n, *((uint64_t *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_WCHAR:
            for (n = 0; n < test_count; n++) {
                if (*((wchar_t *)buf+n) != *((wchar_t *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%d ansbuf[%d]=%d: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, *((wchar_t *)buf+n), n, *((wchar_t *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_C_BOOL:
            for (n = 0; n < test_count; n++) {
                if (*((bool *)buf+n) != *((bool *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%d ansbuf[%d]=%d: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, *((bool *)buf+n), n, *((bool *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_FLOAT:
            for (n = 0; n < test_count; n++) {
                if (*((float *)buf+n) != *((float *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%f ansbuf[%d]=%f: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, *((float *)buf+n), n, *((float *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_DOUBLE:
            for (n = 0; n < test_count; n++) {
                if (*((double *)buf+n) != *((double *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%lf ansbuf[%d]=%lf: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, *((double *)buf+n), n, *((double *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_LONG_DOUBLE:
            for (n = 0; n < test_count; n++) {
                if (*((long double *)buf+n) != *((long double *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%Lf ansbuf[%d]=%Lf: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, *((long double *)buf+n), n, *((long double *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
#if defined(MPIX_C_FLOAT16)
        case DT_C_FLOAT16:
            for (n = 0; n < test_count; n++) {
                if (*((_Float16 *)buf+n) != *((_Float16 *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%f ansbuf[%d]=%f: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, (double)*((_Float16 *)buf+n), n, (double)*((_Float16 *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
#endif
        case DT_C_COMPLEX:
        case DT_C_FLOAT_COMPLEX:
            for (n = 0; n < test_count; n++) {
                if (*((float _Complex *)buf+n) != *((float _Complex *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%f+%fi ansbuf[%d]=%f+%fi: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, __real__ *((float _Complex *)buf+n), __imag__ *((float _Complex *)buf+n),
                            n, __real__ *((float _Complex *)ansbuf+n), __imag__ *((float _Complex *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_C_DOUBLE_COMPLEX:
            for (n = 0; n < test_count; n++) {
                if (*((double _Complex *)buf+n) != *((double _Complex *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%lf+%lfi ansbuf[%d]=%lf+%lfi: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, __real__ *((double _Complex *)buf+n), __imag__ *((double _Complex *)buf+n),
                            n, __real__ *((double _Complex *)ansbuf+n), __imag__ *((double _Complex *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_C_LONG_DOUBLE_COMPLEX:
            for (n = 0; n < test_count; n++) {
                if (*((long double _Complex *)buf+n) != *((long double _Complex *)ansbuf+n)) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%Lf+%Lfi ansbuf[%d]=%Lf+%Lfi: "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, __real__ *((long double _Complex *)buf+n),
                            __imag__ *((long double _Complex *)buf+n), n,
                            __real__ *((long double _Complex *)ansbuf+n),
                            __imag__ *((long double _Complex *)ansbuf+n),
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_SHORT_INT:
            for (n = 0; n < test_count; n++) {
                if (((short_int *)buf+n)->val != ((short_int *)ansbuf+n)->val ||
                    ((short_int *)buf+n)->ind != ((short_int *)ansbuf+n)->ind) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%hd(%d) ansbuf[%d]=%hd(%d): "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, ((short_int *)buf+n)->val, ((short_int *)buf+n)->ind,
                            n, ((short_int *)ansbuf+n)->val, ((short_int *)ansbuf+n)->ind,
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_2INT:
            for (n = 0; n < test_count; n++) {
                if (((int_int *)buf+n)->val != ((int_int *)ansbuf+n)->val ||
                    ((int_int *)buf+n)->ind != ((int_int *)ansbuf+n)->ind) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%d(%d) ansbuf[%d]=%d(%d): "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, ((int_int *)buf+n)->val, ((int_int *)buf+n)->ind,
                            n, ((int_int *)ansbuf+n)->val, ((int_int *)ansbuf+n)->ind,
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        case DT_LONG_INT:
            for (n = 0; n < test_count; n++) {
                if (((long_int *)buf+n)->val != ((long_int *)ansbuf+n)->val ||
                    ((long_int *)buf+n)->ind != ((long_int *)ansbuf+n)->ind) {
                    fprintf(stderr,
                            "[%s] data mismatch: buf[%d]=%ld(%d) ansbuf[%d]=%ld(%d): "
                            "rank(w/c)=%d/%d datatype=%s count=%d\n",
                            __func__, n, ((long_int *)buf+n)->val, ((long_int *)buf+n)->ind,
                            n, ((long_int *)ansbuf+n)->val, ((long_int *)ansbuf+n)->ind,
                            wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count);
                    return FUNC_ERROR;
                }
            }
            break;
        default:
            fprintf(stderr, "[%s] unknown datatype: rank(w/c)=%d/%d datatype=%d count=%d\n",
                    __func__, wrank, comm_rank, test_datatype_n, test_count);
            return FUNC_ERROR;
    } // end switch

    return FUNC_SUCCESS;
}


/*
 * MPI_Bcast test
 */
static int test_bcast(char *buf, char *ansbuf, long buf_size)
{
    int ret=MPI_SUCCESS, lret=FUNC_SUCCESS, i;
    uint64_t cnt_start=0, cnt_end, cnt;
    utf_timer_t call_start=0, call_end=0, elapse_sum=0;

    // Get the VBG counter.
    if (flg_check_vbg) {
        cnt_start = utf_bg_counter;
    }

    if (set_data_for_bcast(buf, ansbuf, buf_size)) {
        return FUNC_ERROR;
    }

    for (i = 0; i < iter; i++) {
        if (comm_rank != root_rank) {
            memset(buf, 0x00, buf_size);
        }
        if (flg_measure) {
            call_start = get_time();
        }

        if (comm_rank == root_rank || !flg_diff_type) {
            ret = MPI_Bcast(buf, test_count, test_datatype, root_rank, tcomm);
        }
        else {
            // If the --diff_type option is specified, non-root rank use a different data type.
            // (Root rank always uses the test_datatype.)
            if (test_datatype != MPI_BYTE) {
                ret = MPI_Bcast(buf, buf_size, MPI_BYTE, root_rank, tcomm);
            }
            else {
                ret = MPI_Bcast(buf, buf_size, MPI_SIGNED_CHAR, root_rank, tcomm);
            }
        }
        if (ret != MPI_SUCCESS) {
            fprintf(stderr,
                    "[%s] MPI_Bcast error: ret=%d rank(w/c)=%d/%d datatype=%s count=%d diff_type=%d\n",
                    __func__, ret, wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count, flg_diff_type);
            return FUNC_ERROR;
        }

        if (flg_measure) {
            call_end = get_time();
            elapse_sum += call_end - call_start;
        }

        // Non-root ranks check if the result was received correctly.
        if (comm_rank != root_rank) {
            if (comp_data(buf, ansbuf) == FUNC_ERROR) {
                lret = FUNC_INCORRECT;
            }
        }
    }

    // Get and check the VBG counter.
    if (flg_check_vbg) {
        cnt_end = utf_bg_counter;
        if (cnt_end >= cnt_start) {
            cnt = cnt_end - cnt_start;
        }
        else {
            cnt = (UINT64_MAX - cnt_start + 1) + cnt_end;
        }
        if (cnt == iter) {
            // VBG was called correctly.
            vbg_ret = VBG_PASS;
        }
        else {
            vbg_ret = VBG_NOT_PASS;
        }
    }

    if (flg_measure) {
        call_elapse = get_call_elapse(elapse_sum);
    }
    return lret;
}


/*
 * MPI_Reduce test
 */
static int test_reduce(char *sendbuf, char *recvbuf, char *ansbuf, long buf_size)
{
    int ret, lret=FUNC_SUCCESS, i;
    char *rbufp;
    uint64_t cnt_start=0, cnt_end, cnt;
    utf_timer_t call_start=0, call_end=0, elapse_sum=0;

    // Get the VBG counter.
    if (flg_check_vbg) {
        cnt_start = utf_bg_counter;
    }

    // Set the input data.
    switch (test_op_n) {
        case OP_SUM:
            lret = set_data_for_reduce_sum(sendbuf, ansbuf, buf_size);
            break;
        case OP_BAND:
        case OP_BOR:
        case OP_BXOR:
            lret = set_data_for_reduce_bit(sendbuf, ansbuf, buf_size);
            break;
        case OP_LAND:
        case OP_LOR:
        case OP_LXOR:
            lret = set_data_for_reduce_logical(sendbuf, ansbuf, buf_size);
            break;
        case OP_MAX:
        case OP_MIN:
            lret = set_data_for_reduce_max_min(sendbuf, ansbuf, buf_size);
            break;
        case OP_MAXLOC:
        case OP_MINLOC:
            lret = set_data_for_reduce_max_min_loc(sendbuf, ansbuf, buf_size);
            break;
        default:
            fprintf(stderr, "[%s] unknown operation: rank(w/c)=%d/%d op=%d\n",
                    __func__, wrank, comm_rank, test_op_n);

            return FUNC_ERROR;
    }
    if (lret != FUNC_SUCCESS) {
        return FUNC_ERROR;
    }
#if TPDEBUG
    fprintf(stderr, "[%s:%d] set data success: rank(w/c)=%d/%d tcomm=%d\n",
            __func__, __LINE__, wrank, comm_rank, (int)tcomm);
#endif

    if (flg_use_in_place && comm_rank == root_rank) {
        // Use recvbuf as a temporary buffer for the send data.
        memcpy(recvbuf, sendbuf, buf_size);
    }

    for (i = 0; i < iter; i++) {
        if (flg_use_in_place && comm_rank == root_rank) {
            if (i > 0) {
                // Reset the send data using the data in a temporary buffer(recvbuf).
                memcpy(sendbuf, recvbuf, buf_size);
            }
        }
        else {
            if (recvbuf != NULL) {
                memset(recvbuf, 0x00, buf_size);
            }
        }

        if (flg_measure) {
            call_start = get_time();
        }

        if (flg_use_in_place && comm_rank == root_rank) {
            ret = MPI_Reduce(MPI_IN_PLACE, sendbuf, test_count, test_datatype, test_op, root_rank, tcomm);
        }
        else {
            ret = MPI_Reduce(sendbuf, recvbuf, test_count, test_datatype, test_op, root_rank, tcomm);
        }
        if (ret != MPI_SUCCESS) {
            fprintf(stderr,
                    "[%s] MPI_Reduce error: ret=%d rank(w/c)=%d/%d datatype=%s count=%d op=%s\n",
                    __func__, ret, wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count,
                    OP_STR[test_op_n]);
            return FUNC_ERROR;
        }

        if (flg_measure) {
            call_end = get_time();
            elapse_sum += call_end - call_start;
        }

        // Root rank checks if the result was received correctly.
        if (comm_rank == root_rank) {
            rbufp = (flg_use_in_place ? sendbuf : recvbuf);
            if (comp_data(rbufp, ansbuf) == FUNC_ERROR) {
                lret = FUNC_INCORRECT;
            }
        }
    }

    // Get and check the VBG counter.
    if (flg_check_vbg) {
        cnt_end = utf_bg_counter;
        if (cnt_end >= cnt_start) {
            cnt = cnt_end - cnt_start;
        }
        else {
            cnt = (UINT64_MAX - cnt_start + 1) + cnt_end;
        }
        if (cnt == iter) {
            // VBG was called correctly.
            vbg_ret = VBG_PASS;
        }
        else {
            vbg_ret = VBG_NOT_PASS;
        }
    }

    if (flg_measure) {
        call_elapse = get_call_elapse(elapse_sum);
    }
    return lret;
}


/*
 * MPI_Allreduce test
 */
static int test_allreduce(char *sendbuf, char *recvbuf, char *ansbuf, long buf_size)
{
    int ret, lret=FUNC_SUCCESS, i;
    char *rbufp;
    uint64_t cnt_start=0, cnt_end, cnt;
    utf_timer_t call_start=0, call_end=0, elapse_sum=0;

    // Get the VBG counter.
    if (flg_check_vbg) {
        cnt_start = utf_bg_counter;
    }

    // Set the input data.
    switch (test_op_n) {
        case OP_SUM:
            lret = set_data_for_reduce_sum(sendbuf, ansbuf, buf_size);
            break;
        case OP_BAND:
        case OP_BOR:
        case OP_BXOR:
            lret = set_data_for_reduce_bit(sendbuf, ansbuf, buf_size);
            break;
        case OP_LAND:
        case OP_LOR:
        case OP_LXOR:
            lret = set_data_for_reduce_logical(sendbuf, ansbuf, buf_size);
            break;
        case OP_MAX:
        case OP_MIN:
            lret = set_data_for_reduce_max_min(sendbuf, ansbuf, buf_size);
            break;
        case OP_MAXLOC:
        case OP_MINLOC:
            lret = set_data_for_reduce_max_min_loc(sendbuf, ansbuf, buf_size);
            break;
        default:
            fprintf(stderr, "[%s] unknown operation: rank(w/c)=%d/%d op=%d\n",
                    __func__, wrank, comm_rank, test_op_n);

            return FUNC_ERROR;
    }
    if (lret != FUNC_SUCCESS) {
        return FUNC_ERROR;
    }
#if TPDEBUG
    fprintf(stderr, "[%s:%d] set data success: rank(w/c)=%d/%d tcomm=%d\n",
            __func__, __LINE__, wrank, comm_rank, (int)tcomm);
#endif

    if (flg_use_in_place) {
        // Use recvbuf as a temporary buffer for the send data.
        memcpy(recvbuf, sendbuf, buf_size);
    }

    for (i = 0; i < iter; i++) {
        if (flg_use_in_place) {
            if (i > 0) {
                // Reset the send data using the data in a temporary buffer(recvbuf).
                memcpy(sendbuf, recvbuf, buf_size);
            }
        }
        else {
            memset(recvbuf, 0x00, buf_size);
        }

        if (flg_measure) {
            call_start = get_time();
        }

        if (flg_use_in_place) {
            ret = MPI_Allreduce(MPI_IN_PLACE, sendbuf, test_count, test_datatype, test_op, tcomm);
        }
        else {
            ret = MPI_Allreduce(sendbuf, recvbuf, test_count, test_datatype, test_op, tcomm);
        }
        if (ret != MPI_SUCCESS) {
            fprintf(stderr,
                    "[%s] MPI_Allreduce error: ret=%d rank(w/c)=%d/%d datatype=%s count=%d op=%s\n",
                    __func__, ret, wrank, comm_rank, DATATYPE_STR[test_datatype_n], test_count,
                    OP_STR[test_op_n]);
            return FUNC_ERROR;
        }

        if (flg_measure) {
            call_end = get_time();
            elapse_sum += call_end - call_start;
        }

        // All ranks check if the result was received correctly.
        rbufp = (flg_use_in_place ? sendbuf : recvbuf);
        if (comp_data(rbufp, ansbuf) == FUNC_ERROR) {
            lret = FUNC_INCORRECT;
        }
    }

    // Get and check the VBG counter.
    if (flg_check_vbg) {
        cnt_end = utf_bg_counter;
        if (cnt_end >= cnt_start) {
            cnt = cnt_end - cnt_start;
        }
        else {
            cnt = (UINT64_MAX - cnt_start + 1) + cnt_end;
        }
        if (cnt == iter) {
            // VBG was called correctly.
            vbg_ret = VBG_PASS;
        }
        else {
            vbg_ret = VBG_NOT_PASS;
        }
    }

    if (flg_measure) {
        call_elapse = get_call_elapse(elapse_sum);
    }
    return lret;
}


int main(int argc, char **argv)
{
#if EXTENSIVE_MEASUREMENT
    utf_timer_t start=0, end=0;
    double elapse=0.0;
#endif
    int ret=FUNC_SUCCESS, allret=FUNC_SUCCESS, all_vbg_ret=VBG_NOT_PASS, mret;
    int cnum;
    void *buf = NULL, *rbuf = NULL, *ansbuf = NULL;
    int type_size;
    long buf_size = 0;
    MPI_Group wgroup = 0;
    MPI_Group *grp_tbl_p = NULL;    // Pointer to the group table
    MPI_Comm *comm_tbl_p = NULL;    // Pointer to the communicator table

    // Initialize valiables.
    init_val();

#if TPDEBUG
    fprintf(stderr, "## [%d] mpi start!\n", wrank);
#endif
    mret = MPI_Init(&argc, &argv);
    if (mret != MPI_SUCCESS) {
        fprintf(stderr, "[%s:%d] MPI_Init error: ret=%d\n", __func__, __LINE__, ret);
        return FUNC_ERROR;
    }
    mret = MPI_Comm_size(MPI_COMM_WORLD, &wprocs);
    if (mret != MPI_SUCCESS) {
        fprintf(stderr, "[%s:%d] MPI_Comm_size error: ret=%d\n", __func__, __LINE__, ret);
        ret = FUNC_ERROR;
        goto err;
    }
    mret = MPI_Comm_rank(MPI_COMM_WORLD, &wrank);
    if (mret != MPI_SUCCESS) {
        fprintf(stderr, "[%s:%d] MPI_Comm_rank error: ret=%d\n", __func__, __LINE__, ret);
        ret = FUNC_ERROR;
        goto err;
    }
#if TPDEBUG
    fprintf(stderr, "## [%d] test start!\n", wrank);
#endif

    // Check the arguments and set the test patterns.
    if (check_args(argc, argv)) {
        ret = FUNC_ERROR;
        goto err;
    }

    // Print informations.
    if (!wrank && flg_print_info) {
        print_infos();
    }

#if !USE_WTIME
    freq = get_freq();
#endif

    // Get the size of the target datatype and calculate the required buffer size.
    if (test_func != FUNC_MPI_BARRIER) {
        if (test_datatype == MPI_SHORT_INT) {
            type_size = sizeof(short_int);
        }
        else if (test_datatype == MPI_2INT) {
            type_size = sizeof(int_int);
        }
        else if (test_datatype == MPI_LONG_INT) {
            type_size = sizeof(long_int);
        }
        else {
            mret = MPI_Type_size(test_datatype, &type_size);
            if (mret != MPI_SUCCESS) {
                fprintf(stderr, "[%s:%d] MPI_Type_size error: ret=%d rank(w/c)=%d/%d\n",
                        __func__, __LINE__, ret, wrank, comm_rank);
                ret = FUNC_ERROR;
                goto err;
            }
        }
        buf_size = (long)type_size * test_count;
        if (!wrank && flg_print_info) {
            fprintf(stdout, "[info] type_size       =%d\n", type_size);
            fprintf(stdout, "[info] buf_size        =%ld\n", buf_size);
            fflush(stdout);
        }
    }

    if (test_comm < COMM_SPLIT_FREE) {
        // Do not loop for COMM_WORLD/COMM_CREATE/COMM_DUP/COMM_SPLIT
        comm_count = 1;
    }

    if (test_comm == COMM_CREATE ||
        test_comm == COMM_CREATE_FREE ||
        test_comm == COMM_CREATE_NO_FREE) {
        // Get the group associated with MPI_COMM_WORLD.
        ret = MPI_Comm_group(MPI_COMM_WORLD, &wgroup);
        if (ret != MPI_SUCCESS) {
            fprintf(stderr, "[%s:%d] MPI_Comm_group error: ret=%d wrank=%d\n",
                    __func__, __LINE__, ret, wrank);
            ret = FUNC_ERROR;
            goto err;
        }
    }
    if (test_comm == COMM_CREATE_NO_FREE ||
        test_comm == COMM_DUP_NO_FREE    ||
        test_comm == COMM_SPLIT_NO_FREE) {
        // Allocate the communicator table.
        //   table size = comm_count+MPI_COMM_WORLD
        comm_tbl_p = (MPI_Comm *)malloc(sizeof(MPI_Comm)*(comm_count+1));
        if (comm_tbl_p == NULL) {
            fprintf(stderr, "[%s:%d] cannot allocate memory: rank(w)=%d\n",
                    __func__, __LINE__, wrank);
            ret = FUNC_ERROR;
            goto err;
        }
        memset(comm_tbl_p, 0x00, sizeof(MPI_Comm)*(comm_count+1));
        comm_tbl_p[0] = MPI_COMM_WORLD;

        if (test_comm == COMM_CREATE_NO_FREE) {
            // Allocate the group table.
            //   table size = comm_count+MPI_COMM_WORLD's group
            grp_tbl_p = (MPI_Group *)malloc(sizeof(MPI_Group)*(comm_count+1));
            if (grp_tbl_p == NULL) {
                fprintf(stderr, "[%s:%d] cannot allocate memory: rank(w)=%d\n",
                        __func__, __LINE__, wrank);
                ret = FUNC_ERROR;
                goto err;
            }
            memset(grp_tbl_p, 0x00, sizeof(MPI_Group)*(comm_count+1));
            grp_tbl_p[0] = wgroup;
        }

        comm_rank = wrank;
        comm_procs = wprocs;
    }

    for (cnum = 0; cnum < comm_count; cnum++) {

        // Create the communicator (and the group).
        if (test_comm == COMM_CREATE_NO_FREE) {
            ret = create_comm(comm_tbl_p[cnum], comm_rank, comm_procs, grp_tbl_p[cnum]);
        }
        else if ( test_comm == COMM_SPLIT_NO_FREE) {
            ret = create_comm(comm_tbl_p[cnum], comm_rank, comm_procs, wgroup);
        }
        else {
            ret = create_comm(MPI_COMM_WORLD, wrank, wprocs, wgroup);
        }
        if (ret == FUNC_ERROR) {
            if (test_comm == COMM_CREATE_NO_FREE ||
                test_comm == COMM_SPLIT_NO_FREE) {
                fprintf(stderr,
                        "[%s:%d] create_comm error: rank(w/c)=%d/%d cnum=%d procs=%d test_comm=%d\n",
                        __func__, __LINE__, wrank, comm_rank, cnum, comm_procs, test_comm);
            }
            else {
                fprintf(stderr,
                        "[%s:%d] create_comm error: rank(w)=%d cnum=%d procs=%d test_comm=%d\n",
                        __func__, __LINE__, wrank, cnum, wprocs, test_comm);
            }
            goto err;
        }
#if TPDEBUG
        fprintf(stderr, "[%s:%d] create_comm success: rank(w/c)=%d/%d cnum=%d test_comm=%d tcomm=%d\n",
                __func__, __LINE__, wrank, comm_rank, cnum, test_comm, (int)tcomm);
#endif
        if (test_comm == COMM_CREATE_NO_FREE ||
            test_comm == COMM_DUP_NO_FREE    ||
            test_comm == COMM_SPLIT_NO_FREE) {
            comm_tbl_p[cnum+1] = tcomm;
            if (test_comm == COMM_CREATE_NO_FREE) {
                grp_tbl_p[cnum+1] = tgroup;
            }
        }

        // Execute the barrier tests.
        switch(test_func) {
            case FUNC_MPI_BARRIER:
#if EXTENSIVE_MEASUREMENT
                if (flg_measure) {
                    start = get_time();
                }
#endif

                ret = test_barrier();

#if EXTENSIVE_MEASUREMENT
                if (flg_measure) {
                    end = get_time();
                    elapse = get_elapse(start, end);
                }
#endif
                break;

            case FUNC_MPI_BCAST:
                if (allocate_buffer(&buf, NULL, &ansbuf, buf_size)) {
                    ret = FUNC_ERROR;
                    goto err;
                }
#if EXTENSIVE_MEASUREMENT
                if (flg_measure) {
                    start = get_time();
                }
#endif

                ret = test_bcast(buf, ansbuf, buf_size);

#if EXTENSIVE_MEASUREMENT
                if (flg_measure) {
                    end = get_time();
                    elapse = get_elapse(start, end);
                }
#endif
                break;

            case FUNC_MPI_REDUCE:
                if (allocate_buffer(&buf, &rbuf, &ansbuf, buf_size)) {
                    ret = FUNC_ERROR;
                    goto err;
                }
#if TPDEBUG
                if (flg_print_info) {
                    fprintf(stderr, "[%d/%d] buf     =%p\n", wrank, comm_rank, buf);
                    fprintf(stderr, "[%d/%d] rbuf    =%p\n", wrank, comm_rank, rbuf);
                    fprintf(stderr, "[%d/%d] ansbuf  =%p\n", wrank, comm_rank, ansbuf);
                }
#endif
#if EXTENSIVE_MEASUREMENT
                if (flg_measure) {
                    start = get_time();
                }
#endif

                ret = test_reduce(buf, rbuf, ansbuf, buf_size);

#if EXTENSIVE_MEASUREMENT
                if (flg_measure) {
                    end = get_time();
                    elapse = get_elapse(start, end);
                }
#endif
                break;

            case FUNC_MPI_ALLREDUCE:
                if (allocate_buffer(&buf, &rbuf, &ansbuf, buf_size)) {
                    ret = FUNC_ERROR;
                    goto err;
                }
#if TPDEBUG
                if (flg_print_info) {
                    fprintf(stderr, "[%d/%d] buf     =%p\n", wrank, comm_rank, buf);
                    fprintf(stderr, "[%d/%d] rbuf    =%p\n", wrank, comm_rank, rbuf);
                    fprintf(stderr, "[%d/%d] ansbuf  =%p\n", wrank, comm_rank, ansbuf);
                }
#endif
#if EXTENSIVE_MEASUREMENT
                if (flg_measure) {
                    start = get_time();
                }
#endif

                ret = test_allreduce(buf, rbuf, ansbuf, buf_size);

#if EXTENSIVE_MEASUREMENT
                if (flg_measure) {
                    end = get_time();
                    elapse = get_elapse(start, end);
                }
#endif
                break;
        }

        // Check if all ranks in MPI_COMM_WORLD got the correct value.
        mret = MPI_Reduce(&ret, &allret, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);
        if (mret != MPI_SUCCESS) {
            fprintf(stderr, "[%s:%d] MPI_Reduce error: ret=%d wrank=%d\n",
                    __func__, __LINE__, ret, wrank);
            ret = FUNC_ERROR;
            goto err;
        }
        // Check if all ranks in MPI_COMM_WORLD has passed VBG.
        mret = MPI_Reduce(&vbg_ret, &all_vbg_ret, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);
        if (mret != MPI_SUCCESS) {
            fprintf(stderr, "[%s:%d] MPI_Reduce error: ret=%d wrank=%d\n",
                    __func__, __LINE__, ret, wrank);
            ret = FUNC_ERROR;
            goto err;
        }
#if TPDEBUG
        fprintf(stderr, "[%s:%d] test(%d) end: rank(w/c)=%d/%d test_comm=%d tcomm=%d ret=%d\n",
                __func__, __LINE__, cnum, wrank, comm_rank, test_comm, (int)tcomm, ret);
#endif

        if (ret != FUNC_SUCCESS || (wrank==0 && allret != FUNC_SUCCESS)) {
            // End the test when an error occurs.
            break;
        }

        // Free the communicator (and the group in the case of COMM_CREATE/COMM_CREATE_FREE).
        if (test_comm < COMM_SPLIT_NO_FREE) {
            if (free_comm()) {
                ret = FUNC_ERROR;
                goto err;
            }
#if TPDEBUG
            fprintf(stderr, "[%s:%d] free_comm success: rank(w/c)=%d/%d\n",
                    __func__, __LINE__, wrank, comm_rank);
#endif
        }
    } // end for

    if (wgroup != 0) {
#if TPDEBUG
        fprintf(stderr, "[%s:%d] call MPI_Group_free: rank(w/c)=%d/%d wgroup=%d\n",
                __func__, __LINE__, wrank, comm_rank, (int)wgroup);
#endif
        // Free the group associated with MPI_COMM_WORLD.
        ret = MPI_Group_free(&wgroup);
        if (ret != MPI_SUCCESS) {
            fprintf(stderr, "[%s] MPI_Group_free error: ret=%d rank(w/c)=%d/%d\n",
                    __func__, ret, wrank, comm_rank);
            ret = FUNC_ERROR;
            goto err;
        }
    }

    // For COMM_*_NO_FREE, free the communicator and the group after the test.
    if (test_comm == COMM_DUP_NO_FREE    ||
        test_comm == COMM_SPLIT_NO_FREE) {
        for (cnum = 1; cnum <= comm_count; cnum++) {
            tcomm = comm_tbl_p[cnum];
            if (free_comm()) {
                ret = FUNC_ERROR;
                goto err;
            }
            comm_tbl_p[cnum] = 0;
        }
    }
    else if (test_comm == COMM_CREATE_NO_FREE) {
        for (cnum = 1; cnum <= comm_count; cnum++) {
            tcomm = comm_tbl_p[cnum];
            tgroup = grp_tbl_p[cnum];
            if (free_comm()) {
                ret = FUNC_ERROR;
                goto err;
            }
            comm_tbl_p[cnum] = 0;
            grp_tbl_p[cnum] = 0;
        }
    }

#if TPDEBUG
    fprintf(stderr, "## [%d] test end!\n", wrank);
#endif

err:
    if (buf != NULL) {
        free(buf);
        buf = NULL;
    }
    if (rbuf != NULL) {
        free(rbuf);
        rbuf = NULL;
    }
    if (ansbuf != NULL) {
        free(ansbuf);
        ansbuf = NULL;
    }
    if (grp_tbl_p != NULL) {
        free(grp_tbl_p);
        grp_tbl_p = NULL;
    }
    if (comm_tbl_p != NULL) {
        free(comm_tbl_p);
        comm_tbl_p = NULL;
    }
#if TPDEBUG
    fprintf(stderr, "[%s:%d] call MPI_Fnalize: rank(w)=%d\n", __func__, __LINE__, wrank);
#endif
    mret = MPI_Finalize();
    if (mret != MPI_SUCCESS) {
        fprintf(stderr, "[%s:%d] MPI_Finalize error: ret=%d wrank=%d\n",
                __func__, __LINE__, ret, wrank);
        ret = FUNC_ERROR;
    }
#if TPDEBUG
    fprintf(stderr, "## [%d] mpi end!\n", wrank);
#endif

    if (ret != FUNC_SUCCESS) {
        allret = ret;
    }

    if (!wrank) {
        msgtmp[0] = '\0';
        msgtmp2[0] = '\0';
        if (flg_measure) {
#if EXTENSIVE_MEASUREMENT
            sprintf(msgtmp, "%.3lf,%.3lf", elapse, call_elapse);
#else
            sprintf(msgtmp, "%.3lf", call_elapse);
#endif
        }
        if (flg_check_vbg) {
            sprintf(msgtmp2, "%s", (all_vbg_ret==VBG_PASS?"passed":"-"));
        }
        // output: "<MPI test results>,<VBG results>,<Performance results(usec)>"
        fprintf(stdout, "%9s,%6s,%7s\n",
                (allret==FUNC_SUCCESS   ? "correct":
                (allret==FUNC_INCORRECT ? "incorrect":"error")), msgtmp2, msgtmp);
        fflush(stdout);
    }

    return ret;
}
