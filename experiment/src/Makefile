#UTF_ARCH=x86
#UTF_ARCH=fugaku
include mkdef.$(UTF_ARCH)

INSTDIR = ../bin
PROGS = test_utofu
SRCS = test_utofu.c testlib.c utf_init.c utf_procmap.c
OBJS = $(SRCS:.c=.o)

#
# Compile options:
#	PEEK_MEMORY:	peek recv buffer to check if a message arrives
#	REMOTE_PUT:	use utofu_put. utofu_put_piggyback is used in default
#	CACHE_INJECT:	set cache injection flag in utofu_put
#	VCQIDTAB:	copy all node's vcqid to a table from the utf_info structure
CFLAGS += -DPEEK_MEMORY
CFLAGS += -DREMOTE_PUT
CFLAGS += -DCACHE_INJECT
#CFLAGS += -DVCQIDTAB

$(PROGS): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
install:
	[ -d $(INSTDIR) ] || mkdir $(INSTDIR)
	$(INSTALL) $(PROGS) $(INSTDIR)/
clean:
	rm -f $(PROGS) *.o *~

depend:
	makedepend -I$(CCINCLUDE) $(CFLAGS) $(SRCS)
# DO NOT DELETE

test_utofu.o: /usr/include/string.h /usr/include/features.h
test_utofu.o: /usr/include/stdc-predef.h /usr/include/sys/cdefs.h
test_utofu.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
test_utofu.o: /usr/include/xlocale.h /usr/include/stdio.h
test_utofu.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
test_utofu.o: /usr/include/libio.h /usr/include/_G_config.h
test_utofu.o: /usr/include/wchar.h /usr/include/bits/stdio_lim.h
test_utofu.o: /usr/include/bits/sys_errlist.h /usr/include/unistd.h
test_utofu.o: /usr/include/bits/posix_opt.h /usr/include/bits/environments.h
test_utofu.o: /usr/include/bits/confname.h /usr/include/getopt.h
test_utofu.o: /usr/include/stdlib.h /usr/include/bits/waitflags.h
test_utofu.o: /usr/include/bits/waitstatus.h /usr/include/endian.h
test_utofu.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h
test_utofu.o: /usr/include/bits/byteswap-16.h /usr/include/sys/types.h
test_utofu.o: /usr/include/time.h /usr/include/sys/select.h
test_utofu.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
test_utofu.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
test_utofu.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
test_utofu.o: /usr/include/bits/stdlib-float.h /usr/include/stdint.h
test_utofu.o: /usr/include/bits/wchar.h utf_externs.h testlib.h utf_timer.h
test_utofu.o: utf_tsc.h utf_conf.h /usr/include/assert.h utf_tofu.h
test_utofu.o: utf_errmacros.h
testlib.o: /usr/include/stdio.h /usr/include/features.h
testlib.o: /usr/include/stdc-predef.h /usr/include/sys/cdefs.h
testlib.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
testlib.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
testlib.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
testlib.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
testlib.o: /usr/include/unistd.h /usr/include/bits/posix_opt.h
testlib.o: /usr/include/bits/environments.h /usr/include/bits/confname.h
testlib.o: /usr/include/getopt.h /usr/include/stdlib.h
testlib.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
testlib.o: /usr/include/endian.h /usr/include/bits/endian.h
testlib.o: /usr/include/bits/byteswap.h /usr/include/bits/byteswap-16.h
testlib.o: /usr/include/sys/types.h /usr/include/time.h
testlib.o: /usr/include/sys/select.h /usr/include/bits/select.h
testlib.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
testlib.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
testlib.o: /usr/include/alloca.h /usr/include/bits/stdlib-float.h testlib.h
testlib.o: utf_externs.h utf_timer.h utf_tsc.h /usr/include/stdint.h
testlib.o: /usr/include/bits/wchar.h
utf_init.o: /usr/include/getopt.h utf_conf.h /usr/include/assert.h
utf_init.o: /usr/include/features.h /usr/include/stdc-predef.h
utf_init.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
utf_init.o: /usr/include/gnu/stubs.h /usr/include/sys/types.h
utf_init.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
utf_init.o: /usr/include/time.h /usr/include/endian.h
utf_init.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h
utf_init.o: /usr/include/bits/byteswap-16.h /usr/include/sys/select.h
utf_init.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
utf_init.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
utf_init.o: /usr/include/bits/pthreadtypes.h /usr/include/stdio.h
utf_init.o: /usr/include/libio.h /usr/include/_G_config.h
utf_init.o: /usr/include/wchar.h /usr/include/bits/stdio_lim.h
utf_init.o: /usr/include/bits/sys_errlist.h /usr/include/stdlib.h
utf_init.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
utf_init.o: /usr/include/alloca.h /usr/include/bits/stdlib-float.h
utf_init.o: /usr/include/unistd.h /usr/include/bits/posix_opt.h
utf_init.o: /usr/include/bits/environments.h /usr/include/bits/confname.h
utf_init.o: utf_externs.h utf_errmacros.h utf_tofu.h utf_timer.h utf_tsc.h
utf_init.o: /usr/include/stdint.h /usr/include/bits/wchar.h
utf_procmap.o: /usr/include/stdlib.h /usr/include/features.h
utf_procmap.o: /usr/include/stdc-predef.h /usr/include/sys/cdefs.h
utf_procmap.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
utf_procmap.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
utf_procmap.o: /usr/include/endian.h /usr/include/bits/endian.h
utf_procmap.o: /usr/include/bits/byteswap.h /usr/include/bits/types.h
utf_procmap.o: /usr/include/bits/typesizes.h /usr/include/bits/byteswap-16.h
utf_procmap.o: /usr/include/sys/types.h /usr/include/time.h
utf_procmap.o: /usr/include/sys/select.h /usr/include/bits/select.h
utf_procmap.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
utf_procmap.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
utf_procmap.o: /usr/include/alloca.h /usr/include/bits/stdlib-float.h
utf_procmap.o: /usr/include/stdio.h /usr/include/libio.h
utf_procmap.o: /usr/include/_G_config.h /usr/include/wchar.h
utf_procmap.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
utf_procmap.o: /usr/include/dlfcn.h /usr/include/bits/dlfcn.h
utf_procmap.o: /usr/include/sys/ipc.h /usr/include/bits/ipctypes.h
utf_procmap.o: /usr/include/bits/ipc.h /usr/include/sys/shm.h
utf_procmap.o: /usr/include/bits/shm.h /usr/include/limits.h
utf_procmap.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
utf_procmap.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
utf_procmap.o: utf_conf.h /usr/include/assert.h /usr/include/unistd.h
utf_procmap.o: /usr/include/bits/posix_opt.h /usr/include/bits/environments.h
utf_procmap.o: /usr/include/bits/confname.h /usr/include/getopt.h
utf_procmap.o: utf_externs.h utf_errmacros.h utf_tofu.h
