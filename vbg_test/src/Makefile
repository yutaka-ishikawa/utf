#UTF_ARCH=x86
UTF_ARCH=fugaku
include ./mkdef.$(UTF_ARCH)


.PHONY: all clean

INSTDIR = ../run
PROGS = utf_large_scale_test
SRCS  = utf_large_scale_test.c

CFLAGS += $(INCDIR)


all: $(PROGS) install

$(PROGS): $(SRCS)
	$(MPICC) $(CFLAGS) -o $@ $^
install:
	[ -d $(INSTDIR) ] || mkdir $(INSTDIR)
	$(INSTALL) $(PROGS) $(INSTDIR)/
clean:
	rm -f $(PROGS) *~

