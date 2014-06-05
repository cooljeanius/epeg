## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4 --install

SUBDIRS = src doc

MAINTAINERCLEANFILES = Makefile.in aclocal.m4 config.guess \
                       config.h.in config.sub configure install-sh \
		       ltconfig ltmain.sh missing mkinstalldirs \
		       stamp-h.in epeg_docs.tar epeg_docs.tar.gz epeg.c \
		       configure.scan

install-data-local:
        @$(NORMAL_INSTALL)
	if test -d $(srcdir)/data; then \
	  $(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/data; \
	  for d in $(srcdir)/data/*; do \
	    cp -pRv $${d} $(DESTDIR)$(pkgdatadir)/data; \
          done \
	fi

dist-hook: ./gendoc
	./gendoc; \
	tar cvf epeg_docs.tar doc; \
	rm -f epeg_docs.tar.gz; \
	GZIP=$(GZIP_ENV) gzip -9 epeg_docs.tar; \
	if test -d data; then \
	  mkdir $(distdir)/data; \
	  for file in data/*; do \
	    cp -pRv $${file} $(distdir)/data; \
	  done \
	fi
							  

bin_SCRIPTS = epeg-config

check_SCRIPTS = test_epeg-config

TESTS = test_epeg-config

EXTRA_DIST = README AUTHORS COPYING COPYING-PLAIN epeg.spec epeg.c.in gendoc Doxyfile epeg_docs.tar.gz

DISTCLEANFILES = *~ .DS_Store doc/.DS_Store autoscan.log

## (MAINTAINERCLEANFILES is already set above)

configure.scan:
	autoscan 2>&1 || touch $@

distclean-local:
	rm -fv @PACKAGE_TARNAME@-@PACKAGE_VERSION@.tar.gz
	rm -rfv autom4te.cache || rmdir autom4te.cache
	(test -w @PACKAGE_TARNAME@-@PACKAGE_VERSION@ && (rm -rf @PACKAGE_TARNAME@-@PACKAGE_VERSION@ || rmdir @PACKAGE_TARNAME@-@PACKAGE_VERSION@)) || stat @PACKAGE_TARNAME@-*.*.* || echo "it can be difficult to distclean the results of distcheck"
