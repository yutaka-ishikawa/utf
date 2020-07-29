ARCH		= $(shell arch)
CC		= gcc
#CFLAGS_DEF	= -Wall -std=c11
#CFLAGS_DEF	= -Wall -std=gnu99
CFLAGS_DEF	= -Wall -std=gnu11

TOFU_HOME	= ../..
CPPFLAGS	+= -I$(TOFU_HOME)/include
LDFLAGS		+= -L$(TOFU_HOME)/$(ARCH)/lib

