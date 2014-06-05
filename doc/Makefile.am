## automake Makefile for doc subdir for epeg

SUBDIRS = . latex texinfo

DISTCLEANFILES = .DS_Store man/.DS_Store

EXTRA_DIST = latex_backup_Makefile.am

all: latex/Makefile.am html/html.helpindex

latex/Makefile.am: latex_backup_Makefile.am
	cp -v $< $@

html/html.helpindex:
	if test -d html; then (hiutil -C html -f $@ -ag 2>&1 || touch $@); else echo "no html to index"; fi
