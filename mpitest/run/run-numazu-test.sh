#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-TEST" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#
#PJM -L "node=4"
#PJM --mpi "max-proc-per-node=4"
#PJM -L "elapse=00:00:10"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#

MPIOPT="-of results/%n.%j.out -oferr results/%n.%j.err"
RED_LEN=128
GS_LEN=128
AL_LEN=128
NP=16
ITER=100
VRYFY=2

export MPICH_TOFU_SHOW_PARAMS=1
echo "checking Barrier"
time mpich_exec -n $NP $MPIOPT ../bin/colld -l $RED_LEN -s 0x1 -i $ITER -V $VRYFY #

unset MPICH_TOFU_SHOW_PARAMS

echo; echo
echo "checking Reduce"
time mpich_exec -n $NP $MPIOPT ../bin/colld -l $RED_LEN -s 0x2 -i $ITER -V $VRYFY #
echo; echo
echo "checking Allreduce"
time mpich_exec -n $NP $MPIOPT ../bin/colld -l $RED_LEN -s 0x4 -i $ITER -V $VRYFY #
echo; echo
echo "checking Gather"
time mpich_exec -n $NP $MPIOPT ../bin/colld -l $GS_LEN  -s 0x8 -i $ITER -V $VRYFY #
echo; echo
echo "checking Scatter"
time mpich_exec -n $NP $MPIOPT ../bin/colld -l $GS_LEN  -s 0x20 -i $ITER -V $VRYFY #
echo; echo
echo "checking Alltoall"
time mpich_exec -n $NP $MPIOPT ../bin/colld -l $AL_LEN  -s 0x10 -i $ITER -V $VRYFY #

exit

