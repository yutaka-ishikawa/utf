#!/bin/sh
#------ pjsub option --------#
#PJM -N "UTF-COMMGROUP" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#
#PJM -L "node=2"
#PJM --mpi "max-proc-per-node=4"
#	PJM --mpi "rank-map-bynode"
#PJM --mpi "rank-map-bychip" #default
#PJM -L "elapse=00:00:30"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#
export LD_LIBRARY_PATH=../../build/:$HOME/mpich-tofu/lib:$LD_LIBRARY_PATH
#

export MPIR_CVAR_OFI_USE_PROVIDER=tofu
export MPICH_CH4_OFI_ENABLE_SCALABLE_ENDPOINTS=1
export UTF_MSGMODE=0	# Eager 
export UTF_TRANSMODE=1	# Aggressive
export TOFU_NAMED_AV=1

echo "LD_LIBRARY_PATH= " $LD_LIBRARY_PATH
echo "TOFU_NAMED_AV = " $TOFU_NAMED_AV
echo "UTF_MSGMODE   = " $UTF_MSGMODE "(0: Eager, 1: Rendezous)"
echo "UTF_TRANSMODE = " $UTF_TRANSMODE "(0: Chained, 1: Aggressive)"

#NP=32
NP=8
#mpiexec -of results/%n.%j.out -np $NP ./test_commgroup
mpiexec -np $NP ./test_commgroup
exit

##export UTF_DEBUG=0x400
#mpiexec -np 3 ./test_mapping
#mpiexec -of results/%n.%j.out -np 4 ./test_mapping
#printenv
