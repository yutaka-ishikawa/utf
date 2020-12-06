#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-COLL192" # jobname
#PJM -S		# output statistics
#PJM --spath "results/coll-192/%n.%j.stat"
#PJM -o "results/coll-192/%n.%j.out"
#PJM -e "results/coll-192/%n.%j.err"
#
#	PJM -L "node=2:noncont"
#PJM -L "node=4:noncont"
#	PJM -L "node=8:noncont"
#	PJM --mpi "max-proc-per-node=2"
#PJM --mpi "max-proc-per-node=48"
#	PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:00:50"
#	PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-mck2_and_spack2,jobenv=linux"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack1,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#

. ./mpich.env

MPIOPT="-of results/coll-192/%n.%j.out -oferr results/coll-192/%n.%j.err"

echo "TOFU_NAMED_AV = " $TOFU_NAMED_AV
echo "UTF_MSGMODE   = " $UTF_MSGMODE "(0: Eager, 1: Rendezous)"
echo "UTF_TRANSMODE = " $UTF_TRANSMODE "(0: Chained, 1: Aggressive)"
echo "MPIR_CVAR_CH4_OFI_ENABLE_TAGGED = " $MPIR_CVAR_CH4_OFI_ENABLE_TAGGED
echo "MPIR_CVAR_CH4_OFI_EAGER_MAX_MSG_SIZE = " $MPIR_CVAR_CH4_OFI_EAGER_MAX_MSG_SIZE

#   coll -s  0x1: Barrier, 0x2: Reduce, 0x4: Allreduce, 0x8: Gather, 0x10: Alltoall, 0x20: Scatter 

mpiexec -n 128 $MPIOPT ../bin/coll -l 10240 -s 0x10
#mpiexec -n 128 $MPIOPT ../bin/coll -l 1024 -s 0x10
#mpiexec -n 128 $MPIOPT ../bin/coll -l 512 -s 0x10

#mpiexec -n 24 $MPIOPT ../bin/coll -l 512 -s 0x10
#mpiexec -n 128 $MPIOPT ../bin/coll -l 8 -s 0x10  # 4096 B OK on TAGGED
#mpiexec -n 24 $MPIOPT ../bin/coll -l 512 -s 0x10	# 49152 B OK on TAGGED, but not on NONTAGGED
#mpiexec -n 128 $MPIOPT ../bin/coll -l 512 -s 0x10  # No more Eager Receiver Buffer on TAGGED
#mpiexec -n 24 $MPIOPT ../bin/coll -l 256 -s 0x10 # 24576 B OK
#mpiexec -n 24 $MPIOPT ../bin/coll -l 32 -s 0x10 # 3072 B OK
#mpiexec -n 24 $MPIOPT ../bin/coll -l 16 -s 0x10 # OK
#mpiexec -n 24 $MPIOPT ../bin/coll -l 8 -s 0x10	# 768 B OK
#mpiexec -n 24 $MPIOPT ../bin/coll -l 512 -s 0x10	# 49152 B NG
#mpiexec -n 24 $MPIOPT ../bin/coll -l 512 -s 0x10

#PASS mpiexec -n 24 $MPIOPT ../bin/coll -l 512 -s 0x27
#PASS mpiexec -n 24 $MPIOPT ../bin/coll -l 512 -s 0x7
#PASS mpiexec -n 24 $MPIOPT ../bin/coll -l 512 -s 0x3
#PASS mpiexec -n 24 $MPIOPT ../bin/coll -l 512 -s 0x1
#BAD mpiexec -n 24 $MPIOPT ../bin/coll -l 512 -s 0xff
#OK mpiexec -n 2 $MPIOPT ../bin/coll -l 512 -s 0xff
#OK mpiexec -n 8 $MPIOPT ../bin/coll -l 512 -s 0xff
#OK mpiexec -n 16 $MPIOPT ../bin/coll -l 512 -s 0xff
#BAD mpiexec -n 32 $MPIOPT ../bin/coll -l 512 -s 0xff
#mpiexec -n 128 $MPIOPT ../bin/coll -l 512 -s 0xff

exit

#################################################################################

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
export UTF_TRANSMODE=1	# Aggressive
export TOFU_NAMED_AV=1

echo "TOFU_NAMED_AV = " $TOFU_NAMED_AV
echo "UTF_MSGMODE   = " $UTF_MSGMODE "(0: Eager, 1: Rendezous)"
echo "UTF_TRANSMODE = " $UTF_TRANSMODE "(0: Chained, 1: Aggressive)"
echo "MPIR_CVAR_CH4_OFI_ENABLE_TAGGED = " $MPIR_CVAR_CH4_OFI_ENABLE_TAGGED

#   coll -s  0x1: Barrier, 0x2: Reduce, 0x4: Allreduce, 0x8: Gather, 0x10: Alltoall, 0x20: Scatter 

mpiexec -n 24 $MPIOPT ../bin/coll -l 16 -s 0x10 # 
#mpiexec -n 24 $MPIOPT ../bin/coll -l 8 -s 0x10	# 768 B OK
#mpiexec -n 24 $MPIOPT ../bin/coll -l 512 -s 0x10	# 12288B BAD
#mpiexec -n 24 $MPIOPT ../bin/coll -l 512 -s 0x10

#PASS mpiexec -n 24 $MPIOPT ../bin/coll -l 512 -s 0x27
#PASS mpiexec -n 24 $MPIOPT ../bin/coll -l 512 -s 0x7
#PASS mpiexec -n 24 $MPIOPT ../bin/coll -l 512 -s 0x3
#PASS mpiexec -n 24 $MPIOPT ../bin/coll -l 512 -s 0x1
#BAD mpiexec -n 24 $MPIOPT ../bin/coll -l 512 -s 0xff
#OK mpiexec -n 2 $MPIOPT ../bin/coll -l 512 -s 0xff
#OK mpiexec -n 8 $MPIOPT ../bin/coll -l 512 -s 0xff
#OK mpiexec -n 16 $MPIOPT ../bin/coll -l 512 -s 0xff
#BAD mpiexec -n 32 $MPIOPT ../bin/coll -l 512 -s 0xff
#mpiexec -n 128 $MPIOPT ../bin/coll -l 512 -s 0xff

#mpiexec -n 128 $MPIOPT ../bin/coll -l 512 -s 0x1

#mpiexec ../bin/coll -l 512 -v 0x1
#mpiexec ../bin/coll -l 512	# 512*192 = 393216
#echo "Coll ALL"
#mpiexec ../bin/coll -l 512 -s 0x2f -V # 512*192 = 393216
#echo "Gather test"
#mpiexec ../bin/coll -l 512 -s 0x8 -V # 512*192 = 393216
#echo "Scatter test"
#mpiexec ../bin/coll -l 512 -s 0x20 -V # 512*192 = 393216
#ldd ../bin/coll
echo
echo
