#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-PINGPONG-F" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#
#PJM -L "node=2"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:04:10"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#
SAVED_LD_LIBRARY_PATH=$LD_LIBRARY_PATH

MPIOPT="-of results/%n.%j.out -oferr results/%n.%j.err"
#MAX_LEN=134217728	# 128 MB
MAX_LEN=134217728 # 1 MB
VRYFY=2

function pingpong() {
    echo; echo
    echo "FJMPI ITER=" $ITER
    mpiexec -n 2 $MPIOPT ../bin/pingpong-f -i $ITER -L $MAX_LEN -V $VRYFY #
}

for ITER in 10 20 30 40 100 200 1000
do
    pingpong
done
exit
