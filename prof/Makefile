CC = fccpx -Nclang -ffj-fjprof
CFLAGS = -O3 -ffj-ilfunc -fbuiltin -fomit-frame-pointer -finline-functions -g
MPICC = mpicc

all : test_profile test_mpiprofile test_mpiprofile-gcc
test_profile: test_profile.c
	$(CC) $(CFLAGS) -o $@  $^
test_mpiprofile: test_mpiprofile.c
	export MPICH_CC="fccpx -Nclang -ffj-fjprof"; $(MPICC) $(CFLAGS) -o $@  $^
test_mpiprofile-gcc: test_mpiprofile.c
	$(MPICC) -o $@  -DNOT_FCOMPILER $^
clean:
	rm -f test_profile test_mpiprofile test_mpiprofile-gcc

