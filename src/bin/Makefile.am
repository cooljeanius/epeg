## Process this file with automake to produce Makefile.in

AM_LDFLAGS = \
-L/usr/local/lib \
-L@prefix@/lib

AM_CPPFLAGS = \
-I../lib \
-I/usr/local/include \
-I$(top_srcdir)/src/lib \
@my_includes@ \
-L@prefix@/lib

bin_PROGRAMS = epeg

epeg_SOURCES = \
epeg_main.c \
epeg_main.h

epeg_LDADD = \
$(top_builddir)/src/lib/libepeg.la

epeg_DEPENDENCIES = $(top_builddir)/src/lib/libepeg.la
