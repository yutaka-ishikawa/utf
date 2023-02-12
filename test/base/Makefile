#UTF_ARCH=x86
#UTF_ARCH=fugaku
include ../../src/dep/mkdef.$(UTF_ARCH)
CFLAGS += -I../../src/include/
CFLAGS += $(INCLUDES)
##LDFLAGS = -L../build/ -lutf -L/home/users/ea01/ea0103/riken-mpich/lib -lpmix
##LDFLAGS = -L../build/ -lutf -L$(HOME)/mpich-tofu/lib -lpmix
LDFLAGS = -L$(MPICH_HOME)/lib -lutf -lpmix

SRC_DIR=.
SRC_MPICH=/home/z30485/mpich-tofu-org
DST_HOMEDIR=/data/01/pz0485/z30485
DST_RUNDIR=$(DST_HOMEDIR)/work/mpich-org

.PHONY: all clean

PROGS := test_p2p test_memreg test_mapping test_func test_hdr
MPI_PROGS := test_vbg
##LIBS  := ../build/utf.so
SRCS  := test_p2p.c testlib.c test_memreg.c test_mapping.c test_func.c test_hdr.c
OBJS  := $(SRCS:.c=.o)
DEPS  := $(SRCS:.c=.d)
#OBJS  := $(SRCS:.c=.o) ../build/base/*.o

all: $(PROGS)

$(PROGS): $(OBJS)
#	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
	$(CC) $(CFLAGS) -o $@ $@.o testlib.o $(LDFLAGS)
$(MPI_PROGS): $(OBJS)
	$(MPICC)  -o $@ $@.o testlib.o
test_hdr: test_hdr.c
	$(CC) $(CFLAGS) -o $@ test_hdr.c
	$(CC) $(CFLAGS) -S test_hdr.c
clean:
	rm -f *.o
run-clean:
	rm -f core* tofulog-* ping*-timing-*.txt
copy:
	cp run-wisteria-test_p2p-2.sh $(DST_RUNDIR)/
	cp test_p2p $(DST_RUNDIR)/
depend:
	makedepend  -I$(INCLUDES) $(SRCS) -f - >depend
%.d: %.c
	$(CC) $(CFLAGS) -M $< >$@
include $(DEPS)
