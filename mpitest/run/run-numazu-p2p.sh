#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-P2P" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#
#PJM -L "node=2:noncont"
#	PJM -L "node=4:noncont"
#	PJM -L "node=8:noncont"
#	PJM --mpi "max-proc-per-node=2"
#	PJM --mpi "max-proc-per-node=4"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:01:40"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-mck2_and_spack2,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack1,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#
export LD_LIBRARY_PATH=${HOME}/mpich-tofu/lib:$LD_LIBRARY_PATH
export MPIR_CVAR_OFI_USE_PROVIDER=tofu
export MPICH_CH4_OFI_ENABLE_SCALABLE_ENDPOINTS=1
export TOFU_NAMED_AV=1
## CONF_TOFU_INJECTSIZE=1856 (MSG_EAGER_SIZE = 1878)

#export UTF_MSGMODE=1	# Rendezous
export UTF_MSGMODE=0	# Eager
#export UTF_TRANSMODE=0	# Chained
export UTF_TRANSMODE=1	# Aggressive
export MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG=1
export MPIR_CVAR_CH4_OFI_ENABLE_MR_VIRT_ADDRESS=1	# MPICH 3.4.x
export MPIR_CVAR_CH4_OFI_ENABLE_RMA=1

#export UTF_DEBUG=0x3c	# PROTOCOL EAGER RENDEZOUS RMA
#export UTF_DEBUG=0x43c	# INIFIN PROTOCOL EAGER RENDEZOUS RMA

echo "TOFU_NAMED_AV = " $TOFU_NAMED_AV
echo "UTF_MSGMODE   = " $UTF_MSGMODE "(0: Eager, 1: Rendezous)"
echo "UTF_TRANSMODE = " $UTF_TRANSMODE "(0: Chained, 1: Aggressive)"
echo "UTF_DEBUG     = " $UTF_DEBUG
echo

#
#
#

####################################################################################################
#
#  TAGGED EAGER
export UTF_MSGMODE=0	# Eager
export MPIR_CVAR_CH4_OFI_ENABLE_TAGGED=1	#
#1,000,000,000 1GiB
echo "UTF_MSGMODE   = " $UTF_MSGMODE "(0: Eager, 1: Rendezvous)"
echo "MPIR_CVAR_CH4_OFI_ENABLE_TAGGED = " $MPIR_CVAR_CH4_OFI_ENABLE_TAGGED
echo "mpiexec ../bin/sendrecv -l 1000000000 -i 10"
time mpiexec ../bin/sendrecv -l 1000000000 -i 10	2>&1 # 26.95 sec 
echo

#
#  NO_TAGGED EAGER
export UTF_MSGMODE=0	# Eager
export MPIR_CVAR_CH4_OFI_ENABLE_TAGGED=0	#
echo "UTF_MSGMODE   = " $UTF_MSGMODE "(0: Eager, 1: Rendezvous)"
echo "MPIR_CVAR_CH4_OFI_ENABLE_TAGGED = " $MPIR_CVAR_CH4_OFI_ENABLE_TAGGED
echo "mpiexec ../bin/sendrecv -l 1000000000 -i 10"
time mpiexec ../bin/sendrecv -l 1000000000 -i 10	2>&1 # 22.7 sec 
echo

exit
################################################################################
#
#  TAGGED RENDEZVOUS
export UTF_MSGMODE=1	# Rendezvous
export MPIR_CVAR_CH4_OFI_ENABLE_TAGGED=1	#
#1,000,000,000 1GiB
echo "UTF_MSGMODE   = " $UTF_MSGMODE "(0: Eager, 1: Rendezvous)"
echo "MPIR_CVAR_CH4_OFI_ENABLE_TAGGED = " $MPIR_CVAR_CH4_OFI_ENABLE_TAGGED
echo "mpiexec ../bin/sendrecv -l 1000000000 -i 10"
time mpiexec ../bin/sendrecv -l 1000000000 -i 10	2>&1 #
echo

#
#  NO_TAGGED RENDEZVOUS
export UTF_MSGMODE=1	# Rendezvous
export MPIR_CVAR_CH4_OFI_ENABLE_TAGGED=0	#
echo "UTF_MSGMODE   = " $UTF_MSGMODE "(0: Eager, 1: Rendezvous)"
echo "MPIR_CVAR_CH4_OFI_ENABLE_TAGGED = " $MPIR_CVAR_CH4_OFI_ENABLE_TAGGED
echo "mpiexec ../bin/sendrecv -l 1000000000 -i 10"
time mpiexec ../bin/sendrecv -l 1000000000 -i 10	2>&1 #
echo
####################################################################################################
exit
#
#mpiexec ../bin/sendrecv -l 100 -i 100
#mpiexec ../bin/sendrecv -l 1000 -i 100 -d 8
#mpiexec ../bin/sendrecv -l 10 -i 1000
#mpiexec ../bin/sendrecv -l 10000 -v -d 1 -i 1
echo 
echo
exit

##export TOFULOG_DIR=./results
#export FI_LOG_LEVEL=Debug
#export FI_LOG_PROV=tofu
#	-x FI_LOG_PROV=tofu \
#	-x MPICH_DBG=FILE \
#	-x MPICH_DBG_CLASS=COLL \
#	-x MPICH_DBG_LEVEL=TYPICAL \
#
#	-x PMIX_DEBUG=1 \
#	-x FI_LOG_LEVEL=Debug \
#
