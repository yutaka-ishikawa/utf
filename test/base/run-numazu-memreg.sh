#!/bin/sh
#------ pjsub option --------#
#PJM -N "UTF-UTOFU" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#
#PJM -L "node=1"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:00:10"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#
export LD_LIBRARY_PATH=../build/:$LD_LIBRARY_PATH
#

mpiexec -np 1 ./test_memreg
exit
