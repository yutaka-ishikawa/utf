#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-COLL-VBG-1RACK" # jobname
#PJM -S		# output statistics
#PJM --spath "results/vbg/%n.%j.stat"
#PJM -o "results/vbg/%n.%j.out"
#PJM -e "results/vbg/%n.%j.err"
#
#PJM -L "node=384"
#	PJM -L "node=8:noncont"
#PJM --mpi "max-proc-per-node=1"
#	PJM --mpi "max-proc-per-node=2"
#	PJM --mpi "max-proc-per-node=4"
#	PJM --mpi "max-proc-per-node=4"
#PJM -L "elapse=00:00:20"
#PJM -L "rscunit=rscunit_ft01,rscgrp=eap-llio,jobenv=linux2"
#	PJM -L "rscunit=rscunit_ft01,rscgrp=eap-small,jobenv=linux2"
#PJM -L proc-core=unlimited
#------- Program execution -------#

MPIOPT="-of results/vbg/%n.%j.out -oferr results/vbg/%n.%j.err"
VBGLOAD="env LD_PRELOAD=../../build/libmpi_vbg.so"

echo "UTF_BG_CONFIRM" $UTF_BG_CONFIRM
echo "UTF_BG_DBG" $UTF_BG_DB
echo "UTF_BG_DISABLE" $UTF_BG_DISABLE
echo "UTF_BG_INITWAIT" $UTF_BG_INITWAIT
echo "UTF_BG_BARRIER" $UTF_BG_BARRIER
echo "UTF_BG_UTFPROGRESS" $UTF_BG_UTFPROGRESS

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
