#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-COLL-VBG" # jobname
#PJM -S		# output statistics
#PJM --spath "results/vbg/%n.%j.stat"
#PJM -o "results/vbg/%n.%j.out"
#PJM -e "results/vbg/%n.%j.err"
#
#PJM -L "node=4"
#	PJM -L "node=8:noncont"
#	PJM --mpi "max-proc-per-node=2"
#	PJM --mpi "max-proc-per-node=4"
#PJM --mpi "max-proc-per-node=4"
#PJM -L "elapse=00:01:00"
#PJM -L "rscunit=rscunit_ft01,rscgrp=small"
#PJM -L proc-core=unlimited
#------- Program execution -------#

MPIOPT="-of results/vbg/%n.%j.out -oferr results/vbg/%n.%j.err"
export PATH=~/mpich-tofu/bin:$PATH
export MPICH_HOME=~/mpich-tofu/
which mpich_exec
export UTF_BG_LOAD=1

export MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG=1
export MPICH_TOFU_SHOW_PARAMS=1

export UTF_BG_CONFIRM=1
echo "UTF_BG_LOAD= " $UTF_BG_LOAD
echo "UTF_BG_CONFIRM= " $UTF_BG_CONFIRM

echo "Barrier"
mpich_exec $MPIOPT $VBGLOAD ../src/test_collective_vbg -s 0x1

echo;echo
echo "Bcast"
mpich_exec $MPIOPT $VBGLOAD ../src/test_collective_vbg -s 0x2

echo;echo
echo "Reduce"
mpich_exec $MPIOPT $VBGLOAD ../src/test_collective_vbg -s 0x4

echo;echo
echo "AllReduce"
mpich_exec $MPIOPT $VBGLOAD ../src/test_collective_vbg -s 0x8

exit

