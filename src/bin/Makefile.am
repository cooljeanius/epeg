## Process this file with automake to produce Makefile.in for src/bin

DISTCLEANFILES = .DS_Store

# A list of all the files in the current directory which can be regenerated
MAINTAINERCLEANFILES = Makefile.in

AM_LDFLAGS = \
	-L@prefix@/lib

MY_AM_BAD_LDFLAGS_HARDCODED = -L/usr/local/lib

AM_CPPFLAGS = \
	-I../lib \
	-I@prefix@/include \
	-I$(top_srcdir)/src/lib \
	@my_includes@

MY_AM_BAD_CPPFLAGS_HARDCODED = -I/usr/local/include

bin_PROGRAMS = epeg

epeg_SOURCES = \
	epeg_main.c \
	epeg_main.h

epeg_LDADD = \
	$(top_builddir)/src/lib/libepeg.la

epeg_DEPENDENCIES = $(top_builddir)/src/lib/libepeg.la

EXTRA_DIST = test_epeg

check_SCRIPTS = test_epeg

TESTS = test_epeg
