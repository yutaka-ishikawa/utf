#UTF_ARCH=x86
#UTF_ARCH=fugaku
include mkdef.$(UTF_ARCH)

INSTDIR = ../bin
MPIPROGS = hello coll sendrecv
MPISRCS = hello.c coll.c sendrecv.c
MPIOBJS = $(MPISRCS:.c=.o)
SRCS = testlib.c
OBJS = $(SRCS:.c=.o)

all: $(MPIPROGS)
$(MPIPROGS): $(MPIOBJS) testlib.o
	$(MPICC) -o $@ $@.o testlib.o $(LDFLAGS)
.c.o:
	$(MPICC) -c -g  -Wall $(CFLAGS) $<
install:
	[ -d $(INSTDIR) ] || mkdir $(INSTDIR)
	$(INSTALL) $(MPIPROGS) $(INSTDIR)/
clean:
	rm -f $(MPIPROGS) *.o *~
depend:
	makedepend -I$(CCINCLUDE) $(CFLAGS) $(SRCS)

