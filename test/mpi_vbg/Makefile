#UTF_ARCH=x86
#UTF_ARCH=fugaku
include ../../src/dep/mkdef.$(UTF_ARCH)

INCLUDES += -I../../src/include
PROGS :=
SRCS := ../../src/mpi_vbg/commgroup.c
OBJS := $(SRCS:.c=.o)


.PHONY: all install clean depend
all: $(OBJS) test_commgroup
test_commgroup: ../../src/mpi_vbg/commgroup.c
	$(MPICC) -o $@ $(INCLUDES) $(CFLAGS) -DCOMMGROUP_TEST -DDEBUG $^
$(PROGS): $(OBJS)
	$(MPICC) -o $@ $^
$(OBJS): $(SRCS)
	$(MPICC) $(INCLUDES) $(CFLAGS) -c $^
clean:
	rm -f commgroup.o test_commgroup
run-clean:
	rm -f core.*
