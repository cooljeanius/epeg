## automake Makefile for handmade latex rules
## backed up one directory above where it is actually used, just in case
## the gendoc script deletes it

# A list of all the files in the current directory which can be regenerated
MAINTAINERCLEANFILES = Makefile.in

all: @MAINT@ refman.dvi Makefile Makefile.am
.PHONY: all

ps-local: refman.ps
.PHONY: ps-local

pdf-local: refman.pdf
.PHONY: pdf-local

ps_2on1: refman_2on1.ps
.PHONY: ps_2on1

pdf_2on1: refman_2on1.pdf
.PHONY: pdf_2on1

refman.ps: refman.dvi
	dvips -o refman.ps refman.dvi

refman.pdf: refman.ps
	ps2pdf refman.ps refman.pdf

refman.dvi: refman.tex doxygen.sty @MAINT@ clean-local
	if [ "x$(srcdir)" != "x." ]; then \
	  cp -v $(srcdir)/refman.tex .; \
	  cp -v $(srcdir)/doxygen.sty .; \
	fi
	@echo "Running latex..."
	latex refman.tex || latex $(srcdir)/refman.tex
	@echo "Running makeindex..."
	makeindex refman.idx
	@echo "Rerunning latex...."
	latex refman.tex || latex $(srcdir)/refman.tex
	latex_count=5 ; \
	while $(EGREP) -s 'Rerun (LaTeX|to get cross-references right)' refman.log && [ $${latex_count} -gt 0 ];\
	    do \
	      echo "Rerunning latex....";\
	      latex refman.tex || latex $(srcdir)/refman.tex;\
	      latex_count=`expr $${latex_count} - 1` ;\
	    done

refman_2on1.ps: refman.ps
	psnup -2 refman.ps >refman_2on1.ps

refman_2on1.pdf: refman_2on1.ps
	ps2pdf refman_2on1.ps refman_2on1.pdf

clean-local:
	rm -fv *.ps *.dvi *.aux *.toc *.idx *.ind *.ilg *.log *.out *.brf *.blg *.bbl refman.pdf
.PHONY: clean-local

Makefile.am: ../Makefile ../latex_backup_Makefile.am
	$(MAKE) -C .. latex/Makefile.am || cp -v ../latex_backup_Makefile.am $@
	if test -e $@; then touch $@; fi

EXTRA_DIST = doxygen.sty epeg_8c.tex files.tex index.tex pages.tex refman.tex todo.tex ../latex_backup_Makefile.am
