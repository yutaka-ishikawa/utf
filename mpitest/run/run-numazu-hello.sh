#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-HELLO" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#
#PJM -L "node=4"
#PJM --mpi "max-proc-per-node=4"
#	PJM --mpi "max-proc-per-node=1"
#	PJM --mpi "max-proc-per-node=2"
#PJM -L "elapse=00:00:10"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack1,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#

mpiexec -n 2 ../bin/hello -v
echo
