## automake Makefile for doc subdir for epeg

SUBDIRS = . latex texinfo

DISTCLEANFILES = .DS_Store man/.DS_Store

# A list of all the files in the current directory which can be regenerated
MAINTAINERCLEANFILES = Makefile.in

## Only some of the html files in the 'html/' subdir are generated; just
## do the ones that `git` detects as changing on re-runs of gendoc:
MAINTAINERCLEANFILES += html/epeg_8c.html html/globals.html html/globals_func.html html/index.html html/html.helpindex

EXTRA_DIST = latex_backup_Makefile.am ../gendoc ../Doxyfile

nodist_html_DATA = html/html.helpindex

## The Doxyfile refers to these:
dist_doc_DATA = epeg.css foot.html head.html img/epeg_small.png

all: latex/Makefile.am html/html.helpindex

# latex used to be doxygenated; doxygenated latex now gets put in doxylatex
# though, so the latex subdir should now be safe to manually edit. Just in
# case it goes back to getting deleted, though, have this rule:
latex/Makefile.am: latex_backup_Makefile.am
	cp -v $< $@

html/html.helpindex: ../gendoc ../Makefile ../Doxyfile
	if test -x ../gendoc; then \
	  ($(MAKE) -C .. just-gendoc 2>&1) || ($(MAKE) ../gendoc); \
	else \
	  ($(MAKE) ../gendoc) || (echo "not generating docs"); \
	fi
	if test -d html; then \
	  (hiutil -C html -f $@ -ag 2>&1 || touch $@); \
	else \
	  ($(MKDIR_P) html && touch $@) || (echo "no html to index"); \
	fi

../gendoc: ../Makefile $(dist_doc_DATA)
	if test "x$(srcdir)" != "x."; then \
	  for need_doc in $(dist_doc_DATA); do \
	    if test ! -e $${need_doc} -a -e $(srcdir)/$${need_doc}; then \
	      cp -v $(srcdir)/$${need_doc} .; \
	    else \
	      echo "not copying $${need_doc} here"; \
	    fi; \
	  done; \
	else \
	  echo "no special treatment for docs in non-vpath builds"; \
	fi
	$(MAKE) -C .. stamp-gendoc
	touch $@

# These are the same dirs that the gendoc script removes before running
# doxygen:
maintainer-clean-local:
	rm -rfv ./def || rmdir ./def
	rm -rfv ./docbook || rmdir ./docbook
	rm -rfv ./doxylatex || rmdir ./doxylatex
	rm -rfv ./man || rmdir ./man
	rm -rfv ./perlmod || rmdir ./perlmod
	rm -rfv ./rtf || rmdir ./rtf
	rm -rfv ./xml || rmdir ./xml
