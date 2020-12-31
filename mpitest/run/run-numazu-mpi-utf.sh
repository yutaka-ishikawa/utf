#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-UTF" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#
#PJM -L "node=4"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:00:10"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#
SAVED_LD_LIBRARY_PATH=$LD_LIBRARY_PATH

MPIOPT="-of results/%n.%j.out -oferr results/%n.%j.err"

export MPICH_TOFU_SHOW_PARAMS=1
echo "checking MPICH-Tofu with UTF interface"
mpich_exec $MPIOPT ../bin/test_mpi_utf
exit


echo "checking pingpong"
time mpich_exec -n 2 $MPIOPT ../bin/pingpong -L $MAX_LEN -l $MIN_LEN -i $ITER -V $VRYFY
echo; echo
echo "FJMPI"
time mpiexec -n 2 $MPIOPT ../bin/pingpong-f -L $MAX_LEN -l $MIN_LEN -i $ITER -V $VRYFY

exit
