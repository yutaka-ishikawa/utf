#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-VBG" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#
#	PJM -L "node=2:noncont"
#PJM -L "node=2:noncont"
#	PJM -L "node=8:noncont"
#	PJM --mpi "max-proc-per-node=2"
#	PJM --mpi "max-proc-per-node=4"
#PJM --mpi "max-proc-per-node=4"
#PJM -L "elapse=00:00:40"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-mck2_and_spack2,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack1,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#
MPIOPT="-of results/%n.%j.out -oferr results/%n.%j.err"

mpich_exec $MPIOPT ../src/test_vbg

exit
