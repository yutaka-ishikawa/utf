INSTDIR = $(HOME)/utf/
INSTALL=install

ARCH=$(shell arch)

install:
	(cd src/pmix-tsim/; make; make install)
	[ -d $(INSTDIR) ] || mkdir $(INSTDIR)
	[ -d $(INSTDIR)/include ] || mkdir $(INSTDIR)/include
	[ -d $(INSTDIR)/bin ] || mkdir $(INSTDIR)/bin
	[ -d $(INSTDIR)/lib ] || mkdir $(INSTDIR)/lib
	$(INSTALL) -m 644 include/pmix.h $(INSTDIR)/include
	$(INSTALL) -m 644 include/jtofu.h $(INSTDIR)/include
	$(INSTALL) -m 644 include/pmix_fjext.h $(INSTDIR)/include
	$(INSTALL) -m 644 include/utofu.h $(INSTDIR)/include
	$(INSTALL) -m 644 include/utofu_profile.h $(INSTDIR)/include
	$(INSTALL) -m 0644 include/pmix_common.h $(INSTDIR)/include/
	$(INSTALL) -m 0644 include/pmix_extend.h $(INSTDIR)/include/
	$(INSTALL) -m 0644 include/pmix_rename.h $(INSTDIR)/include/
	$(INSTALL) -m 0644 include/pmix_version.h $(INSTDIR)/include/
	$(INSTALL) $(ARCH)/lib/libpmix.so $(INSTDIR)/lib
	$(INSTALL) $(ARCH)/bin/tsim_server $(INSTDIR)/bin
	$(INSTALL) $(ARCH)/bin/showrankmap $(INSTDIR)/bin
	$(INSTALL) $(ARCH)/bin/tcdumpconv $(INSTDIR)/bin
	$(INSTALL) $(ARCH)/lib/libtofucom.so $(INSTDIR)/lib
	$(INSTALL) $(ARCH)/lib/libtofutop.so $(INSTDIR)/lib
