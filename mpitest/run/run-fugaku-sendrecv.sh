#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-SENDRECV" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#
#PJM -L "node=2"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:01:00"
#PJM -L "rscunit=rscunit_ft01,rscgrp=small"
#PJM -L proc-core=unlimited
#------- Program execution -------#

. ./mpich.env
MPIOPT="-of results/%n.%j.out -oferr results/%n.%j.err"
export PATH=~/mpich-tofu-cross/bin:$PATH
export MPICH_HOME=~/mpich-tofu-cross/
which mpich_exec
export MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG=1
export MPICH_TOFU_SHOW_PARAMS=1

ITER=10
for LEN in 100 200 300 400 500 600 700 800 1000
do
    echo
    echo "SENDONE " $LEN
    echo "mpiexec ../bin/sendrecv -l " $LEN " -i " $ITER
    mpich_exec $MPIOPT ../bin/sendrecv -l $LEN -i $ITER
    unset MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG
    unset MPICH_TOFU_SHOW_PARAMS
done
exit

