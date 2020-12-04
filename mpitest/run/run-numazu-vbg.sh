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

export LD_LIBRARY_PATH=${HOME}/mpich-tofu/lib:$LD_LIBRARY_PATH
export MPIR_CVAR_OFI_USE_PROVIDER=tofu
export MPICH_CH4_OFI_ENABLE_SCALABLE_ENDPOINTS=1

#export UTF_DEBUG=12
#export UTF_DEBUG=0xfffc

export UTF_MSGMODE=1	# Rendezous
#export UTF_TRANSMODE=0	# Chained
export UTF_TRANSMODE=1	# Aggressive
export TOFU_NAMED_AV=1
###export TOFULOG_DIR=./results

echo "TOFU_NAMED_AV = " $TOFU_NAMED_AV
echo "UTF_MSGMODE   = " $UTF_MSGMODE "(0: Eager, 1: Rendezous)"
echo "UTF_TRANSMODE = " $UTF_TRANSMODE "(0: Chained, 1: Aggressive)"

mpiexec ../src/test_vbg

exit
