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
#PJM -L "elapse=00:00:30"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#

PROF_DIR=./profdata/rep
export FLIB_FASTOMP=TRUE
for i in `seq 1 17`
do
    echo "***** pa$i ****"  
    echo fapp -C -d ${PROF_DIR}$i -Icpupa,nompi -Hevent=pa$i mpiexec -np 2 ./test_mpi
    fapp -C -d ${PROF_DIR}$i -Icpupa,nompi -Hevent=pa$i mpiexec -np 2 ./test_mpi
done
