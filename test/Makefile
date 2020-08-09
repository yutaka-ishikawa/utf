#UTF_ARCH=x86
#UTF_ARCH=fugaku
include ../src/dep/mkdef.$(UTF_ARCH)
CFLAGS += -I../src/include/
CFLAGS += $(INCLUDES)
LDFLAGS = -L../build/ -lutf

.PHONY: all clean

PROGS := test_p2p
LIBS  := ../build/utf.so
SRCS  := test_p2p.c testlib.c
OBJS  := $(SRCS:.c=.o)
#OBJS  := $(SRCS:.c=.o) ../build/base/*.o

all: $(PROGS)

$(PROGS): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
clean:
	rm -f *.o
run-clean:
	rm -f core*
depend:
	makedepend  -I$(INCLUDES) $(SRCS) -f - >depend
include depend
