#UTF_ARCH=x86
#UTF_ARCH=fugaku
include ../dep/mkdef.$(UTF_ARCH)

INSTDIR  := $(HOME)/mpich-tofu
INSTALL  := install

# -DDEBUGLOG for VBG
CFLAGS += -I../include/ -shared -fPIC -Wall -DDEBUGLOG
CFLAGS += $(INCLUDES)
SRCDIR   := ./base/
SRC_VBGDIR  := ./vbg/
INCDIR   := ./include
BUILDDIR := ../../build/
INCLUDES += -I$(INCDIR)
CFLAGS += -DUTF_DEBUG
TARGET   := $(BUILDDIR)/libmpi_vbg.so

PROGS = mpi_vbg.so
SRCS     := mpi_utf.c commgroup.c
OBJS     := mpi_utf.o
DEPS	 := mpi_utf.d

.PHONY: all install clean depend
all: $(TARGET)
install:
	[ -d $(INSTDIR)/lib ] || mkdir $(INSTDIR)/lib
	$(INSTALL) $(BUILDDIR)/libmpi_vbg.so $(INSTDIR)/lib
	$(INSTALL) -m 0555 mpich_exec $(INSTDIR)/bin
	$(INSTALL) -m 0666 mpich.env $(INSTDIR)/lib

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
%.o : %.c
	$(MPICC) $(CFLAGS) -c -MMD $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
#
-include $(DEPS)
