#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-EXP-PINGPONG" # jobname
#	PJM -N "MPICH-NORMAL-PINGPONG" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#
#PJM -L "node=2"
#PJM --mpi "max-proc-per-node=1"
#	PJM -L "elapse=00:04:10"
#	PJM -L "elapse=00:01:50"
#PJM -L "elapse=00:01:10"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#
SAVED_LD_LIBRARY_PATH=$LD_LIBRARY_PATH

MPIOPT="-of results/%n.%j.out -oferr results/%n.%j.err"
#MIN_LEN=1024
#MAX_LEN=1024
#MIN_LEN=2048
#MAX_LEN=2048
#MIN_LEN=1
#MAX_LEN=1048576
#ITER=1000
#MIN_LEN=4096
#MAX_LEN=4096
#MAX_LEN=1048576
MIN_LEN=1
MAX_LEN=134217728
ITER=100

export MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG=1
export MPICH_TOFU_SHOW_PARAMS=1
#export UTF_TIMER=1
#export UTF_DEBUG=0xfffff
#export UTF_DEBUG=0x1124
#export UTF_DEBUG=0x24
#export FI_LOG_PROV=tofu
#export FI_LOG_LEVEL=Debug

echo "******* MPICH EXP ******"

echo "checking pingpong"
mpich_exec -n 2 $MPIOPT ../bin/pingpong -L $MAX_LEN -l $MIN_LEN -i $ITER

#mpich_exec -n 2 $MPIOPT strace ../bin/pingpong -L $MAX_LEN -l $MIN_LEN -i $ITER
exit
