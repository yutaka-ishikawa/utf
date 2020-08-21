#UTF_ARCH=x86
#UTF_ARCH=fugaku
include dep/mkdef.$(UTF_ARCH)

INSTDIR  := $(HOME)/mpich-tofu
INSTALL  := install

CFLAGS += -I./include/ -shared -fPIC
CFLAGS += $(INCLUDES)
SRCDIR   := ./base/
INCDIR   := ./include
BUILDDIR := ../build/
INCLUDES += -I$(INCDIR)
CFLAGS += -DUTF_DEBUG
TARGET   := $(BUILDDIR)/libutf.so

PROGS = utf.so
SRCS     := $(wildcard $(SRCDIR)/*.c)
OBJS     := $(addprefix $(BUILDDIR)/, $(SRCS:.c=.o))
DEPS	 := $(addprefix $(BUILDDIR)/, $(SRCS:.c=.d))

.PHONY: all install clean
all: $(TARGET)
install:
	[ -d $(INSTDIR)/lib ] || mkdir $(INSTDIR)/lib
	[ -d $(INSTDIR)/include ] || mkdir $(INSTDIR)/include
	$(INSTALL) $(BUILDDIR)/libutf.so $(INSTDIR)/lib
	$(INSTALL) $(INCDIR)/utf.h $(INSTDIR)/include

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
$(BUILDDIR)/%.o: %.c
	@mkdir -p $(BUILDDIR)/$(SRCDIR)
	$(CC) $(CFLAGS) -c  $< -o $@

clean:
	rm -f $(BUILDDIR)/$(SRCDIR)/*.o $(TARGET)
depend:
	makedepend -p $(BUILDDIR)  -I$(INCLUDES) $(SRCS) -f dep/depend
#	makedepend -I$(INCLUDES) $(SRCS)
#include dep/depend

##depend: $(DEPS)
##$(BUILDDIR)/%.d: %.c
##	$(CC) $(CFLAGS) -M $<  >  $@
##include $(DEPS)
