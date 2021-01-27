1. Basic Test
  hello.c     -- Testing MPI_Init.
		 run-numazu-hello.sh, run-fugaku-hello.sh, run-fugaku-mck1-hello.sh
  sendone.c   -- MPI_Send/MPI_Recv test.
		 run-numazu-sendone.sh, run-fugaku-sendone.sh
  pingpong.c  -- pingpong benchmark.
		 run-numazu-mpi-utf.sh, run-numazu-pingpong-f.sh, run-numazu-pingpong-timing.sh,
		 run-numazu-pingpong.sh, run-fugaku-pingpong.sh
  sendrecv.c  -- send and receive test.
		 run-numazu-p2p2.sh
  myscatter.c -- simple scatter program.
		 run-numazu-myscatter.sh

2. Collective Test
  colld.c -- MPI collective test, data type is MPI_DOUBLE.
		run-numazu-allreduce-64.sh, run-numazu-test.sh
		run-numazu-coll-{2,4,8,16,32,48,64,192}.sh
		run-fugaku-alltoall-1rack.sh, 
		run-numazu-coll-{1,9,72}rack.sh
  coll.c  -- MPI collective test, data type is MPI_INT (will be overflow in large procs).

3. MPI Window 
  rma.c -- simple test for MPI Window feature.

4. UTF with MPICH Test
  shmem.c        -- utf_shmem test on MPICH environment.
		    run-numazu-shmem.sh
  test_mpi_utf.c -- utf_init test with MPI_Init.
		    run-numazu-mpi-utf.sh
  test_hook.c    -- Testing an MPICH modification to get ranks in a communicator.

5. MPICH with VBG
  test_vbg.c            -- Testing utf VBG implementation on MPICH environment.
			   run-numazu-vbg.sh
  test_collective_vbg.c -- MPI_Barrier/MPI_Reduce/MPI_Allreduce with VBG.
			   run-numazu-coll-vbg.sh, run-fugaku-coll-vbg-1rack.sh
##
  testlib.c -- library.
