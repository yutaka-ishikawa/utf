#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-HELLO" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#
#	PJM -L "node=192"		# 4x3x16
#	PJM -L "node=120"		#
#	PJM -L "node=64" #OK
#	PJM -L "node=32" #OK 3sec (4190) 1025704
#	PJM -L "node=32"
#PJM -L "node=2"
#	PJM -L "node=256"
#	PJM --mpi "max-proc-per-node=2"
#	PJM --mpi "max-proc-per-node=4"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:01:00"
#PJM -L "rscunit=rscunit_ft01,rscgrp=small"
#PJM -L proc-core=unlimited
#------- Program execution -------#

MPIOPT="-of results/%n.%j.out -oferr results/%n.%j.err"
which mpich_exec
export MPICH_HOME=`which mpich_exec | sed -e "s/bin\/mpich_exec//"`

export MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG=1
export MPICH_TOFU_SHOW_PARAMS=1

export UTF_TRANSMODE=0		# Chained mode
export MPIR_CVAR_CH4_OFI_ENABLE_TAGGED=1

echo "******************"
echo $MPICH_HOME

NP=2

$MPICH_HOME/bin/mpich_exec -n $NP $MPIOPT ../bin/hello -v
exit
