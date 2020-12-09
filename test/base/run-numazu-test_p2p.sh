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
#PJM -L "elapse=00:04:10"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#
export LD_LIBRARY_PATH=../../build/:$LD_LIBRARY_PATH
MPIOPT="-of results/%n.%j.out -oferr results/%n.%j.err"

#
#export TOFULOG_DIR=./results
##export UTF_DEBUG=0xfffffd

#MAX_LEN=134217728	# 128 MB
MIN_LEN=1
MAX_LEN=134217728 # 128 MB
ITER=1000
echo;
echo "******************************************************"
export UTF_MSGMODE=1
echo "UTF_MSGMODE(0:eager 1:rendezvous)=" $UTF_MSGMODE
mpiexec $MPIOPT -np 2 ./test_p2p pingpong -i $ITER -l $MIN_LEN -L $MAX_LEN

exit
echo "LD_LIBRARY_PATH=" $LD_LIBRARY_PATH
echo "******************************************************"
export UTF_MSGMODE=0
echo "UTF_MSGMODE(0:eager 1:rendezvous)=" $UTF_MSGMODE
mpiexec $MPIOPT -np 2 ./test_p2p pingpong -i 20 -L 2048

exit
##################################################################################
mpiexec $MPIOPT -np 2 ./test_p2p pingpong -i 10000 -l 0
mpiexec $MPIOPT -np 2 ./test_p2p pingpong -i 10000 -l 4
mpiexec $MPIOPT -np 2 ./test_p2p pingping -i 10000 -l 0 -w
mpiexec $MPIOPT -np 2 ./test_p2p pingping -i 10000 -l 4 -w
exit

#mpiexec -np 2 ./test_p2p pingpong -i 1 -l 1000
#mpiexec -np 2 ./test_p2p pingpong -i 10000 -L 10000000 -D	#  -D stderr redirection
echo "***************************"
echo
mpiexec -np 2 ./test_p2p pingping -i 10000 -D
echo "***************************"
echo
mpiexec -np 2 ./test_p2p pingping -i 10000 -D -w
echo "EXITING"
echo
ldd ./test_p2p
echo
echo "**** Environment Variables ***"
printenv
exit

echo
echo "******************************************************"
mpiexec -np 2 ./test_p2p pingping -i 100
echo
echo "EXITING"
exit
