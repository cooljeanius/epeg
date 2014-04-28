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
	    cp -pR $$d $(DESTDIR)$(pkgdatadir)/data; \
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
	    cp -pR $$file $(distdir)/data; \
	  done \
	fi
							  

bin_SCRIPTS = epeg-config

EXTRA_DIST = README AUTHORS COPYING COPYING-PLAIN epeg.spec epeg.c.in gendoc Doxyfile epeg_docs.tar.gz

DISTCLEANFILES = *~ .DS_Store autoscan.log

configure.scan:
	autoscan || touch configure.scan

distclean-local:
	rm -f @PACKAGE_TARNAME@-@PACKAGE_VERSION@.tar.gz
	rm -rf autom4te.cache || rmdir autom4te.cache
