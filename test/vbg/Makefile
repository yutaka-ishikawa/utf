#UTF_ARCH=x86
#UTF_ARCH=fugaku
include ../../src/dep/mkdef.$(UTF_ARCH)

.PHONY: all clean

PROGS := utf_call_reduce_sample
LIBS  := ../../build/libutf.so
SRCS  := utf_call_reduce_sample.c vname.c
OBJS  := $(SRCS:.c=.o)
FJCFLAGS += -I../../src/include -I../../src/vbg -I/opt/FJSVxos/devkit/aarch64/rfs/usr/include/FJSVtcs/ple
FJLDFLAGS +=  -L../../build/ -lutf

all: $(PROGS)

$(PROGS): $(OBJS)
	(PATH=/opt/FJSVxos/devkit/aarch64/bin/:$(PATH); \
	export OMPI_CC=aarch64-linux-gnu-gcc; \
	export OMPI_LDFLAGS=-L/opt/FJSVxtclanga/tcsds-1.2.27b/lib64; \
	export CFLAGS=-O2; $(FJMPICC) $(FJCFLAGS) $^ -o $@ $(FJLDFLAGS))
#utf_call_reduce_sample.o: utf_call_reduce_sample.c
.c.o:
	(PATH=/opt/FJSVxos/devkit/aarch64/bin/:$(PATH); \
	export OMPI_CC=aarch64-linux-gnu-gcc; \
	export OMPI_LDFLAGS=-L/opt/FJSVxtclanga/tcsds-1.2.27b/lib64; \
	export CFLAGS=-O2; $(FJMPICC) $(FJCFLAGS) -c $^)

clean:
	rm -f *.o
run-clean:
	rm -f core*
