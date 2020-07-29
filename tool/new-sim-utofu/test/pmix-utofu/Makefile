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

LDFLAGS		= -L/home/ishikawa/riken-mpich/lib

#CPPFLAGS	+= -DNDEBUG
#CPPFLAGS	+= -I.

# =======================================================================

PA6_SRCS	= \
		a6.c \
		$(EOA)


PA6_HDRS	= \
		$(EOA)

PA6_OBJS	= $(PA6_SRCS:.c=.o)
PA6_AOUT	= a6.out
PA6_LDFLAGS	= $(LDFLAGS)
PA6_LIBS	= -lpmix -ltofutop -ltofucom

HDRS		+= $(PA6_HDRS)
OBJS		+= $(PA6_OBJS)
BINS		+= $(PA6_AOUT)

# =======================================================================

all:		$(BINS)

clean:
		$(RM) $(BINS)
		$(RM) $(OBJS)
run-clean:
		$(RM) *.sh.o* *.sh.e*

# =======================================================================

$(PA6_AOUT):	$(PA6_OBJS)
		$(CC) $(CFLAGS_LD_NOPIC) -o $(@) \
		$(PA6_OBJS) \
		$(PA6_LDFLAGS) $(PA6_LIBS)

$(PA6_OBJS):	$(PA6_HDRS)

