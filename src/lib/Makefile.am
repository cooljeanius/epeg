## Process this file with automake to produce Makefile.in for src/lib

AUTOMAKE_OPTIONS     = 1.4 gnits

DISTCLEANFILES       = .DS_Store *~

# A list of all the files in the current directory which can be regenerated
MAINTAINERCLEANFILES = Makefile.in

AM_CFLAGS	     = -Wall -Wimplicit -Wno-unused
AM_LDFLAGS	     = -L@prefix@/lib
MY_AM_BAD_LDFLAGS_HARDCODED = -L/usr/local/lib
AM_CPPFLAGS	     = -I$(includedir) \
		       @my_includes@
MY_AM_BAD_CPPFLAGS_HARDCODED = -I/usr/local/include

lib_LTLIBRARIES      = libepeg.la
include_HEADERS      = Epeg.h
libepeg_la_SOURCES   = \
	epeg_main.c \
	epeg_memfile.c \
	epeg_private.h

libepeg_la_LIBADD       = $(LDFLAGS) @my_libs@
libepeg_la_DEPENDENCIES = $(top_builddir)/config.h
libepeg_la_LDFLAGS      = -version-info 9:0:9
