#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-SENDONE" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#
#PJM -L "node=2"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:01:00"
#PJM -L "rscunit=rscunit_ft01,rscgrp=small"
#PJM -L proc-core=unlimited
#------- Program execution -------#
SAVED_LD_LIBRARY_PATH=$LD_LIBRARY_PATH

##MPIOPT="-of results/%n.%j.out -oferr results/%n.%j.err"
export PATH=~/mpich-tofu-nv/bin:$PATH
export MPICH_HOME=~/mpich-tofu-nv/
which mpich_exec
export MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG=1
export MPICH_TOFU_SHOW_PARAMS=1
#export UTF_INFO=0x1
#export UTF_DEBUG=0xffffff
export UTF_DEBUG=0x20		# DLEVEL_PROTO_RMA
#export FI_LOG_PROV=tofu
#export FI_LOG_LEVEL=Debug

export UTF_TRANSMODE=1	# AGGRESSIVE

echo "SENDONE -p (MEMORY TEST)"
echo
mpich_exec -n 2 $MPIOPT ../bin/sendone -p -l 1024 -i 100
unset MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG
unset MPICH_TOFU_SHOW_PARAMS
echo "SENDONE -p (MEMORY TEST)"
echo
mpich_exec -n 2 $MPIOPT ../bin/sendone -p -l 1048576 -i 100
echo "SENDONE -p (MEMORY TEST)"
echo
mpich_exec -n 2 $MPIOPT ../bin/sendone -p -l 1073741824 -i 10

#for LEN in 100 200 300 400 500 600 700 800 1000 2000
for LEN in 1000
do
    echo
    echo "SENDONE -s 0 " $LEN
    mpich_exec -n 2 $MPIOPT ../bin/sendone -l $LEN -i 1 -s 0
    unset MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG
    unset MPICH_TOFU_SHOW_PARAMS
# OK pattern
    echo
    echo "SENDONE -s 1 " $LEN 
    mpich_exec -n 2 $MPIOPT ../bin/sendone -l $LEN -i 2 -s 1
done
exit