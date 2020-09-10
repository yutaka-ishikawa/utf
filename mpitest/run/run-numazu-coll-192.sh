#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-COLL192" # jobname
#PJM -S		# output statistics
#PJM --spath "results/coll-192/%n.%j.stat"
#PJM -o "results/coll-192/%n.%j.out"
#PJM -e "results/coll-192/%n.%j.err"
#
#	PJM -L "node=2:noncont"
#PJM -L "node=4:noncont"
#	PJM -L "node=8:noncont"
#	PJM --mpi "max-proc-per-node=2"
#PJM --mpi "max-proc-per-node=48"
#	PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:00:20"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-mck2_and_spack2,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack1,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#

export LD_LIBRARY_PATH=${HOME}/mpich-tofu/lib:$LD_LIBRARY_PATH
export MPIR_CVAR_OFI_USE_PROVIDER=tofu
export MPICH_CH4_OFI_ENABLE_SCALABLE_ENDPOINTS=1
## CONF_TOFU_INJECTSIZE=1856 (MSG_EAGER_SIZE = 1878)

#export UTF_DEBUG=12
#export UTF_DEBUG=0xfffc
#export UTF_DEBUG=0x14

export UTF_MSGMODE=1	# Rendezous
#export UTF_MSGMODE=0	# Eager
#export UTF_TRANSMODE=0	# Chained
export UTF_TRANSMODE=1	# Aggressive
export TOFU_NAMED_AV=1
export TOFULOG_DIR=./results/coll-192

export MPIR_CVAR_ALLTOALL_SHORT_MSG_SIZE=2147483647 # 32768 in default (integer value)

echo "LD_LIBRARY_PATH=" $LD_LIBRARY_PATH
echo "TOFU_NAMED_AV = " $TOFU_NAMED_AV
echo "UTF_MSGMODE   = " $UTF_MSGMODE "(0: Eager, 1: Rendezous)"
echo "UTF_TRANSMODE = " $UTF_TRANSMODE "(0: Chained, 1: Aggressive)"
echo "MPIR_CVAR_ALLTOALL_SHORT_MSG_SIZE = " $MPIR_CVAR_ALLTOALL_SHORT_MSG_SIZE

#mpiexec ../bin/coll -l 512	# 512*192 = 393216

#echo "Coll ALL"
#mpiexec ../bin/coll -l 512 -s 0x2f -V # 512*192 = 393216

#echo "Gather test"
#mpiexec ../bin/coll -l 512 -s 0x8 -V # 512*192 = 393216

echo "Scatter test"
mpiexec ../bin/coll -l 512 -s 0x20 -V # 512*192 = 393216

echo
echo
ldd ../bin/coll
