## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4 --install

SUBDIRS = src doc

MAINTAINERCLEANFILES = Makefile.in aclocal.m4 config.guess \
                       config.h.in config.sub configure install-sh \
		       ltconfig ltmain.sh missing mkinstalldirs \
		       stamp-h.in epeg_docs.tar epeg_docs.tar.gz epeg.c \
		       configure.scan

BUILT_SOURCES = epeg.c

install-data-local:
        @$(NORMAL_INSTALL)
	if test -d $(srcdir)/data; then \
	  $(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/data; \
	  for d in $(srcdir)/data/*; do \
	    cp -pRv $${d} $(DESTDIR)$(pkgdatadir)/data; \
          done \
	fi

dist-hook: stamp-gendoc Doxyfile
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

just-gendoc: stamp-gendoc Doxyfile
	./gendoc 2>&1
.PHONY: just-gendoc

epeg.c: stamp-gendoc epeg.c.in
	if test "x$(srcdir)" != "x."; then \
	  if test ! -e epeg.c.in -a -e $(srcdir)/epeg.c.in; then \
	    cp -v $(srcdir)/epeg.c.in .; \
	  else \
	    echo "not copying epeg.c.in here"; \
	  fi; \
	else \
	  echo "no special treatment for epeg.c.in in non-vpath builds"; \
	fi
	GENDOC_SHOULD_EXIT_EARLY=1 ./gendoc 2>&1

stamp-gendoc: $(srcdir)/gendoc $(srcdir)/Doxyfile
	@rm -f $@
	if test "x$(srcdir)" != "x."; then \
	  if test ! -e ./gendoc; then \
	    if test -x $(srcdir)/gendoc; then \
	      cp -v $(srcdir)/gendoc .; \
	    elif test -f $(srcdir)/gendoc; then \
	      cp -v $(srcdir)/gendoc . && chmod +x ./gendoc; \
	    else \
	      echo "cannot copy gendoc from '$(srcdir)' to here" >&2; \
	      false; \
	    fi; \
	  else \
	    echo "./gendoc already present"; \
	  fi; \
	  if test ! -e ./Doxyfile; then \
	    if test -f $(srcdir)/Doxyfile; then \
	      cp -v $(srcdir)/Doxyfile .; \
	    else \
	      echo "cannot copy Doxyfile from '$(srcdir)' to here" >&2; \
	      false; \
	    fi; \
	  else \
	    echo "./Doxyfile already present"; \
	  fi; \
	else \
	  echo "nothing to do for '$@' in non-vpath builds"; \
	fi
	touch $@

bin_SCRIPTS = epeg-config

noinst_SCRIPTS = gendoc

check_SCRIPTS = test_epeg-config

check_PROGRAMS = epeg_test

epeg_test_SOURCES = epeg_test.c
epeg_test_DEPENDENCIES = epeg-config
epeg_test_CFLAGS = `epeg-config --cflags`
epeg_test_LDFLAGS = `epeg-config --libs`

TESTS = test_epeg-config

EXTRA_DIST = README AUTHORS COPYING COPYING-PLAIN epeg.spec epeg.c.in gendoc Doxyfile epeg_docs.tar.gz test_epeg-config doc/html/html.helpindex

DISTCLEANFILES = *~ .DS_Store doc/.DS_Store m4/.DS_Store autoscan.log \
  build-aux/*~ stamp-gendoc @MAINTAINER_MODE_TRUE@ epeg.c

## (MAINTAINERCLEANFILES is already set above)

nodist_noinst_DATA = @MAINTAINER_MODE_TRUE@ configure.scan

configure.scan:
	autoscan 2>&1 || touch $@

distclean-local:
	rm -fv @PACKAGE_TARNAME@-@PACKAGE_VERSION@.tar.gz
	rm -rfv autom4te.cache || rmdir autom4te.cache
	(test -w @PACKAGE_TARNAME@-@PACKAGE_VERSION@ && (rm -rfv @PACKAGE_TARNAME@-@PACKAGE_VERSION@ || rmdir @PACKAGE_TARNAME@-@PACKAGE_VERSION@)) || stat @PACKAGE_TARNAME@-*.*.* || echo "it can be difficult to distclean the results of distcheck"
	find . -name .DS_Store -delete 2>&1
