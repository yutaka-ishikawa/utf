UTF_ARCH=x86
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

test_utofu.o: /usr/include/string.h /usr/include/stdio.h
test_utofu.o: /usr/include/unistd.h /usr/include/features.h
test_utofu.o: /usr/include/stdc-predef.h /usr/include/stdlib.h
test_utofu.o: /usr/include/alloca.h /usr/include/stdint.h
test_utofu.o: /home/ishikawa/utf/include/utofu.h testlib.h utf_timer.h
test_utofu.o: utf_tsc.h utf_conf.h /usr/include/assert.h utf_tofu.h
test_utofu.o: /home/ishikawa/utf/include/pmix.h
test_utofu.o: /home/ishikawa/utf/include/pmix_common.h /usr/include/time.h
test_utofu.o: /home/ishikawa/utf/include/pmix_rename.h
test_utofu.o: /home/ishikawa/utf/include/pmix_version.h
test_utofu.o: /home/ishikawa/utf/include/pmix_extend.h
test_utofu.o: /home/ishikawa/utf/include/jtofu.h utf_errmacros.h
testlib.o: /usr/include/stdio.h /usr/include/unistd.h /usr/include/features.h
testlib.o: /usr/include/stdc-predef.h /usr/include/getopt.h
testlib.o: /usr/include/stdlib.h /usr/include/alloca.h testlib.h utf_timer.h
testlib.o: utf_tsc.h /usr/include/stdint.h
utf_init.o: /usr/include/getopt.h /usr/include/features.h
utf_init.o: /usr/include/stdc-predef.h /home/ishikawa/utf/include/pmix.h
utf_init.o: /home/ishikawa/utf/include/pmix_common.h /usr/include/stdlib.h
utf_init.o: /usr/include/alloca.h /usr/include/stdint.h /usr/include/string.h
utf_init.o: /usr/include/time.h /usr/include/unistd.h
utf_init.o: /home/ishikawa/utf/include/pmix_rename.h
utf_init.o: /home/ishikawa/utf/include/pmix_version.h
utf_init.o: /home/ishikawa/utf/include/pmix_extend.h
utf_init.o: /home/ishikawa/utf/include/pmix_fjext.h
utf_init.o: /home/ishikawa/utf/include/utofu.h
utf_init.o: /home/ishikawa/utf/include/jtofu.h utf_conf.h
utf_init.o: /usr/include/assert.h /usr/include/stdio.h utf_externs.h
utf_init.o: utf_errmacros.h utf_tofu.h utf_timer.h utf_tsc.h
utf_procmap.o: /usr/include/stdlib.h /usr/include/alloca.h
utf_procmap.o: /usr/include/features.h /usr/include/stdc-predef.h
utf_procmap.o: /usr/include/stdio.h /usr/include/dlfcn.h
utf_procmap.o: /home/ishikawa/utf/include/pmix.h
utf_procmap.o: /home/ishikawa/utf/include/pmix_common.h /usr/include/stdint.h
utf_procmap.o: /usr/include/string.h /usr/include/time.h
utf_procmap.o: /usr/include/unistd.h /home/ishikawa/utf/include/pmix_rename.h
utf_procmap.o: /home/ishikawa/utf/include/pmix_version.h
utf_procmap.o: /home/ishikawa/utf/include/pmix_extend.h
utf_procmap.o: /home/ishikawa/utf/include/utofu.h
utf_procmap.o: /home/ishikawa/utf/include/jtofu.h
utf_procmap.o: /home/ishikawa/utf/include/pmix_fjext.h /usr/include/limits.h
utf_procmap.o: utf_conf.h /usr/include/assert.h utf_externs.h utf_errmacros.h
utf_procmap.o: utf_tofu.h
