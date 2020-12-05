#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-COLL" # jobname
#PJM -S		# output statistics
#
#PJM --spath "results/coll-9rack/%n.%j.stat"
#PJM -o "results/coll-9rack/%n.%j.out"
#PJM -e "results/coll-9rack/%n.%j.err"
#
#PJM -L "node=3456"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:10:30"
#PJM -L "rscunit=rscunit_ft01,rscgrp=eap-llio,jobenv=linux2"
#PJM -L proc-core=unlimited
#------- Program execution -------#

MPIOPT="-of results/coll-9rack/%n.%j.out -oferr results/coll-9rack/%n.%j.err"

export LD_LIBRARY_PATH=${HOME}/mpich-tofu/lib:$LD_LIBRARY_PATH
export MPIR_CVAR_OFI_USE_PROVIDER=tofu
export MPICH_CH4_OFI_ENABLE_SCALABLE_ENDPOINTS=1
export UTF_MSGMODE=0	# Eager 
##export UTF_MSGMODE=1	# Rendezous
#export UTF_TRANSMODE=0	# Chained
export UTF_TRANSMODE=1	# Aggressive
export TOFU_NAMED_AV=1

export MPIR_CVAR_ALLTOALL_SHORT_MSG_SIZE=2147483647 # 32768 in default (integer value)  
export MPIR_CVAR_CH4_OFI_ENABLE_ATOMICS=-1
export MPIR_CVAR_CH4_OFI_ENABLE_MR_VIRT_ADDRESS=1	# MPICH 3.4.x
export MPIR_CVAR_CH4_OFI_ENABLE_RMA=1
export MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG=1
export MPIR_CVAR_CH4_OFI_ENABLE_TAGGED=0	#

echo "LD_LIBRARY_PATH=" $LD_LIBRARY_PATH
echo "TOFU_NAMED_AV  = " $TOFU_NAMED_AV
echo "UTF_DEBUG      = " $UTF_DEBUG
echo "UTF_MSGMODE    = " $UTF_MSGMODE "(0: Eager, 1: Rendezous)"
echo "UTF_TRANSMODE  = " $UTF_TRANSMODE "(0: Chained, 1: Aggressive)"
echo "MPIR_CVAR_ALLTOALL_SHORT_MSG_SIZE = " $MPIR_CVAR_ALLTOALL_SHORT_MSG_SIZE
###echo "MPIR_CVAR_ALLTOALL_INTER_ALGORITHM   = " $MPIR_CVAR_ALLTOALL_INTER_ALGORITHM
###echo "MPIR_PARAM_ALLTOALLV_INTRA_ALGORITHM = " $MPIR_PARAM_ALLTOALLV_INTRA_ALGORITHM

#
#   coll -v  0x1: Barrier, 0x2: Reduce, 0x4: Allreduce, 0x8: Gather, 0x10: Alltoall, 0x20: Scatter
#
mpiexec $MPIOPT ../bin/coll -l 512 -v 0xff

#mpiexec ../bin/coll -l 5120 -v -s 


#echo
#echo
##ldd ../bin/coll
#	-x FI_LOG_PROV=tofu \
#	-x MPICH_DBG=FILE \
#	-x MPICH_DBG_CLASS=COLL \
#	-x MPICH_DBG_LEVEL=TYPICAL \
#
#	-x PMIX_DEBUG=1 \
#	-x FI_LOG_LEVEL=Debug \
#
