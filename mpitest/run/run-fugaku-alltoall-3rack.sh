#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-ALLTOALL-3rack" # jobname
#PJM -S		# output statistics
#
#PJM --spath "results/coll-3rack/%n.%j.stat"
#PJM -o "results/coll-3rack/%n.%j.out"
#PJM -e "results/coll-3rack/%n.%j.err"
#
#PJM -L "node=1152"
#	PJM -L "node=128"
#	PJM --mpi "max-proc-per-node=1"
#PJM --mpi "max-proc-per-node=4"
#	PJM --mpi "max-proc-per-node=8"
#	PJM --mpi "max-proc-per-node=16"
#	PJM --mpi "max-proc-per-node=32"
#	PJM --mpi "max-proc-per-node=48"
#PJM -L "elapse=00:10:30"
#	PJM -L "elapse=00:3:30"
#PJM -L "rscunit=rscunit_ft01,rscgrp=eap-llio,jobenv=linux2"
#	PJM -L "rscunit=rscunit_ft01,rscgrp=eap-small,jobenv=linux2"
#PJM -L proc-core=unlimited
#------- Program execution -------#

RED_LEN=512  #
#GS_LEN=512   #
GS_LEN=1024   #

#RED_LEN=128
#GS_LEN=128

NP=4608
MPIOPT="-of results/coll-3rack/%n.%j.out -oferr results/coll-3rack/%n.%j.err"
export MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG=1

#
#   coll -s  0x1: Barrier, 0x2: Reduce, 0x4: Allreduce, 0x8: Gather, 0x10: Alltoall, 0x20: Scatter
#

#for RED_LEN in 1 2 3
#for RED_LEN in 4 8 16
#for RED_LEN in 32 64 128
#for RED_LEN in 256 512
for RED_LEN in 4096 8192
do
    echo "checking Alltoall size = " $RED_LEN
    time mpich_exec -n $NP $MPIOPT ../bin/colld -l $RED_LEN -s 0x10	#
    unset MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG
done
exit
