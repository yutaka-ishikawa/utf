#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-COLL144" # jobname
#PJM -S		# output statistics
#
#PJM --spath "results/coll-144rack/%n.%j.stat"
#PJM -o "results/coll-144rack/%n.%j.out"
#PJM -e "results/coll-144rack/%n.%j.err"
#
#PJM -L "node=55296"
#	PJM -L "node=27648"
#	PJM -L "node=1152"
#	PJM --mpi "max-proc-per-node=1"
#PJM --mpi "max-proc-per-node=4"
#	PJM --mpi "max-proc-per-node=32"
#	PJM --mpi "max-proc-per-node=48"
#PJM -L "elapse=00:15:30"
#PJM -L "rscunit=rscunit_ft01,rscgrp=eap-large,jobenv=linux2"
#PJM -L proc-core=unlimited
#------- Program execution -------#

. ./mpich.env
MPIOPT="-of results/coll-144rack/%n.%j.out -oferr results/coll-144rack/%n.%j.err"

#
#   coll -s  0x1: Barrier, 0x2: Reduce, 0x4: Allreduce, 0x8: Gather, 0x10: Alltoall, 0x20: Scatter
#

#
#   coll -s  0x1: Barrier, 0x2: Reduce, 0x4: Allreduce, 0x8: Gather, 0x10: Alltoall, 0x20: Scatter
#
#
#echo  "checking collective except Alltoall"
echo "checking Barrier"
time mpiexec -n 110592 $MPIOPT ../bin/colld-f -l 1 -s 0x1	#
echo "checking Reduce"
time mpiexec -n 110592 $MPIOPT ../bin/colld-f -l 1 -s 0x2	#
echo; echo; echo
echo "checking Allreduce"
time mpiexec -n 110592 $MPIOPT ../bin/colld-f -l 1 -s 0x4	#
echo; echo; echo
echo "checking Gather"
time mpiexec -n 110592 $MPIOPT ../bin/colld-f -l 1 -s 0x8	#
echo; echo; echo
echo "checking Scatter"
time mpiexec -n 110592 $MPIOPT ../bin/colld-f -l 1 -s 0x20	#
echo; echo; echo
echo "checking Alltoall"
time mpiexec -n 110592 $MPIOPT ../bin/colld-f -l 1 -s 0x10	#

exit
###########################################################################
###########################################################################
###########################################################################
export LD_LIBRARY_PATH=${HOME}/mpich-tofu/lib:$LD_LIBRARY_PATH

export MPIR_CVAR_OFI_USE_PROVIDER=tofu
export MPICH_CH4_OFI_ENABLE_SCALABLE_ENDPOINTS=1
export MPIR_CVAR_ALLTOALL_SHORT_MSG_SIZE=2147483647 # 32768 in default (integer value)
export MPIR_CVAR_CH4_OFI_ENABLE_ATOMICS=-1
export MPIR_CVAR_CH4_OFI_ENABLE_MR_VIRT_ADDRESS=1
export MPIR_CVAR_CH4_OFI_ENABLE_RMA=1
export MPIR_CVAR_CH4_OFI_ENABLE_TAGGED=0
export MPIR_CVAR_CH4_OFI_CAPABILITY_SETS_DEBUG=1
export UT_MSGMODE=0
export UTF_TRANSMODE=1  # Aggressive                                                                                      
export TOFU_NAMED_AV=1

echo "TOFU_NAMED_AV = " $TOFU_NAMED_AV
echo "UTF_MSGMODE   = " $UTF_MSGMODE "(0: Eager, 1: Rendezous)"
echo "UTF_TRANSMODE = " $UTF_TRANSMODE "(0: Chained, 1: Aggressive)"
echo "MPIR_CVAR_CH4_OFI_ENABLE_TAGGED = " $MPIR_CVAR_CH4_OFI_ENABLE_T
#
#   coll -s  0x1: Barrier, 0x2: Reduce, 0x4: Allreduce, 0x8: Gather, 0x10: Alltoall, 0x20: Scatter
#
#
echo  "checking collective except Alltoall"
mpiexec -n 110592 $MPIOPT ../bin/coll -l 512 -s 0x2f	#  5 min 57 sec == 72 rack
#mpiexec -n 131072 $MPIOPT ../bin/coll -l 512 -s 0x2f	# ERROR
#mpiexec -n 165888 $MPIOPT ../bin/coll -l 512 -s 0x2f	# ERROR
#mpiexec -n 110592 $MPIOPT ../bin/coll -l 512 -s 0x2f	#  5 min 57 sec == 72 rack
#mpiexec -n 55296 $MPIOPT ../bin/coll -l 512 -s 0x2f	#  3 min 00 sec
#mpiexec -n 36864 $MPIOPT ../bin/coll -l 512 -s 0x2f	#  2 min 13 sec
#mpiexec -n 1152 $MPIOPT ../bin/coll -l 512 -s 0x2f	#  57 sec
#mpiexec -n 3456 $MPIOPT ../bin/coll -l 512 -s 0x2f	#  sec

exit
#mpiexec ../bin/coll -l 5120 -v -s 


#echo
#echo
##ldd ../bin/coll
#	-x FI_LOG_PROV=tofu \
#	-x MPICH_DBG=FILE \
#	-x MPICH_DBG_CLASS=COLL \
#	-x MPICH_DBG_LEVEL=TYPICAL \
#
#	-x PMIX_DEBUG=1 \
#	-x FI_LOG_LEVEL=Debug \
#
