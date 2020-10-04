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

export UTF_MSGMODE=1	# Rendezous
#export UTF_TRANSMODE=0	# Chained
export UTF_TRANSMODE=1	# Aggressive
export TOFU_NAMED_AV=1
##export TOFULOG_DIR=./results/coll-48

echo "TOFU_NAMED_AV = " $TOFU_NAMED_AV
echo "UTF_MSGMODE   = " $UTF_MSGMODE "(0: Eager, 1: Rendezous)"
echo "UTF_TRANSMODE = " $UTF_TRANSMODE "(0: Chained, 1: Aggressive)"

#export PMIX_DEBUG=1
#export UTF_DEBUG=0xffff		# debug all
#export FI_LOG_LEVEL=Debug
#export FI_LOG_PROV=tofu

mpiexec ../bin/coll -l 4
#mpiexec ../bin/coll -l 512
#mpiexec ../bin/coll -v -l 16777216 -i 10	# all-to-all max 16MiB for 32 procs
#						# 23 sec for 10 times
##mpiexec ../bin/coll -l 8388608		# 8MiB 6 sec
echo 
echo
exit

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
