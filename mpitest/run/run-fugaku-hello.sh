#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-HELLO" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#
#	PJM -L "node=192"		# 4x3x16
#PJM -L "node=120"		#
#	PJM -L "node=64" #OK
#	PJM -L "node=32" #OK 3sec (4190) 1025704
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
## CONF_TOFU_INJECTSIZE=1856 (MSG_EAGER_SIZE = 1878)


#export UTF_DEBUG=0xfffc

export UTF_MSGMODE=0	# Eager 
#export UTF_MSGMODE=1	# Rendezous
#export UTF_TRANSMODE=0	# Chained
export UTF_TRANSMODE=1	# Aggressive
export TOFU_NAMED_AV=1
export TOFULOG_DIR=./results

echo "LD_LIBRARY_PATH=" $LD_LIBRARY_PATH
echo "TOFU_NAMED_AV  = " $TOFU_NAMED_AV
echo "UTF_MSGMODE    = " $UTF_MSGMODE "(0: Eager, 1: Rendezous)"
echo "UTF_TRANSMODE  = " $UTF_TRANSMODE "(0: Chained, 1: Aggressive)"

#export PMIX_DEBUG=1
#export UTF_DEBUG=0xffff		# debug all
#export FI_LOG_LEVEL=Debug
#export FI_LOG_PROV=tofu

mpiexec ../bin/hello
echo
echo

#	-x FI_LOG_PROV=tofu \
#	-x MPICH_DBG=FILE \
#	-x MPICH_DBG_CLASS=COLL \
#	-x MPICH_DBG_LEVEL=TYPICAL \
#
#	-x PMIX_DEBUG=1 \
#	-x FI_LOG_LEVEL=Debug \
#
