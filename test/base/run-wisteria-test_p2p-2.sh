#!/bin/sh
#------ pjsub option --------#
<<<<<<< HEAD
#PJM -N "UTF-P2P"
#PJM -S	
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#PJM -g "pz0485"
#PJM -L "node=2"
#PJM -L "rscgrp=debug-o"
#PJM -L "elapse=00:01:20"
#------- Program execution -------#

export LD_LIBRARY_PATH=../../build/:$LD_LIBRARY_PATH
#MPIOPT="-of results/%n.%j.out -oferr results/%n.%j.err"
MPIOPT="-of-proc results/%n.%j.out -oferr-proc results/%n.%j.err"
export MPICH_HOME=/data/01/pz0485/z30485/mpich-tofu-org/
export PATH=$PATH:$MPICH_HOME/bin:$PATH
export LD_LIBRARY_PATH=$MPICH_HOME/lib:$LD_LIBRARY_PATH
=======
#PJM -L "node=2"
#PJM -L "rscgrp=debug-o"
#------- Program execution -------#

export LD_LIBRARY_PATH=../../build/:$LD_LIBRARY_PATH
MPIOPT="-of results/%n.%j.out -oferr results/%n.%j.err"
>>>>>>> d5cef91ab5ec7fc05f4d0ed9d75ed3930b90f601

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
<<<<<<< HEAD
#ITER=1000
ITER=100
=======
ITER=1000
>>>>>>> d5cef91ab5ec7fc05f4d0ed9d75ed3930b90f601

echo "LD_LIBRARY_PATH=" $LD_LIBRARY_PATH

echo "******************************************************"
export UTF_INFO=0x1
<<<<<<< HEAD
# UTF_MSGMODE (0: Eager, 1: Rendezous)
# UTF_TRANSMODE (0: Chained, 1: Aggressive)
#	Aggressive in default
#export UTF_MSGMODE=1
export UTF_MSGMODE=0
# 2 (UTOFU) + 4 (PROTOOL) + 8 (EAGER) = 14
#export UTF_DEBUG=0x000e
=======
export UTF_MSGMODE=1
>>>>>>> d5cef91ab5ec7fc05f4d0ed9d75ed3930b90f601
mpiexec $MPIOPT -np 2 ./test_p2p pingpong -i $ITER -l $MIN_LEN -L $MAX_LEN $VRYFY

exit

echo;
echo "******************************************************"
export UTF_INFO=0x1
export UTF_MSGMODE=0
mpiexec $MPIOPT -np 2 ./test_p2p pingpong -i $ITER -l $MIN_LEN -L $MAX_EAGERLEN $VRYFY
<<<<<<< HEAD
=======

>>>>>>> d5cef91ab5ec7fc05f4d0ed9d75ed3930b90f601
