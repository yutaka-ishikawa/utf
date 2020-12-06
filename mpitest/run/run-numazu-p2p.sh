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
#PJM -L "elapse=00:03:40"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-mck2_and_spack2,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack1,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#

. ./mpich.env
MPIOPT="-of results/%n.%j.out -oferr results/%n.%j.err"

####################################################################################################
#
#  TAGGED EAGER
export UTF_MSGMODE=0	# Eager
export MPIR_CVAR_CH4_OFI_ENABLE_TAGGED=1	#
#
echo
echo "UTF_MSGMODE   = " $UTF_MSGMODE "(0: Eager, 1: Rendezvous)"
echo "MPIR_CVAR_CH4_OFI_ENABLE_TAGGED = " $MPIR_CVAR_CH4_OFI_ENABLE_TAGGED
#1,000,000,000 1GiB
echo "mpiexec ../bin/sendrecv -l 1000000000 -i 10"
time mpiexec $MPIOPT ../bin/sendrecv -l 1000000000 -i 10	2>&1 # 26.95 sec 
echo

#
#  NO_TAGGED EAGER
export UTF_MSGMODE=0	# Eager
export MPIR_CVAR_CH4_OFI_ENABLE_TAGGED=0	#
echo "UTF_MSGMODE   = " $UTF_MSGMODE "(0: Eager, 1: Rendezvous)"
echo "MPIR_CVAR_CH4_OFI_ENABLE_TAGGED = " $MPIR_CVAR_CH4_OFI_ENABLE_TAGGED
echo "mpiexec ../bin/sendrecv -l 1000000000 -i 10"
time mpiexec $MPIOPT ../bin/sendrecv -l 1000000000 -i 10	2>&1 # 22.7 sec 
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
