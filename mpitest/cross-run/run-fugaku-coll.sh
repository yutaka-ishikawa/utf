#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-COLL" # jobname
#PJM -S		# output statistics
#
#PJM --spath "results/coll/%n.%j.stat"
#PJM -o "results/coll/%n.%j.out"
#PJM -e "results/coll/%n.%j.err"
#PJM -L "node=4"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:01:00"
#PJM -L "rscunit=rscunit_ft01,rscgrp=small"
#PJM -L proc-core=unlimited
#------- Program execution -------#

MPIOPT="-of results/coll/%n.%j.out -oferr results/coll/%n.%j.err"
export PATH=~/mpich-tofu-cross/bin:$PATH
export MPICH_HOME=~/mpich-tofu-cross/
which mpich_exec
export MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG=1
export MPICH_TOFU_SHOW_PARAMS=1
export UTF_TRANSMODE=0

#
#   coll -s  0x1: Barrier, 0x2: Reduce, 0x4: Allreduce, 0x8: Gather, 0x10: Alltoall, 0x20: Scatter
#
for LEN in 100 200 300 400 500 600 700 800 1000
do
    echo
    echo "COLL(Reduce) " $LEN
    echo "mpich_exec $MPIOPT ../bin/coll -s 0x2 -l " $LEN
    mpich_exec $MPIOPT ../bin/coll -l $LEN -s 0x2
    unset MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG
    unset MPICH_TOFU_SHOW_PARAMS
done
exit

