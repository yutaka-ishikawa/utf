#UTF_ARCH=x86
#UTF_ARCH=fugaku
include dep/mkdef.$(UTF_ARCH)

CFLAGS += -I./include/ -shared -fPIC
CFLAGS += $(INCLUDES)
INCLUDES += -I./include
##CFLAGS += -DUTF_DEBUG
SRCDIR   := ./base/
BUILDDIR := ../build/
TARGET   := $(BUILDDIR)/libutf.so
INSTDIR  := ../bin

PROGS = utf.so
SRCS     := $(wildcard $(SRCDIR)/*.c)
OBJS     := $(addprefix $(BUILDDIR)/, $(SRCS:.c=.o))
DEPS	 := $(addprefix $(BUILDDIR)/, $(SRCS:.c=.d))

.PHONY: all clean
all: $(TARGET)


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
