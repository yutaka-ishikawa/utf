# =======================================================================

-include mdep.mk

# =======================================================================

CFLAGS_DEF	?= -Wall
CFLAGS_OPT	?= -O0 -g
#CFLAGS_OPT	?= -O3 -g
CFLAGS_PIC	?= -fPIC

CFLAGS_LD_NOPIC	= $(CFLAGS_DEF) $(CFLAGS_OPT)
CFLAGS_LD_PIC	= $(CFLAGS_DEF) $(CFLAGS_OPT) $(CFLAGS_PIC)
CFLAGS		= $(CFLAGS_DEF) $(CFLAGS_OPT) $(CFLAGS_PIC)

#CPPFLAGS	+= -DNDEBUG
CPPFLAGS	+= -I./include

INSTALL		?= install

# =======================================================================

L10_SRCS	= \
		pmix_tsim.c \
		pmix_extra.c \
		dfile2.c \
		$(EOA)


L10_HDRS	= \
		include/pmix.h \
		include/pmix_fjext.h \
		$(EOA)

L10_OBJS	= $(L10_SRCS:.c=.o)
L10_AOUT	= libpmix.so
L10_LDFLAGS	= $(LDFLAGS)
L10_LIBS	= -shared -ltofutop

HDRS		+= $(L10_HDRS)
OBJS		+= $(L10_OBJS)
BINS		+= $(L10_AOUT)

# =======================================================================

all:		$(BINS)

clean:
		$(RM) $(BINS)
		$(RM) $(OBJS)

install:
		$(INSTALL) libpmix.so $(TOFU_HOME)/$(ARCH)/lib
#YI		$(INSTALL) -m 644 include/pmix.h $(TOFU_HOME)/include
#YI		$(INSTALL) -m 644 include/pmix_fjext.h $(TOFU_HOME)/include

# =======================================================================

$(L10_AOUT):	$(L10_OBJS)
		$(CC) $(CFLAGS_LD_NOPIC) -o $(@) \
		$(L10_OBJS) \
		$(L10_LDFLAGS) $(L10_LIBS)

$(L10_OBJS):	$(L10_HDRS)

