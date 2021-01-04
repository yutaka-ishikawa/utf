#!/bin/sh
#------ pjsub option --------#
#PJM -N "UTF-UTOFU2" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#
#PJM -L "node=2"
#	PJM --mpi "max-proc-per-node=2"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:01:30"
#PJM -L "rscunit=rscunit_ft01,rscgrp=eap-small,jobenv=linux2"
#PJM -L proc-core=unlimited
#------- Program execution -------#
export LD_LIBRARY_PATH=../../build/:$LD_LIBRARY_PATH
MPIOPT="-of results/%n.%j.out -oferr results/%n.%j.err"

#
#export TOFULOG_DIR=./results
##export UTF_DEBUG=0xfffffd

#MAX_LEN=134217728	# 128 MiB
#MIN_LEN=1
#MAX_LEN=134217728 # 128 MiB
#MAX_EAGERLEN=262144 # 256 KiB
#MIN_LEN=128
#MIN_LEN=256
#ITER=1000
#ITER=2

VRYFY=-v
MIN_LEN=1
MAX_EAGERLEN=32768
MAX_LEN=134217728
ITER=1000

echo "LD_LIBRARY_PATH=" $LD_LIBRARY_PATH

echo "******************************************************"
export UTF_INFO=0x1
export UTF_MSGMODE=1
mpiexec $MPIOPT -np 2 ./test_p2p pingpong -i $ITER -l $MIN_LEN -L $MAX_LEN $VRYFY

exit

echo;
echo "******************************************************"
export UTF_INFO=0x1
export UTF_MSGMODE=0
mpiexec $MPIOPT -np 2 ./test_p2p pingpong -i $ITER -l $MIN_LEN -L $MAX_EAGERLEN $VRYFY

