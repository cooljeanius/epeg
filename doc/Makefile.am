SUBDIRS = . latex texinfo

all: latex/Makefile.am html/html.helpindex

latex/Makefile.am: latex_backup_Makefile.am
	cp $< $@

html/html.helpindex:
	hiutil -C html -f html/html.helpindex -ag
