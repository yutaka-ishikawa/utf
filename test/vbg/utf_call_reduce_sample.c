#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <mpi.h>

#include <utofu.h>
#include <jtofu.h>
#include <utf.h>
#include <utf_bg.h>


#include <sys/times.h>

extern int make_vname(int, int);

extern int myprintf(const char *fmt, ...);

#define NUM_REPEAT  100000
#define CYCLES
#ifdef CYCLES
typedef uint64_t opal_timer_t;

static inline opal_timer_t
opal_sys_timer_get_cycles(void)
{
opal_timer_t ret;

__asm__ __volatile__ ("isb" ::: "memory");
__asm__ __volatile__ ("mrs %0,  CNTVCT_EL0" : "=r" (ret));

return ret;
}


static inline opal_timer_t
opal_sys_timer_freq(void)
{
opal_timer_t freq;
__asm__ __volatile__ ("mrs %0,  CNTFRQ_EL0" : "=r" (freq));
return (opal_timer_t)(freq);
}
#else
#endif

int main(int argc, char *argv[]) {
    int rc, num_processes, rank;
    size_t num_tnis;
    utofu_tni_id_t tni_id, *tni_ids;
    //utofu_vcq_hdl_t lcl_vcq_hdl;
    uint32_t *rankset;
    utf_bg_info_t *lcl_bginfo, *rmt_bginfo;
    utf_coll_group_t utf_grp;
#ifdef CYCLES
    opal_timer_t cycles_start, cycles_end, cycles_start_b, cycles_end_b, cycles_start_i, cycles_end_i, freq;
    double time, maxtime, maxtimeb, maxtimei;
#else
    double start, end, startb, endb, starti, starti;
#endif
    int i;
    double input, result, *data;
    uint64_t inputi, resulti, *datai;

    struct timeval tv1, tv2;
    double timec[4]={1.0,1.0,1.0,1.0}, maxtimec[4];
    volatile int  endloop=0;

    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &num_processes);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // rankset格納用のメモリを取得する。
    rankset = malloc(sizeof(uint32_t) * num_processes);

    // bginfo格納用のメモリを取得する。
    lcl_bginfo = malloc(sizeof(utf_bg_info_t) * num_processes);
    rmt_bginfo = malloc(sizeof(utf_bg_info_t) * num_processes);

    for(i=0; i<num_processes; i++){
      *(rankset+i) = i;
    }

    if (UTF_SUCCESS != make_vname(rank,num_processes)) {
      MPI_Abort(MPI_COMM_WORLD,1);
    }

    ////////////////////////////////////////
    // バリア回路の作成に必要な資源を確保する。
    rc = utf_bg_alloc(rankset, num_processes, rank, jtofu_query_max_proc_per_node(), UTF_BG_TOFU, lcl_bginfo);
    if (rc != UTF_SUCCESS) goto err;

    rc = MPI_Alltoall(lcl_bginfo, sizeof(utf_bg_info_t), MPI_BYTE,        
                      rmt_bginfo, sizeof(utf_bg_info_t), MPI_BYTE, MPI_COMM_WORLD);
    if (rc != MPI_SUCCESS) goto err;

    // バリア回路を作成する。
    rc = utf_bg_init(rankset, num_processes, rank, rmt_bginfo, &utf_grp);
    if (rc != UTF_SUCCESS) goto err;

    //utf_setgrp2mpicomm(MPI_COMM_WORLD, utf_grp);
    // 全てのプロセスでバリア回路が作成されたことを保証する。
 
    rc = MPI_Barrier(MPI_COMM_WORLD);
    if (rc != MPI_SUCCESS) goto err;
    
    // bginfo格納用のメモリを解放する。
    free(lcl_bginfo);
    free(rmt_bginfo);

    // rankset格納用のメモリを解放する。
    free(rankset);

    /////////////////////////////////////////////////////////////////////////////

#ifdef CYCLES
freq=opal_sys_timer_freq();
#else
#endif

//warm up
 for(i=0; i<1000000; i++){
   // バリアを実行する。
   rc = utf_barrier(utf_grp);
   if (rc != UTF_SUCCESS) goto err;

   // バリアの実行完了を確認する。
   while (utf_poll_barrier (utf_grp) == UTF_ERR_NOT_COMPLETED){
     //progress
   }
   if (rc != UTF_SUCCESS) goto err;

 }


 
//1st

#ifdef CYCLES
cycles_start_b = opal_sys_timer_get_cycles();
#else
start = MPI_Wtime();
#endif
 for(i=0; i<NUM_REPEAT; i++){
   // バリアを実行する。
   rc = utf_barrier(utf_grp);
   if (rc != UTF_SUCCESS) goto err;

   // バリアの実行完了を確認する。
   while (utf_poll_barrier (utf_grp) == UTF_ERR_NOT_COMPLETED){
     //progress
   }
   if (rc != UTF_SUCCESS) goto err;

 }
#ifdef CYCLES
cycles_end_b = opal_sys_timer_get_cycles();
#else
end = MPI_Wtime();
#endif

//warm up
 for(i=0; i<1000000; i++){
     rc = utf_reduce(utf_grp, &input, 1, NULL, &result, NULL, UTF_DATATYPE_DOUBLE, UTF_REDUCE_OP_SUM, 0);
     if (rc != UTF_SUCCESS) goto err;

     while (utf_poll_reduce (utf_grp, (void **)&data) == UTF_ERR_NOT_COMPLETED){
       //progress
     }
     if (rc != UTF_SUCCESS) goto err;
 }



//2nd
#ifdef CYCLES
 cycles_start = opal_sys_timer_get_cycles();
#else
 start = MPI_Wtime();
#endif
 input = 0.1 * rank;
 for(i=0; i<NUM_REPEAT; i++){
   rc = utf_reduce(utf_grp, &input, 1, NULL, &result, NULL, UTF_DATATYPE_DOUBLE, UTF_REDUCE_OP_SUM, 0);
   if (rc != UTF_SUCCESS) goto err;

   while (utf_poll_reduce (utf_grp, (void **)&data) == UTF_ERR_NOT_COMPLETED){
     //progress
   }
   if (rc != UTF_SUCCESS) goto err;
 }
#ifdef CYCLES
 cycles_end = opal_sys_timer_get_cycles();
#else
 end = MPI_Wtime();
#endif

    ///////////////////////////////////////
    // バリア資源を解放する。
    rc = utf_bg_free(utf_grp);
    if (rc != UTF_SUCCESS) goto err;

    ///////////////////////////////////////

#ifdef CYCLES
    time = (((double)cycles_end_b-cycles_start_b)/freq)*1000000.0/NUM_REPEAT;
    MPI_Reduce(&time, &maxtimeb, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    time = (((double)cycles_end-cycles_start)/freq)*1000000.0/NUM_REPEAT;
    MPI_Reduce(&time, &maxtime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
#else
    time = (endb-startb)/freq)*1000000.0/NUM_REPEAT;
    MPI_Reduce(&time, &maxtimeb, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    time = (end-start)*1000000.0/NUM_REPEAT;
    MPI_Reduce(&time, &maxtime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
#endif
    if (!rank){
      printf("!!!!!!!!!!!!!!!!!!!!!!!!!  Result=%lf:%lf:  B: %lf  R: %lf \n", *data, result, maxtimeb, maxtime);fflush(stdout);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    return 0;

 err:
    myprintf("ABORT\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
    return 1;

}

