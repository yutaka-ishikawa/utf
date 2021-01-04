#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-Allreduce64" # jobname
#PJM -S		# output statistics
#PJM --spath "results/coll-allreduce64/%n.%j.stat"
#PJM -o "results/coll-allreduce64/%n.%j.out"
#PJM -e "results/coll-allreduce64/%n.%j.err"
#
#PJM -L "node=16:noncont"
#PJM --mpi "max-proc-per-node=4"
#PJM -L "elapse=00:04:00"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-all,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-mck2_and_spack2,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#
MPIOPT="-of results/coll-allreduce64/%n.%j.out -oferr results/coll-allreduce64/%n.%j.err"
# max RMA 8388608
###export MPIR_CVAR_REDUCE_SHORT_MSG_SIZE=100000

#
#   coll -s  0x1: Barrier, 0x2: Reduce, 0x4: Allreduce, 0x8: Gather, 0x10: Alltoall, 0x20: Scatter
#	     0x40: Gatherv, 0x80: Bcast
#
export MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG=1
export MPICH_TOFU_SHOW_PARAMS=1
export UTF_INFO=0x1
#export UTF_DEBUG=0xffffff
#export FI_LOG_PROV=tofu
#export FI_LOG_LEVEL=Debug
#export UTF_INJECT_COUNT=1
export UTF_INJECT_COUNT=4
export UTF_DEBUG=0x200	# DLEVEL_ERR
#export UTF_DBGTIMER_INTERVAL=40
#export UTF_DBGTIMER_ACTION=1

NP=64
ITER=1000
#ITER=1
VRYFY="-V 1"
BARRIER="-b"
#for LEN in 8192 65536 524288 #size in double
#for LEN in 512 # 4 sec
for LEN in 8 128 256 512 1024 2048 #size in double
do
    echo;echo
    mpich_exec -n $NP $MPIOPT ../src/colld -l $LEN -i $ITER -s 0x4 $VRYFY # Reduce
    unset MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG
    unset MPICH_TOFU_SHOW_PARAMS
    unset UTF_INFO
#    echo;echo
#    mpich_exec -n $NP $MPIOPT ../src/colld -l $LEN -i $ITER -s 0x80 $VRYFY # Bcast
done
#mpich_exec -n $NP $MPIOPT ../src/colld -l $LEN -i $ITER -s 0x10	$VRYFY # Alltoall

exit

