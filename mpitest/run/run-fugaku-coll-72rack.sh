#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-COLL72" # jobname
#PJM -S		# output statistics
#
#PJM --spath "results/coll-72rack/%n.%j.stat"
#PJM -o "results/coll-72rack/%n.%j.out"
#PJM -e "results/coll-72rack/%n.%j.err"
#
#PJM -L "node=27648"
#	PJM -L "node=1152"
#	PJM --mpi "max-proc-per-node=1"
#PJM --mpi "max-proc-per-node=4"
#	PJM --mpi "max-proc-per-node=32"
#	PJM --mpi "max-proc-per-node=48"
#PJM -L "elapse=00:30:00"
#PJM -L "rscunit=rscunit_ft01,rscgrp=eap-large,jobenv=linux2"
#PJM -L proc-core=unlimited
#------- Program execution -------#

MPIOPT="-of results/coll-72rack/%n.%j.out -oferr results/coll-72rack/%n.%j.err"

#
#   coll -s  0x1: Barrier, 0x2: Reduce, 0x4: Allreduce, 0x8: Gather, 0x10: Alltoall, 0x20: Scatter
#
#
NP=110592
export MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG=1

for RED_LEN in 1024 262144 524288
do
    echo; echo;
    echo "checking Reduce"
    time mpich_exec -n $NP $MPIOPT ../bin/colld -l $RED_LEN -s 0x2 -V 1	#
    unset MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG
    echo; echo;
    echo "checking Allreduce"
    time mpich_exec -n $NP $MPIOPT ../bin/colld -l $RED_LEN -s 0x4 -V 1	#
done
exit

for RED_LEN in 1 1024 2048 4096
do
    echo; echo;
    echo "checking Reduce"
    time mpich_exec -n $NP $MPIOPT ../bin/colld -l $RED_LEN -s 0x2	#
    unset MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG
    echo; echo;
    echo "checking Allreduce"
    time mpich_exec -n $NP $MPIOPT ../bin/colld -l $RED_LEN -s 0x4	#
done
