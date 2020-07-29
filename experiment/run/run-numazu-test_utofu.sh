#!/bin/sh
#------ pjsub option --------#
#PJM -N "UTF-UTOFU" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#
#PJM -L "node=2"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:00:20"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#

INSTDIR=../bin
export LOGDIR=./results

echo "******************************************************"
mpiexec -np 2 $INSTDIR/test_utofu pingpong -i 100
echo
echo "******************************************************"
mpiexec -np 2 $INSTDIR/test_utofu pingping -i 100
echo
echo "EXITING"
