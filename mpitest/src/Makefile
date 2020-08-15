#UTF_ARCH=x86
#UTF_ARCH=fugaku
include mkdef.$(UTF_ARCH)

INSTDIR = ../bin
PROGS = hello
SRCS = hello.c testlib.c
OBJS = $(SRCS:.c=.o)

$(PROGS): $(OBJS)
	$(MPICC) -o $@ $^ $(LDFLAGS)
.c.o:
	$(MPICC) -c -g  -Wall $(CFLAGS) $<
install:
	[ -d $(INSTDIR) ] || mkdir $(INSTDIR)
	$(INSTALL) $(PROGS) $(INSTDIR)/
clean:
	rm -f $(PROGS) *.o *~
depend:
	makedepend -I$(CCINCLUDE) $(CFLAGS) $(SRCS)

