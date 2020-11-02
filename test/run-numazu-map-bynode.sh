#!/bin/sh
#------ pjsub option --------#
#PJM -N "UTF-MAP-BYNODE" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#
#PJM -L "node=4"
#PJM --mpi "max-proc-per-node=8"
#PJM --mpi "rank-map-bynode"
#	PJM --mpi "rank-map-bychip" #default
#PJM -L "elapse=00:01:30"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#
export LD_LIBRARY_PATH=../build/:$LD_LIBRARY_PATH
#

echo "RANK-MAP-BYNODE"
for np in `seq 32`
do
  echo " np = " $np
   mpiexec -of results/%n.%j.out -np $np ./test_mapping
done

exit

##export UTF_DEBUG=0x400
#mpiexec -np 3 ./test_mapping
#mpiexec -of results/%n.%j.out -np 4 ./test_mapping
#printenv
