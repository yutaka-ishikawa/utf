#UTF_ARCH=x86
#UTF_ARCH=fugaku
include mkdef.$(UTF_ARCH)

INSTDIR = ../bin
MPIPROGS = hello coll colld sendrecv shmem test_vbg test_collective_vbg colld-f
MPISRCS = hello.c coll.c colld.c sendrecv.c shmem.c test_vbg.c test_collective_vbg.c
MPIOBJS = $(MPISRCS:.c=.o)
SRCS = testlib.c
OBJS = $(SRCS:.c=.o)

all: $(MPIPROGS)
$(MPIPROGS): $(MPIOBJS) testlib.o
	$(MPICC) -o $@ $@.o testlib.o $(LDFLAGS)
colld-f: colld.c
	$(FJMPICC) -o $@ colld.c testlib.c -DFJMPI
.c.o:
	$(MPICC) -c -g  -Wall $(CFLAGS) $<
install:
	[ -d $(INSTDIR) ] || mkdir $(INSTDIR)
	$(INSTALL) $(MPIPROGS) $(INSTDIR)/
clean:
	rm -f $(MPIPROGS) *.o *~
depend:
	makedepend -I$(CCINCLUDE) $(CFLAGS) $(SRCS)

