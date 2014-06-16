dnl#                                               -*- Autoconf -*-
dnl# Process this file with autoconf to produce a configure script.
dnl#
dnl# (style note: lines with autoconf macros on them are ended with 'dnl'
dnl# when doing so removes an empty line in the generated configure script,
dnl# without also changing the indentation of the next non-empty line)
dnl#

# get rid of that stupid cache mechanism:
if test -e config.cache; then
  rm -f config.cache 2>&1
fi

# misc. initializion things:
AC_PREREQ([2.69])dnl
AC_INIT([epeg],[0.9.0],[raster@rasterman.com])dnl
AC_CONFIG_SRCDIR([epeg.c.in])dnl
AC_CONFIG_AUX_DIR([build-aux])dnl
AC_REQUIRE_AUX_FILE([mkinstalldirs])dnl
AC_CONFIG_MACRO_DIR([m4])dnl
AC_CANONICAL_TARGET
AC_OBJEXT
AC_EXEEXT
AC_USE_SYSTEM_EXTENSIONS([])dnl
AM_INIT_AUTOMAKE([1.4 gnits])
AM_SANITY_CHECK([])dnl
AM_MAINTAINER_MODE([])dnl
AC_CONFIG_HEADERS([config.h])dnl
AC_SYS_LARGEFILE([])dnl
AC_SYS_LONG_FILE_NAMES([])dnl
AC_SEARCH_LIBS([strerror],[cposix]) dnl# used to be AC\_ISC\_POSIX

# Checks for programs.
if test "x${CC}" = "x"; then
  test -z "${CC}"
  AC_PROG_CC([])dnl
else
  test ! -z "${CC}" && export CC
  AC_SUBST([CC])
  AC_PROG_GCC_TRADITIONAL([])dnl
fi
AC_PROG_CC_C_O([])dnl
AC_PROG_CPP([])dnl
AC_PROG_CXX([])dnl
AC_PROG_INSTALL([])dnl
AC_PROG_LN_S
AC_PROG_MKDIR_P([])dnl
AC_DEFUN([AM_LT_PREREQS],[
  AC_REQUIRE([AC_PROG_AWK])dnl
  AC_REQUIRE([AC_PROG_MAKE_SET])dnl
])dnl
AM_LT_PREREQS([])dnl
LT_INIT([shared win32-dll dlopen])dnl
AC_DEFUN([AC_REQUIRE_RANLIB],[
  AC_REQUIRE([AC_PROG_RANLIB])dnl
])dnl
if test "x${RANLIB}" = "x"; then
  test -z "${RANLIB}" && test -z "${ac_cv_prog_ac_ct_RANLIB}"
  AC_REQUIRE_RANLIB([])
else
  test ! -z "${RANLIB}" && export RANLIB
  AC_SUBST([RANLIB])
fi
AC_DEFUN([AC_REQUIRE_GREPS],[
  AC_REQUIRE([AC_PROG_GREP])dnl
  AC_REQUIRE([AC_PROG_EGREP])dnl
  AC_REQUIRE([AC_PROG_FGREP])dnl
  AC_SUBST([EGREP])dnl
])dnl
AC_REQUIRE_GREPS([])
AC_PATH_PROG([CJPEG],[cjpeg])dnl
AC_PATH_PROG([DJPEG],[djpeg])dnl
AC_PATH_PROG([JPEGTRAN],[jpegtran])dnl
AC_PATH_PROG([RDJPGCOM],[rdjpgcom])dnl
AC_PATH_PROG([WRJPGCOM],[wrjpgcom])dnl

# documentation tools:
AM_MISSING_PROG([DVIPS_PROG],[dvips])dnl
AM_MISSING_PROG([HIUTIL],[hiutil])dnl
AM_MISSING_PROG([LATEX],[latex])dnl
AM_MISSING_PROG([MAKEINDEX],[makeindex])dnl
AM_MISSING_PROG([PS2PDF],[ps2pdf])dnl
AM_MISSING_PROG([PSNUP],[psnup])dnl

# Checks for libraries.
AC_CHECK_LIB([c],[fprintf])dnl
LT_LIB_M([])dnl
AC_CHECK_LIB([jpeg],[jpeg_destroy_compress],[],[
  AC_CHECK_LIB([jpeg],[main])dnl
])dnl
dnl# AX_* macros require the autoconf-archive package:
AX_CHECK_GD([])dnl

# Checks for header files.
AC_DEFUN([AC_REQUIRE_HEADER_STDC],[
  AC_REQUIRE([AC_HEADER_STDC])dnl
])dnl
AC_REQUIRE_HEADER_STDC([])dnl
AC_HEADER_TIME([])dnl
AC_HEADER_STAT([])dnl
AC_CHECK_HEADERS([fcntl.h limits.h setjmp.h stdio.h time.h])dnl

# jpeg-specific headers:
AC_CHECK_HEADERS([jconfig.h jerror.h jmorecfg.h jpeglib.h])dnl

# Checks for typedefs, structures, and compiler characteristics.
AC_ENABLE_SHARED([])dnl
AC_C_BIGENDIAN([])dnl
AC_C_CONST([])dnl
AC_C_PROTOTYPES([])dnl
AC_TYPE_SIZE_T([])dnl
AC_TYPE_UINTMAX_T([])dnl

dnl# FIXME: come up with a macro to simplify checks like this: 
AC_CHECK_TYPES([jmp_buf],[
  dnl# for some reason the sizeof check has an extra, unused parameter...
  AC_CHECK_SIZEOF([jmp_buf],[],[
#ifdef HAVE_SETJMP_H
# include <setjmp.h>
#else
# if defined(__GNUC__) && !defined(__STRICT_ANSI__)
#  warning "This conftest for jmp_buf expects <setjmp.h> to be included."
# endif /* __GNUC__ && !__STRICT_ANSI__ */
#endif /* HAVE_SETJMP_H */
  ])dnl
  dnl# unlike the size check, the alignment check does NOT have
  dnl# the extra unused parameter though:
  AC_CHECK_ALIGNOF([jmp_buf],[
#ifdef HAVE_SETJMP_H
# include <setjmp.h>
#else
# if defined(__GNUC__) && !defined(__STRICT_ANSI__)
#  warning "This conftest for jmp_buf expects <setjmp.h> to be included."
# endif /* __GNUC__ && !__STRICT_ANSI__ */
#endif /* HAVE_SETJMP_H */
  ])dnl
],[],[
#ifdef HAVE_SETJMP_H
# include <setjmp.h>
#else
# if defined(__GNUC__) && !defined(__STRICT_ANSI__)
#  warning "This conftest for jmp_buf expects <setjmp.h> to be included."
# endif /* __GNUC__ && !__STRICT_ANSI__ */
#endif /* HAVE_SETJMP_H */
])dnl
AC_CHECK_TYPES([FILE *],[
  dnl# unlinke the setjmp tests, the checks for size and alignment of
  dnl# file pointers do NOT need their default includes modified:
  AC_CHECK_SIZEOF([FILE *])dnl
  AC_CHECK_ALIGNOF([FILE *])dnl
],[],[
#ifdef HAVE_STDIO_H
# include <stdio.h>
#else
# if defined(__GNUC__) && !defined(__STRICT_ANSI__)
#  warning "This conftest for file pointers wants <stdio.h> to be included"
# endif /* __GNUC__ && !__STRICT_ANSI__ */
#endif /* HAVE_STDIO_H */
])dnl

# jpeg-specific types:
AC_CHECK_TYPES([const JOCTET *, j_common_ptr, J_COLOR_SPACE],[],[],[
#include <stdio.h>
#ifdef HAVE_JPEGLIB_H
# include <jpeglib.h>
#else
# if defined(__GNUC__) && !defined(__STRICT_ANSI__)
#  warning "This conftest for a jpeg type wants to include <jpeglib.h>"
# endif /* __GNUC__ && !__STRICT_ANSI__ */
#endif /* HAVE_JPEGLIB_H */
])dnl

# Checks for library functions.
AC_FUNC_MALLOC([])dnl
AC_FUNC_REALLOC([])dnl
dnl# I wish func checks allowed the includes to be modified, but oh well:
AC_CHECK_FUNCS([atoi calloc exit fclose fileno fmemopen fopen free fseek \
                fstat ftell fwrite longjmp memcpy open_memstream printf \
                rewind setjmp snprintf sscanf strchr strcmp strdup strlen \
                strncmp tmpfile])dnl

# jpeg-specific functions:
AC_CHECK_FUNCS([jpeg_calc_output_dimensions \
jpeg_create_compress jpeg_create_decompress \
jpeg_destroy_compress jpeg_destroy_decompress \
jpeg_finish_compress jpeg_finish_decompress \
jpeg_read_header jpeg_read_scanlines jpeg_save_markers \
jpeg_set_defaults jpeg_set_quality \
jpeg_start_compress jpeg_start_decompress \
jpeg_std_error jpeg_stdio_dest jpeg_stdio_src jpeg_write_marker])dnl

# Checks for declarations.
AC_CHECK_DECLS([JCS_GRAYSCALE, JCS_CMYK, JCS_RGB, JCS_YCbCr, JDCT_IFAST, JDCT_ISLOW],[],[],[
#include <stdio.h>
#ifdef HAVE_JPEGLIB_H
# include <jpeglib.h>
#else
# if defined(__GNUC__) && !defined(__STRICT_ANSI__)
#  warning "This conftest for a jpeg decl wants to include <jpeglib.h>"
# endif /* __GNUC__ && !__STRICT_ANSI__ */
#endif /* HAVE_JPEGLIB_H */
])dnl

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
    AC_DEFINE_UNQUOTED([PACKAGE_BIN_DIR],["${ac_default_prefix}/bin"],
                       [Installation directory for user executables])dnl
  else
    AC_DEFINE_UNQUOTED([PACKAGE_BIN_DIR],["${prefix}/bin"],
                       [Installation directory for user executables])dnl
  fi
else
  AC_DEFINE_UNQUOTED([PACKAGE_BIN_DIR],["${bindir}"],
                     [Installation directory for user executables])dnl
fi

dnl# Set PACKAGE_LIB_DIR in config.h.
if test "x${libdir}" = 'xNONE'; then
  if test "x${prefix}" = "xNONE"; then
    AC_DEFINE_UNQUOTED([PACKAGE_LIB_DIR],["${ac_default_prefix}/lib"],
                       [Installation directory for libraries])dnl
  else
    AC_DEFINE_UNQUOTED([PACKAGE_LIB_DIR],["${prefix}/lib"],
                       [Installation directory for libraries])dnl
  fi
else
  AC_DEFINE_UNQUOTED([PACKAGE_LIB_DIR],["${libdir}"],
                     [Installation directory for libraries])dnl
fi

dnl# Set PACKAGE_SOURCE_DIR in config.h.
packagesrcdir=`cd ${srcdir} && pwd`
AC_DEFINE_UNQUOTED([PACKAGE_SOURCE_DIR],["${packagesrcdir}"],
                   [Source code directory])dnl

my_includes=""
my_libs="-ljpeg"
AC_SUBST([my_includes])dnl
AC_SUBST([my_libs])dnl

AC_CONFIG_FILES([
Makefile
doc/Makefile
doc/latex/Makefile
doc/texinfo/Makefile
src/Makefile
src/lib/Makefile
src/bin/Makefile
epeg-config
])dnl
AC_CONFIG_COMMANDS([default],[
chmod +x epeg-config
touch epeg_docs.tar.gz
],[])dnl
AC_OUTPUT([])dnl
