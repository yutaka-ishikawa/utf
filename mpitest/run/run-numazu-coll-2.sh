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

#
#   coll -s  0x1: Barrier, 0x2: Reduce, 0x4: Allreduce, 0x8: Gather, 0x10: Alltoall, 0x20: Scatter
#
export MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG=1
export MPICH_TOFU_SHOW_PARAMS=1
#export UTF_INFO=0x1
#export UTF_DEBUG=0xffffff
#export FI_LOG_PROV=tofu
#export FI_LOG_LEVEL=Debug

NP=2
ITER=1
VRYFY="-V 1"
LEN=8
mpich_exec -n $NP $MPIOPT ../src/colld -l $LEN -i $ITER -s 0x10	$VRYFY # Alltoall

#mpiexec ../bin/coll -v -l 16777216 -i 10	# all-to-all max 16MiB for 32 procs
#						# 23 sec for 10 times
##mpiexec ../bin/coll -l 8388608		# 8MiB 6 sec
echo 
echo
exit
