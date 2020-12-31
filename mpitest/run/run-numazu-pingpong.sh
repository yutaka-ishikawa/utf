#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-PINGPONG" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#
#PJM -L "node=2"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:04:10"
#	PJM -L "elapse=00:00:10"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#
SAVED_LD_LIBRARY_PATH=$LD_LIBRARY_PATH

MPIOPT="-of results/%n.%j.out -oferr results/%n.%j.err"
MAX_LEN=1048576	# 1 MB
#MAX_LEN=134217728	# 128 MB
MIN_LEN=1
ITER=1000
VRYFY="-V 1"

export MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG=1
export MPICH_TOFU_SHOW_PARAMS=1
export UTF_INFO=0x1
#export UTF_DEBUG=0xffffff
#export FI_LOG_PROV=tofu
#export FI_LOG_LEVEL=Debug
export UTF_COMDEBUG=1

echo "********MPICH EXP*********"
echo "checking pingpong"

mpich_exec -n 2 $MPIOPT ../bin/pingpong -L $MAX_LEN -l $MIN_LEN -i $ITER $VRYFY

export LD_LIBRARY_PATH=$SAVED_LD_LIBRARY_PATH
echo; echo
echo "FJMPI"
mpiexec -n 2 $MPIOPT ../bin/pingpong-f -L $MAX_LEN -l $MIN_LEN -i $ITER $VRYFY
exit
