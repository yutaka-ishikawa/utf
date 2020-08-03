#!/bin/sh
#------ pjsub option --------#
#PJM -N "TEST_PROFILE" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#
#PJM -L "node=1"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:00:30"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#

[ -d ./profdata ] || mkdir ./profdata
PROF_DIR=./profdata/rep
export FLIB_FASTOMP=TRUE
for i in `seq 1 17`
do
    echo "***** pa$i ****"  
    echo fapp -C -d ${PROF_DIR}$i -Icpupa,nompi -Hevent=pa$i ./test_profile
    fapp -C -d ${PROF_DIR}$i -Icpupa,nompi -Hevent=pa$i ./test_profile
done

#printenv
