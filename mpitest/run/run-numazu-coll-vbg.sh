#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-COLL-VBG" # jobname
#PJM -S		# output statistics
#PJM --spath "results/vbg/%n.%j.stat"
#PJM -o "results/vbg/%n.%j.out"
#PJM -e "results/vbg/%n.%j.err"
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

#MPIOPT="-of results/vbg/%n.%j.out -oferr results/vbg/%n.%j.err"
VBGLOAD="env LD_PRELOAD=../../build/libmpi_vbg.so"

export UTF_BG_CONFIRM=1
#export UTF_BG_DBG=1
#export UTF_BG_DISABLE=1
#export UTF_BG_INITWAIT=10000
#export UTF_BG_BARRIER=1
#export UTF_BG_UTFPROGRESS=1

echo "UTF_BG_CONFIRM" $UTF_BG_CONFIRM
echo "UTF_BG_DBG" $UTF_BG_DB
echo "UTF_BG_DISABLE" $UTF_BG_DISABLE
echo "UTF_BG_INITWAIT" $UTF_BG_INITWAIT
echo "UTF_BG_BARRIER" $UTF_BG_BARRIER
echo "UTF_BG_UTFPROGRESS" $UTF_BG_UTFPROGRESS

export MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG=1

mpich_exec $MPIOPT $VBGLOAD ../src/test_collective_vbg

exit
