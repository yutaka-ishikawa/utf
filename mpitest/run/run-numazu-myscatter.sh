#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-MYSCATTER" # jobname
#PJM -S		# output statistics
#PJM --spath "results/myscatter/%n.%j.stat"
#PJM -o "results/myscatter/%n.%j.out"
#PJM -e "results/myscatter/%n.%j.err"
#
#	PJM -L "node=16:noncont"
#PJM -L "node=8:noncont"
#PJM --mpi "max-proc-per-node=20"
#	PJM --mpi "max-proc-per-node=8"
#	PJM --mpi "max-proc-per-node=4"
#PJM -L "elapse=00:01:30"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-all,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-mck2_and_spack2,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#
MPIOPT="-of results/myscatter/%n.%j.out -oferr results/myscatter/%n.%j.err"
# max RMA 8388608
###export MPIR_CVAR_REDUCE_SHORT_MSG_SIZE=100000

export MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG=1
export MPICH_TOFU_SHOW_PARAMS=1
export UTF_INFO=0x1
#export UTF_INFO=0x3		# 0x1: MESSAGE RELATED INFO, 0x2: stag info
export UTF_INJECT_COUNT=1

export UTF_ASEND_COUNT=1	# added on 2020/01/01 20:05
#export UTF_DEBUG=0xffffff
#export FI_LOG_PROV=tofu
#export FI_LOG_LEVEL=Debug
#export UTF_INJECT_COUNT=4
#export UTF_DEBUG=0x200	# DLEVEL_ERR
#export UTF_DEBUG=12	# PROTOCOL|PROTO_EAGER
#export UTF_DBGTIMER_INTERVAL=40
#export UTF_DBGTIMER_ACTION=1

#NP=64	# 4sec
#NP=256
#NP=127 # 10sec
#NP=128	# 08sec
#NP=129	# 07sec
#NP=160	#
NP=130
LEN=1
ITER=100
mpich_exec -n $NP $MPIOPT ../src/myscatter -l $LEN -i $ITER
#mpich_exec -n $NP $MPIOPT ../src/myscatter -l $LEN -i $ITER -v

exit

