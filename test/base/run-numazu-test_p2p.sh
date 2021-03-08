#!/bin/sh
#------ pjsub option --------#
#PJM -N "UTF-PINGPONG" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#
#PJM -L "node=2"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:01:50"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-mck2_and_spack2,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#
export LD_LIBRARY_PATH=../../build/:$LD_LIBRARY_PATH
MPIOPT="-of results/%n.%j.out -oferr results/%n.%j.err"

#
#export TOFULOG_DIR=./results
export UTF_INFO=0x1
#export UTF_COMDEBUG=1
##export UTF_DEBUG=0xfffffd

#MAX_LEN=134217728	# 128 MiB
#MIN_LEN=1
#MAX_LEN=134217728 # 128 MiB
#MAX_EAGERLEN=262144 # 256 KiB
#MIN_LEN=128
#MIN_LEN=256
#ITER=1000
#export UTF_TRANSMODE=1
#export UTF_TRANSMODE=0
#export UTF_MSGMODE=1
#ITER=1
#mpiexec $MPIOPT -np 2 ./test_p2p pingpong -i $ITER -l $MIN_LEN -L $MAX_LEN $VRYFY

VRYFY=-v
MIN_LEN=1
MAX_EAGERLEN=32768
#MAX_LEN=1024
MAX_LEN=134217728
ITER=1000

echo "LD_LIBRARY_PATH=" $LD_LIBRARY_PATH

echo "******************************************************"
export UTF_INFO=0x1
#export UTF_DEBUG=0x10

export UTF_MSGMODE=1	# RENDEZVOUS
#export UTF_MSGMODE=0	# EAGER

#export UTF_TRANSMODE=0	# CHAINE
export UTF_TRANSMODE=1	# AGRESSIVE

mpiexec $MPIOPT -np 2 ./test_p2p pingpong -i $ITER -l $MIN_LEN -L $MAX_LEN $VRYFY

exit

echo;
echo "******************************************************"
export UTF_INFO=0x1
export UTF_MSGMODE=0
mpiexec $MPIOPT -np 2 ./test_p2p pingpong -i $ITER -l $MIN_LEN -L $MAX_EAGERLEN $VRYFY

