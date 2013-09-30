dnl#                                               -*- Autoconf -*-
dnl# Process this file with autoconf to produce a configure script.

# get rid of that stupid cache mechanism
if test -e config.cache; then
	rm -f config.cache
fi

AC_PREREQ([2.69])
AC_INIT([epeg],[0.9.0],[raster@rasterman.com])
AC_CONFIG_SRCDIR([configure.in])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_TARGET
AC_OBJEXT
AC_EXEEXT
AC_USE_SYSTEM_EXTENSIONS
AM_INIT_AUTOMAKE([foreign -Wall])
AM_SANITY_CHECK
AM_MAINTAINER_MODE
AC_CONFIG_HEADERS([config.h])

AC_SEARCH_LIBS([strerror],[cposix]) dnl# used to be AC\_ISC\_POSIX
if test "x$CC" = "x"; then
	test -z "$CC"
	AC_PROG_CC
else
	test ! -z "$CC" && export CC
	AC_SUBST([CC])
	AC_PROG_GCC_TRADITIONAL
fi
AC_PROG_CC_C_O
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LN_S
LT_INIT([shared])
if test "x$RANLIB" = "x"; then
	test -z "$RANLIB"
	AC_PROG_RANLIB
else
	test ! -z "$RANLIB" && export RANLIB
	AC_SUBST([RANLIB])
fi
AC_PATH_PROG([CJPEG],[cjpeg])
AC_PATH_PROG([DJPEG],[djpeg])
AC_PATH_PROG([JPEGTRAN],[jpegtran])
AC_PATH_PROG([RDJPGCOM],[rdjpgcom])
AC_PATH_PROG([WRJPGCOM],[wrjpgcom])

# Checks for libraries.
LT_LIB_M
# FIXME: Replace `main' with a function in `-ljpeg':
AC_CHECK_LIB([jpeg],[main])
# AX_* macros require the autoconf-archive package
AX_CHECK_GD

# Checks for header files.
AC_DEFUN([AC_REQUIRE_HEADER_STDC],[
  AC_REQUIRE([AC_HEADER_STDC])
])
AC_REQUIRE_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h limits.h stdio.h])
AC_CHECK_HEADERS([jconfig.h jerror.h jmorecfg.h jpeglib.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_ENABLE_SHARED([])
AC_C_BIGENDIAN
AC_C_CONST
AC_C_PROTOTYPES
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([strchr strdup strlen])
AC_CHECK_FUNCS([fmemopen])
AC_CHECK_FUNCS([open_memstream])

if test "x${exec_prefix}" = "xNONE"; then
  if test "x${prefix}" = "xNONE"; then
    bindir="${ac_default_prefix}/bin";
  else
    bindir="${prefix}/bin";
  fi
else
  if test "x${prefix}" = "xNONE"; then
    bindir="${ac_default_prefix}/bin";
  else
    bindir="${prefix}/bin";
  fi
fi

if test "x${exec_prefix}" = "xNONE"; then
  if test "x${prefix}" = "xNONE"; then
    libdir="${ac_default_prefix}/lib";
  else
    libdir="${prefix}/lib";
  fi
else
  if test "x${prefix}" = "xNONE"; then
    libdir="${ac_default_prefix}/lib";
  else
    libdir="${prefix}/lib";
  fi
fi

dnl# Set PACKAGE_BIN_DIR in config.h.
if test "x${bindir}" = 'xNONE'; then
  if test "x${prefix}" = "xNONE"; then
    AC_DEFINE_UNQUOTED([PACKAGE_BIN_DIR],["${ac_default_prefix}/bin"],[Installation directory for user executables])
  else
    AC_DEFINE_UNQUOTED([PACKAGE_BIN_DIR],["${prefix}/bin"],[Installation directory for user executables])
  fi
else
  AC_DEFINE_UNQUOTED([PACKAGE_BIN_DIR],["${bindir}"],[Installation directory for user executables])
fi

dnl# Set PACKAGE_LIB_DIR in config.h.
if test "x${libdir}" = 'xNONE'; then
  if test "x${prefix}" = "xNONE"; then
    AC_DEFINE_UNQUOTED([PACKAGE_LIB_DIR],["${ac_default_prefix}/lib"],[Installation directory for libraries])
  else
    AC_DEFINE_UNQUOTED([PACKAGE_LIB_DIR],["${prefix}/lib"],[Installation directory for libraries])
  fi
else
  AC_DEFINE_UNQUOTED([PACKAGE_LIB_DIR],["${libdir}"],[Installation directory for libraries])
fi

dnl# Set PACKAGE_SOURCE_DIR in config.h.
packagesrcdir=`cd $srcdir && pwd`
AC_DEFINE_UNQUOTED([PACKAGE_SOURCE_DIR],["${packagesrcdir}"],[Source code directory])

my_includes=""
my_libs="-ljpeg"
AC_SUBST([my_includes])
AC_SUBST([my_libs])

AC_CONFIG_FILES([
Makefile
doc/latex/Makefile
src/Makefile
src/lib/Makefile
src/bin/Makefile
epeg-config
])
AC_CONFIG_COMMANDS([default],[
chmod +x epeg-config
touch epeg_docs.tar.gz
],[])
AC_OUTPUT
