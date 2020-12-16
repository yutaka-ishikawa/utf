#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-COLL16" # jobname
#PJM -S		# output statistics
#PJM --spath "results/coll-16/%n.%j.stat"
#PJM -o "results/coll-16/%n.%j.out"
#PJM -e "results/coll-16/%n.%j.err"
#
#PJM -L "node=4:noncont"
#PJM --mpi "max-proc-per-node=4"
#PJM -L "elapse=00:00:10"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-mck2_and_spack2,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#
MPIOPT="-of results/coll-16/%n.%j.out -oferr results/coll-16/%n.%j.err"

# max RMA 8388608

export MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG=1
export MPIR_CVAR_REDUCE_SHORT_MSG_SIZE=100000
##export UTF_DEBUG=0x24
mpich_exec $MPIOPT -n 2 ../bin/colld -l 8400000 -i 100 -s 0x2 -V 2
#mpich_exec $MPIOPT -n 16 ../bin/colld -l 8196 -i 1 -s 0x2 -V 2

exit

