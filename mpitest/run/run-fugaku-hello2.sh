#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-HELLO2" # jobname
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
#PJM --mpi "max-proc-per-node=4"
#	PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:01:00"
#PJM -L "rscunit=rscunit_ft01,rscgrp=dvsys-mck6-4,jobenv=linux2"
#PJM -L proc-core=unlimited
#------- Program execution -------#

#module switch lang/tcsds-1.2.27b

MPIOPT="-of results/%n.%j.out -oferr results/%n.%j.err"
#export MPICH_HOME=$HOME/mpich-tofu-fc
#export MPICH_HOME=$HOME/mpich-tofu-fc
export MPICH_HOME=$HOME/mpich-tofu-fc2

echo "******************"
echo $MPICH_HOME

#NP=128 # OK
#NP=1024
#NP=16
NP=2
#NP=64 # OK
$MPICH_HOME/bin/mpich_exec -n $NP $MPIOPT ../bin/hello -v
echo
printenv
exit
