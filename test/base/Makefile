#UTF_ARCH=x86
#UTF_ARCH=fugaku
include ../../src/dep/mkdef.$(UTF_ARCH)
CFLAGS += -I../../src/include/
CFLAGS += $(INCLUDES)
##LDFLAGS = -L../build/ -lutf -L/home/users/ea01/ea0103/riken-mpich/lib -lpmix
LDFLAGS = -L../build/ -lutf -L$(HOME)/mpich-tofu/lib -lpmix

.PHONY: all clean

PROGS := test_p2p test_memreg test_mapping test_func
MPI_PROGS := test_vbg
LIBS  := ../build/utf.so
SRCS  := test_p2p.c testlib.c test_memreg.c test_mapping.c test_func.c
OBJS  := $(SRCS:.c=.o)
#OBJS  := $(SRCS:.c=.o) ../build/base/*.o

all: $(PROGS)

$(PROGS): $(OBJS)
#	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
	$(CC) $(CFLAGS) -o $@ $@.o testlib.o $(LDFLAGS)
$(MPI_PROGS): $(OBJS)
	$(MPICC)  -o $@ $@.o testlib.o
clean:
	rm -f *.o
run-clean:
	rm -f core* tofulog-* ping*-timing-*.txt
depend:
	makedepend  -I$(INCLUDES) $(SRCS) -f - >depend
include depend
