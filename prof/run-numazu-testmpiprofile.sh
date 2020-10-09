#!/bin/sh
#------ pjsub option --------#
#PJM -N "TEST_PROFILE" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#
#PJM -L "node=2"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:00:10"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#
export LD_LIBRARY_PATH=$HOME/mpich-tofu/lib:$LD_LIBRARY_PATH
export MPIR_CVAR_OFI_USE_PROVIDER=tofu
export MPICH_CH4_OFI_ENABLE_SCALABLE_ENDPOINTS=1
export MPIR_CVAR_ALLTOALL_SHORT_MSG_SIZE=2147483647 # 32768 in default (integer value)  
##export UTF_MSGMODE=1	# Rendezous
export UTF_MSGMODE=0	# Eager
export TOFU_NAMED_AV=1
export UTF_TRANSMODE=1	# Aggressive
export UTF_MSGMODE=1

echo "LD_LIBRARY_PATH=" $LD_LIBRARY_PATH
echo "TOFU_NAMED_AV = " $TOFU_NAMED_AV
echo "UTF_MSGMODE    = " $UTF_MSGMODE
echo "UTF_TRANSMODE = " $UTF_TRANSMODE "(0: Chained, 1: Aggressive)"
echo "UTF_DEBUG      = " $UTF_DEBUG
echo "TOFULOG_DIR    = " $TOFULOG_DIR
echo "TOFU_DEBUG_FD  = " $TOFU_DEBUG_FD
echo "TOFU_DEBUG_LVL = " $TOFU_DEBUG_LVL

export OMP_PROC_BIND=close
[ -d ./profdata ] || mkdir ./profdata
PROF_DIR=./profdata/rep
export FLIB_FASTOMP=TRUE

for i in `seq 1 17`
do
    echo "***** pa$i ****"  
    echo fapp -C -d ${PROF_DIR}$i -Icpupa,nompi -Hevent=pa$i mpiexec -np 2 ./test_mpiprofile
    fapp -C -d ${PROF_DIR}$i -Icpupa,nompi -Hevent=pa$i mpiexec -np 2 ./test_mpiprofile
    echo -e "[RETURN-VAL]: $?\n"
done
