#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-BCAST"
#PJM -S
#PJM --spath "results/mpich-org/bcast/%n.%j.stat"
#PJM -o "results/mpich-org/bcast/%n.%j.out"
#PJM -e "results/mpich-org/bcast/%n.%j.err"
#	#PJM -L "node=4"
#PJM -L "node=8"
#PJM -g "pz0485"
#PJM -L "rscgrp=debug-o"
#PJM --mpi "max-proc-per-node=4"
#PJM -L "elapse=00:01:00"
#PJM -L proc-core=unlimited
#------- Program execution -------#

MPIOPT="-of-proc results/mpich-org/bcast/%n.%j.out -oferr-proc results/mpich-org/bcast/%n.%j.err"
export MPICH_HOME=/data/01/pz0485/z30485/mpich-tofu-org/
export PATH=$PATH:$MPICH_HOME/bin:$PATH
which mpich_exec
export MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG=1

#
#
# UTF_MSGMODE (0: Eager, 1: Rendezous)
#	Eager in default
# UTF_TRANSMODE (0: Chained, 1: Aggressive)
#	Aggressive in default
export MPICH_TOFU_SHOW_PARAMS=1
## export UTF_TRANSMODE=0  # looks work, really ??
export UTF_TRANSMODE=1
export UTF_MSGMODE=0

ITER=1000
#NP=16
NP=32
#LEN=16384 ##4096B
LEN=349525

CMD=../bin-org/bcast
##RUN_HOME=/data/01/pz0485/z30485/work/mpich-org/bcast/

mpich_exec $MPIOPT $CMD -l $LEN -i $ITER
exit
#mpiexec -n $NP $MPIOPT ../bin/bcast-f -l $LEN -i $ITER


