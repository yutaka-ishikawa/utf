#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-MYSCATTER-GATHER-TAGGED" # jobname
#PJM -S		# output statistics
#PJM --spath "results/myscatter-gather/%n.%j.stat"
#PJM -o "results/myscatter-gather/%n.%j.out"
#PJM -e "results/myscatter-gather/%n.%j.err"
#
#	PJM -L "node=16:noncont"
#	PJM -L "node=8:noncont"
#PJM -L "node=12:noncont"
#PJM --mpi "max-proc-per-node=48"
#PJM -L "elapse=00:02:00"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-all,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-mck2_and_spack2,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#

#MPIOPT="-of results/myscatter-gather/%n.%j.out -oferr results/myscatter-gather/%n.%j.err"

# max RMA 8388608
###export MPIR_CVAR_REDUCE_SHORT_MSG_SIZE=100000

export MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG=1
export MPICH_TOFU_SHOW_PARAMS=1
export UTF_INFO=0x1
#export UTF_INFO=0x3		# 0x1: MESSAGE RELATED INFO, 0x2: stag info
#export UTF_INJECT_COUNT=1

export UTF_ASEND_COUNT=1	# added on 2020/01/01 20:05
#export UTF_DEBUG=0xffffff
#export FI_LOG_PROV=tofu
#export FI_LOG_LEVEL=Debug
#export UTF_INJECT_COUNT=4
#export UTF_DEBUG=0x200	# DLEVEL_ERR
#export UTF_DEBUG=12	# PROTOCOL|PROTO_EAGER

export UTF_DBGTIMER_INTERVAL=100
export UTF_DBGTIMER_ACTION=1

export MPIR_CVAR_CH4_OFI_ENABLE_TAGGED=1	# TAGGED
export UTF_TRANSMODE=0				# CHAINED

NP=512
LEN=1
#ITER=1000
#ITER=10000
#ITER=1000
#ITER=100
#ITER=10 # 00:05
#ITER=100 # 00:05
ITER=1000 # 00:05
ITER=10000 # 00:16
ITER=50000 # 01:05
#echo "mpich_exec -n $NP $MPIOPT ../src/myscatter_gather -l $LEN -i $ITER -v"
#mpich_exec -n $NP $MPIOPT ../src/myscatter_gather -l $LEN -i $ITER -v

echo "mpich_exec -n $NP $MPIOPT ../src/myscatter_gather -l $LEN -i $ITER"
mpich_exec -n $NP $MPIOPT ../src/myscatter_gather -l $LEN -i $ITER
exit


date
for LEN in 1 100 1000 10000 100000; do
    echo "mpich_exec -n $NP $MPIOPT ../src/myscatter_gather -l $LEN -i $ITER"
    mpich_exec -n $NP $MPIOPT ../src/myscatter_gather -l $LEN -i $ITER
    date
    unset MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG
    unset MPICH_TOFU_SHOW_PARAMS
    unset UTF_INFO
    echo
done

exit

#mpich_exec -n $NP $MPIOPT ../src/myscatter -l $LEN -i $ITER -v
