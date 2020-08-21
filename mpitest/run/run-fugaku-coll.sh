#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-COLL" # jobname
#PJM -S		# output statistics
#
#	PJM --spath "results/coll-2/%n.%j.stat"
#	PJM -o "results/coll-2/%n.%j.out"
#	PJM -e "results/coll-2/%n.%j.err"
#
#	PJM --spath "results/coll-384-l512/%n.%j.stat"
#	PJM -o "results/coll-384-l512/%n.%j.out"
#	PJM -e "results/coll-384-l512/%n.%j.err"
#
#	PJM --spath "results/coll-256-l1/%n.%j.stat"
#	PJM -o "results/coll-256-l1/%n.%j.out"
#	PJM -e "results/coll-256-l1/%n.%j.err"
#
#PJM --spath "results/coll-256-l512/%n.%j.stat"
#PJM -o "results/coll-256-l512/%n.%j.out"
#PJM -e "results/coll-256-l512/%n.%j.err"
#
#	PJM --spath "results/coll-192-l512/%n.%j.stat"
#	PJM -o "results/coll-192-l512/%n.%j.out"
#	PJM -e "results/coll-192-l512/%n.%j.err"
#
#	PJM --spath "results/coll-64-l512/%n.%j.stat"
#	PJM -o "results/coll-64-l512/%n.%j.out"
#	PJM -e "results/coll-64-l512/%n.%j.err"
#	PJM --spath "results/coll-32-l512/%n.%j.stat"
#	PJM -o "results/coll-32-l512/%n.%j.out"
#	PJM -e "results/coll-32-l512/%n.%j.err"
#
#	PJM -L "node=16384" #OK	#
#	PJM -L "node=384" #OK	9sec	# 1 rack
#PJM -L "node=256" #OK	# 4x3x16
#	PJM -L "node=192" #OK	# 4x3x16
#	PJM -L "node=64"		#
#	PJM -L "node=32"
#	PJM -L "node=16" #OK 3sec (3250) 1025588
#	PJM -L "node=8" #OK 3sec (960ms) 1025576
#	PJM -L "node=2" #OK 3sec (960ms) 1025576
#	PJM --mpi "max-proc-per-node=2"
#	PJM --mpi "max-proc-per-node=4"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:00:30"
#PJM -L "rscunit=rscunit_ft01,rscgrp=eap-small,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft01,rscgrp=dvsys-mck1,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#

export LD_LIBRARY_PATH=${HOME}/mpich-tofu/lib:$LD_LIBRARY_PATH
export MPIR_CVAR_OFI_USE_PROVIDER=tofu
export MPICH_CH4_OFI_ENABLE_SCALABLE_ENDPOINTS=1
##export MPIR_CVAR_ALLTOALL_INTER_ALGORITHM=pairwise_exchange
##export MPIR_PARAM_ALLTOALLV_INTRA_ALGORITHM=pairwise_sendrecv_replace
export MPIR_CVAR_ALLTOALL_SHORT_MSG_SIZE=204800
#export MPIR_CVAR_ALLTOALL_MEDIUM_MSG_SIZE=128	# shorter than MPIR_CVAR_ALLTOALL_SHORT_MSG_SIZE
## CONF_TOFU_INJECTSIZE=1856 (MSG_EAGER_SIZE = 1878)


#export UTF_DEBUG=0xfffc
#export UTF_DEBUG=0xffff		# debug all
#export FI_LOG_LEVEL=Debug
#export FI_LOG_PROV=tofu

export UTF_MSGMODE=0	# Eager 
#export UTF_MSGMODE=1	# Rendezous
#export UTF_TRANSMODE=0	# Chained
export UTF_TRANSMODE=1	# Aggressive
export TOFU_NAMED_AV=1

#export TOFULOG_DIR=./results/coll-2/
#export TOFULOG_DIR=./results/coll-384-l1/
#export TOFULOG_DIR=./results/coll-256-l1/

#export TOFULOG_DIR=./results/coll-256-l512/

#export TOFULOG_DIR=./results/coll-192-l512/

#export TOFULOG_DIR=./results/coll-64-l512/
#export TOFULOG_DIR=./results/coll-32-l512/

echo "LD_LIBRARY_PATH=" $LD_LIBRARY_PATH
echo "TOFU_NAMED_AV  = " $TOFU_NAMED_AV
echo "UTF_DEBUG      = " $UTF_DEBUG
echo "UTF_MSGMODE    = " $UTF_MSGMODE "(0: Eager, 1: Rendezous)"
echo "UTF_TRANSMODE  = " $UTF_TRANSMODE "(0: Chained, 1: Aggressive)"
echo "MPIR_CVAR_ALLTOALL_SHORT_MSG_SIZE = " $MPIR_CVAR_ALLTOALL_SHORT_MSG_SIZE
###echo "MPIR_CVAR_ALLTOALL_INTER_ALGORITHM   = " $MPIR_CVAR_ALLTOALL_INTER_ALGORITHM
###echo "MPIR_PARAM_ALLTOALLV_INTRA_ALGORITHM = " $MPIR_PARAM_ALLTOALLV_INTRA_ALGORITHM

#mpiexec ../bin/coll -l 1 -v
mpiexec ../bin/coll -l 512 -v
#mpiexec ../bin/coll -l 5120 -v
echo
echo
ldd ../bin/coll

#	-x FI_LOG_PROV=tofu \
#	-x MPICH_DBG=FILE \
#	-x MPICH_DBG_CLASS=COLL \
#	-x MPICH_DBG_LEVEL=TYPICAL \
#
#	-x PMIX_DEBUG=1 \
#	-x FI_LOG_LEVEL=Debug \
#
