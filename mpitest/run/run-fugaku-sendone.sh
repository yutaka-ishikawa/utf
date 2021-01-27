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
#PJM -L "elapse=00:00:5"
#PJM -L "rscunit=rscunit_ft01,rscgrp=eap-small,jobenv=linux2"
#PJM -L proc-core=unlimited
#------- Program execution -------#
SAVED_LD_LIBRARY_PATH=$LD_LIBRARY_PATH

MPIOPT="-of results/%n.%j.out -oferr results/%n.%j.err"

export MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG=1
export MPICH_TOFU_SHOW_PARAMS=1
#export UTF_INFO=0x1
#export UTF_DEBUG=0xffffff
#export FI_LOG_PROV=tofu
#export FI_LOG_LEVEL=Debug

export UTF_TRANSMODE=1	# AGGRESSIVE
mpich_exec -n 2 $MPIOPT ../bin/sendone
exit
