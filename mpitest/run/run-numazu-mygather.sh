#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-MYGATHER" # jobname
#PJM -S		# output statistics
#PJM --spath "results/mygather/%n.%j.stat"
#PJM -o "results/mygather/%n.%j.out"
#PJM -e "results/mygather/%n.%j.err"
#
#	PJM -L "node=16:noncont"
#PJM -L "node=4:noncont"
#	PJM -L "node=8:noncont"
#	PJM --mpi "max-proc-per-node=20"
#PJM --mpi "max-proc-per-node=8"
#	PJM --mpi "max-proc-per-node=4"
#PJM -L "elapse=00:12:20"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-all,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-mck2_and_spack2,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#
MPIOPT="-of results/mygather/%n.%j.out -oferr results/mygather/%n.%j.err"

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
#export UTF_DEBUG=0x020000
export TFI_FIN_WIPE=1

NP=32

#LEN=1
#ITER=200
#ITER=100 # OK 03sec
#ITER=1000 # OK 03sec
#ITER=10000 # OK 04sec
#ITER=100000 # OK 14sec

#LEN=1024
#ITER=1000 # OK 03sec
#ITER=10000 # OK 14sec
#ITER=100000 # OK 35sec

LEN=1048576 # 1 MiB
#ITER=1000 # OK 8sec
#ITER=10000 # OK 1:05 
ITER=100000 # OK 09:35

#export UTF_DBGTIMER_INTERVAL=15
#export UTF_DBGTIMER_ACTION=1

mpich_exec -n $NP $MPIOPT ../src/mygather -l $LEN -i $ITER

exit
