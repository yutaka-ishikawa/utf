#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-COLL2" # jobname
#PJM -S		# output statistics
#PJM --spath "results/coll-2/%n.%j.stat"
#PJM -o "results/coll-2/%n.%j.out"
#PJM -e "results/coll-2/%n.%j.err"
#
#	PJM -L "node=2:noncont"
#PJM -L "node=1:noncont"
#	PJM -L "node=8:noncont"
#	PJM --mpi "max-proc-per-node=2"
#PJM --mpi "max-proc-per-node=2"
#	PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:00:10"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-mck2_and_spack2,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack1,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#
MPIOPT="-of results/coll-2/%n.%j.out -oferr results/coll-2/%n.%j.err"
export LD_LIBRARY_PATH=${HOME}/mpich-tofu/lib:$LD_LIBRARY_PATH
export MPIR_CVAR_OFI_USE_PROVIDER=tofu
export MPICH_CH4_OFI_ENABLE_SCALABLE_ENDPOINTS=1
export MPIR_CVAR_ALLTOALL_SHORT_MSG_SIZE=2147483647 # 32768 in default (integer value)  
export MPIR_CVAR_CH4_OFI_ENABLE_ATOMICS=-1
export MPIR_CVAR_CH4_OFI_ENABLE_MR_VIRT_ADDRESS=1
export MPIR_CVAR_CH4_OFI_ENABLE_RMA=1
export MPIR_CVAR_CH4_OFI_ENABLE_TAGGED=0
export MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG=1
export UT_MSGMODE=0
export UTF_TRANSMODE=1	# Aggressive
export TOFU_NAMED_AV=1

echo "TOFU_NAMED_AV = " $TOFU_NAMED_AV
echo "UTF_MSGMODE   = " $UTF_MSGMODE "(0: Eager, 1: Rendezous)"
echo "UTF_TRANSMODE = " $UTF_TRANSMODE "(0: Chained, 1: Aggressive)"
echo "MPIR_CVAR_CH4_OFI_ENABLE_TAGGED = " $MPIR_CVAR_CH4_OFI_ENABLE_TAGGED

# testing MPI_Reduce
mpiexec $MPIOPT ../bin/coll -l 512 -s 0xff
#mpiexec $MPIOPT ../bin/coll -l 512 -s 0x1

#mpiexec $MPIOPT ../bin/coll -s 2 -l 1 -i 1
#mpiexec ../bin/coll -l 4
#mpiexec ../bin/coll -l 512
#mpiexec ../bin/coll -v -l 16777216 -i 10	# all-to-all max 16MiB for 32 procs
#						# 23 sec for 10 times
##mpiexec ../bin/coll -l 8388608		# 8MiB 6 sec
echo 
echo
exit

#####################################################################################
## CONF_TOFU_INJECTSIZE=1856 (MSG_EAGER_SIZE = 1878)
#export UTF_DEBUG=12
#export UTF_DEBUG=0xfffc
##export UTF_MSGMODE=1	# Rendezous
##export UTF_TRANSMODE=0	# Chained
#export PMIX_DEBUG=1
#export UTF_DEBUG=0xffff		# debug all
#export FI_LOG_LEVEL=Debug
#export FI_LOG_PROV=tofu

############################################################################
export UTF_MSGMODE=0	# Eager
echo "TOFU_NAMED_AV = " $TOFU_NAMED_AV
echo "UTF_MSGMODE   = " $UTF_MSGMODE "(0: Eager, 1: Rendezous)"
echo "UTF_TRANSMODE = " $UTF_TRANSMODE "(0: Chained, 1: Aggressive)"
#mpiexec ../bin/coll -v -l 5120
#mpiexec ../bin/coll -v -l 524288 -i 100	# 512KiB 7sec for 100 times 4 procs
#mpiexec ../bin/coll -v -l 1048576 -i 100	# 1MiB
#mpiexec ../bin/coll -v -l 8388608 -i 50	# 8MiB 41sec for 50 times using 4 procs
						# 8MiB 96sec for 50 times using 8 procs
#mpiexec ../bin/coll -v -l 8388608 -i 5		# 8MiB 52sec for 5 times using 32 procs
mpiexec ../bin/coll -v -l 16777216 -i 2		# all-to-all max 16MiB (1 GiB working memory) for 32 procs
						# 47 sec for 2 times

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
