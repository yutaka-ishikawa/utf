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
#PJM -L "elapse=00:00:20"
#PJM -L "rscunit=rscunit_ft01,rscgrp=eap-small,jobenv=linux2"
#PJM -L proc-core=unlimited
#------- Program execution -------#

MPIOPT="-of results/vbg/%n.%j.out -oferr results/vbg/%n.%j.err"

#export UTF_BG_DBG=1
#export UTF_BG_CONFIRM=1

echo "Barrier"
mpich_exec $MPIOPT env LD_PRELOAD=../../build/libmpi_vbg.so ../src/test_collective_vbg -s 0x1

echo;echo
echo "Bcast"
mpich_exec $MPIOPT env LD_PRELOAD=../../build/libmpi_vbg.so ../src/test_collective_vbg -s 0x2

echo;echo
echo "Reduce"
mpich_exec $MPIOPT env LD_PRELOAD=../../build/libmpi_vbg.so ../src/test_collective_vbg -s 0x4

echo;echo
echo "AllReduce"
mpich_exec $MPIOPT env LD_PRELOAD=../../build/libmpi_vbg.so ../src/test_collective_vbg -s 0x8

exit
