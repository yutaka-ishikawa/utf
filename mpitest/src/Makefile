#UTF_ARCH=x86
#UTF_ARCH=fugaku
include mkdef.$(UTF_ARCH)

CFLAGS += -I../../src/include
INSTDIR = ../bin
MPIPROGS = hello coll colld sendrecv shmem test_vbg test_collective_vbg pingpong test_mpi_utf sendone myscatter mygather test_hook myscatter_gather
FJMPIPROGS =  colld-f pingpong-f
MPISRCS = hello.c coll.c colld.c sendrecv.c shmem.c test_vbg.c test_collective_vbg.c pingpong.c test_mpi_utf.c sendone.c myscatter.c mygather.c test_hook.c myscatter_gather.c
MPIOBJS = $(MPISRCS:.c=.o)
SRCS = testlib.c
OBJS = $(SRCS:.c=.o)

all: $(MPIPROGS) $(FJMPIPROGS) test_makekey
$(MPIPROGS): $(MPIOBJS) testlib.o
	$(MPICC) -o $@ $@.o testlib.o $(LDFLAGS)
colld-f: colld.c
	$(FJMPICC) -o $@ colld.c testlib.c -DFJMPI -I../../src/include
pingpong-f: pingpong.c
	$(FJMPICC) -o $@ pingpong.c testlib.c -DFJMPI -I../../src/include
.c.o:
	$(MPICC) -c -g  -Wall $(CFLAGS) $<
sendone.o: makekey.h
test_makekey: test_makekey.c makekey.h
	gcc -o test_makekey test_makekey.c
install:
	[ -d $(INSTDIR) ] || mkdir $(INSTDIR)
	$(INSTALL) $(MPIPROGS) $(FJMPIPROGS) $(INSTDIR)/
clean:
	rm -f $(MPIPROGS) *.o *~
depend:
	makedepend -I$(CCINCLUDE) $(CFLAGS) $(SRCS)

