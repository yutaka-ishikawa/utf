#!/bin/sh
#------ pjsub option --------#
#PJM -N "UTF-VBG" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#
#PJM -L "node=2"
#	PJM --mpi "max-proc-per-node=1"
#PJM --mpi "max-proc-per-node=4"
#PJM -L "elapse=00:00:10"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#

#MPI_HOME=/opt/FJSVxtclanga/tcsds-1.2.27b
#export PATH=${MPI_HOME}/bin:$PATH
#export LD_LIBRARY_PATH=${MPI_HOME}/lib64:${LD_LIBRARY_PATH}

export LD_LIBRARY_PATH=../../build/:$LD_LIBRARY_PATH

echo "LD_LIBRARY_PATH=" $LD_LIBRARY_PATH

mpiexec utf_call_reduce_sample

exit
