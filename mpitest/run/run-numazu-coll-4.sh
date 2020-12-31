#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-COLL4" # jobname
#PJM -S		# output statistics
#PJM --spath "results/coll-4/%n.%j.stat"
#PJM -o "results/coll-4/%n.%j.out"
#PJM -e "results/coll-4/%n.%j.err"
#
#PJM -L "node=4:noncont"
#PJM --mpi "max-proc-per-node=4"
#	PJM -L "node=1:noncont"
#	PJM -L "node=8:noncont"
#	PJM --mpi "max-proc-per-node=2"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:00:10"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-mck2_and_spack2,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack1,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#
MPIOPT="-of results/coll-4/%n.%j.out -oferr results/coll-4/%n.%j.err"

#
#   coll -s  0x1: Barrier, 0x2: Reduce, 0x4: Allreduce, 0x8: Gather, 0x10: Alltoall, 0x20: Scatter
#
export MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG=1
export MPICH_TOFU_SHOW_PARAMS=1
export UTF_INFO=0x1
#export UTF_DEBUG=0xffffff
#export FI_LOG_PROV=tofu
#export FI_LOG_LEVEL=Debug

NP=4
ITER=1
VRYFY="-V 1"
#for LEN in 8 128 256 512 1024 2048 #size in double
LEN=512
#do
    mpich_exec -n $NP $MPIOPT ../src/colld -l $LEN -i $ITER -s 0x2 $VRYFY # Reduce
#    mpiexec -n $NP $MPIOPT ../src/colld-f -l $LEN -i $ITER -s 0x2 $VRYFY # Reduce
#done
exit

#mpich_exec -n $NP $MPIOPT ../src/colld -l $LEN -i $ITER -s 0x10	$VRYFY # Alltoall
