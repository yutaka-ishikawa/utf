#!/bin/sh
#------ pjsub option --------#
#PJM -N "UTF-UTOFU2" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#
#	PJM -L "node=2:noncont"
#PJM -L "node=2"
#	PJM -L "node=1"
#	PJM --mpi "max-proc-per-node=2"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:00:20"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack1,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#

# For utf.so
export LD_LIBRARY_PATH=../build/:$LD_LIBRARY_PATH

# The stderr redirection is enabled if -D option is specified
export TOFULOG_DIR=./results

#export PMIX_DEBUG=1
#export UTF_DEBUG=0xfffffd
#
export UTF_MSGMODE=1	# 0: eager 1: rendezvous
echo "LD_LIBRARY_PATH=" $LD_LIBRARY_PATH
echo "UTF_MSGMODE(0:eager 1:rendezvous)=" $UTF_MSGMODE

echo "******************************************************"
mpiexec -np 2 ./test_p2p pingpong -i 1000 -L 16777216 -D	#  -D stderr redirection
echo "***************************"
echo
echo
ldd ./test_p2p
echo "**** Environment Variables ***"
printenv
exit

###mpiexec -np 2 ./test_p2p pingpong -i 10 -l 512 -D	#  -D stderr redirection
###exit
