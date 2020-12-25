#UTF_ARCH=x86
#UTF_ARCH=fugaku
include dep/mkdef.$(UTF_ARCH)

INSTDIR  := $(HOME)/mpich-tofu
INSTALL  := install

# -DDEBUGLOG for VBG
#CFLAGS += -I../include/ -shared -fPIC -Wall -DDEBUGLOG
CFLAGS += -I../include/ -shared -fPIC -Wall
CFLAGS += $(INCLUDES)
SRCDIR   := ./base/
SRC_VBGDIR  := ./vbg/
INCDIR   := ./include
BUILDDIR := ../build/
INCLUDES += -I$(INCDIR)
#CFLAGS += -DUTF_DEBUG -g
TARGET   := $(BUILDDIR)/libutf.so

PROGS = utf.so
SRCS     := $(wildcard $(SRCDIR)/*.c) $(wildcard $(SRC_VBGDIR)/*.c)
OBJS     := $(addprefix $(BUILDDIR)/, $(SRCS:.c=.o))
DEPS	 := $(addprefix $(BUILDDIR)/, $(SRCS:.c=.d))

.PHONY: all install clean depend
all: $(TARGET)
install:
	[ -d $(INSTDIR)/lib ] || mkdir $(INSTDIR)/lib
	[ -d $(INSTDIR)/include ] || mkdir $(INSTDIR)/include
	$(INSTALL) $(BUILDDIR)/libutf.so $(INSTDIR)/lib
	$(INSTALL) $(INCDIR)/utf.h $(INSTDIR)/include

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
$(BUILDDIR)/%.o: %.c
	@mkdir -p $(BUILDDIR)/$(SRCDIR) $(BUILDDIR)/$(SRC_VBGDIR)
	$(CC) $(CFLAGS) -c  -MMD $< -o $@

clean:
	rm -f $(BUILDDIR)/$(SRCDIR)/*.o $(BUILDDIR)/$(SRC_VBGDIR)/*.o \
	     $(BUILDDIR)/$(SRCDIR)/*.d $(BUILDDIR)/$(SRC_VBGDIR)/*.d $(TARGET)
#
-include $(DEPS)
