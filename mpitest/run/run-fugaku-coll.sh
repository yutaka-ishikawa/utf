#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-COLL" # jobname
#PJM -S		# output statistics
#
#PJM --spath "results/coll/%n.%j.stat"
#PJM -o "results/coll/%n.%j.out"
#PJM -e "results/coll/%n.%j.err"
#PJM -L "node=4"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:01:00"
#PJM -L "rscunit=rscunit_ft01,rscgrp=small"
#PJM -L proc-core=unlimited
#------- Program execution -------#

MPIOPT="-of results/coll/%n.%j.out -oferr results/coll/%n.%j.err"
export PATH=~/mpich-tofu-nv/bin:$PATH
export MPICH_HOME=~/mpich-tofu-nv/
which mpich_exec
export MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG=1
export MPICH_TOFU_SHOW_PARAMS=1
export UTF_TRANSMODE=0

#
#   coll -s  0x1: Barrier, 0x2: Reduce, 0x4: Allreduce, 0x8: Gather, 0x10: Alltoall, 0x20: Scatter
#
#mpich_exec $MPIOPT ../bin/coll -l 4 -s 0x2 -v
#mpich_exec $MPIOPT ../bin/coll -l 100 -s 0x2
#mpich_exec $MPIOPT ../bin/coll -l 256 -s 0x2
mpich_exec $MPIOPT ../bin/coll -l 400 -s 0x2 -v
mpich_exec $MPIOPT ../bin/coll -l 512 -s 0x2 -v

