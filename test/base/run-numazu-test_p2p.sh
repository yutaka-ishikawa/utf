#!/bin/sh
#------ pjsub option --------#
#PJM -N "UTF-UTOFU" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#
#PJM -L "node=2"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:00:10"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#
export LD_LIBRARY_PATH=../build/:$LD_LIBRARY_PATH
#
#export TOFULOG_DIR=./results

echo "LD_LIBRARY_PATH=" $LD_LIBRARY_PATH
echo "******************************************************"
mpiexec -np 2 ./test_p2p pingpong -i 1-l 1000
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
