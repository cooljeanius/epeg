#==========================================================================
#        http://www.gnu.org/software/autoconf-archive/ax_check_gd.html
#==========================================================================
#
# SYNOPSIS
#
#   AX_CHECK_GD
#
# DESCRIPTION
#
#   Check for the gd library. (See http://www.boutell.com/gd/) If gd is
#   found, the output variables GD_CFLAGS, GD_LDFLAGS and GD_LIBS will
#   contain the compiler flags, linker flags and libraries necessary to use
#   gd; otherwise, those variables will be empty. In addition, the symbol
#   HAVE_GD is defined if the library is found, & the symbols HAVE_GD_GIF,
#   HAVE_GD_JPEG and HAVE_GD_PNG are defined if the lirbary supports
#   creating images in gif, jpeg and png formats, respectively.
#
#   The user may use --with-gd=no or --without-gd to skip checking for the
#   library. (The default is --with-gd=yes.) If the library is installed in
#   an unusual location, --with-gd=DIR will cause the macro to look for
#   gdlib-config in DIR/bin or, failing that, for the headers and libraries
#   in DIR/include and DIR/lib.
#
#   Feedback welcome!
#
# LICENSE
#
#   Copyright (c) 2008 Nick Markham <markhn@rpi.edu>
#
#   Copying and distribution of this file, with/without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 10

AC_DEFUN([AX_CHECK_GD],[
    AC_ARG_WITH([gd],
        [AS_HELP_STRING([--with-gd(=DIR)],
                        [use the gd library (in DIR)])],[],
        [with_gd=yes])dnl

    if test "x${with_gd}" != "xno"; then
        AC_PATH_PROG([GDLIB_CONFIG],[gdlib-config],[],
                     [${with_gd}/bin:${PATH}])dnl

        if test -n "${GDLIB_CONFIG}"; then
            GD_CFLAGS=`${GDLIB_CONFIG} --cflags`
            GD_LDFLAGS=`${GDLIB_CONFIG} --ldflags`
            GD_LIBS=`${GDLIB_CONFIG} --libs`
        elif test -d "${with_gd}"; then
            GD_CFLAGS="-I${with_gd}/include"
            GD_LDFLAGS="-L${with_gd}/lib"
            AC_CHECK_LIB([z],[inflateReset],[GD_LIBS="-lz"])dnl
            AC_CHECK_LIB([png],[png_check_sig],
                         [GD_LIBS="-lpng ${GD_LIBS}"],[],
                         [${GD_LIBS}])
        fi

        save_CFLAGS="${CFLAGS}"
        CFLAGS="${GD_CFLAGS} ${CFLAGS}"
        save_LDFLAGS="${LDFLAGS}"
        LDFLAGS="${GD_LDFLAGS} ${LDFLAGS}"

        AC_CHECK_LIB([gd],[gdImageCreate],[
            AC_DEFINE([HAVE_GD],[1],
                      [Define to 1 if you have the gd library.])dnl
            AC_CHECK_LIB([gd],[gdImageGif],[
                AC_DEFINE([HAVE_GD_GIF],[1],
                          [Define to 1 if GD supports gif.])],
                [],["${GD_LIBS}"])dnl
            AC_CHECK_LIB([gd],[gdImageJpeg],[
                AC_DEFINE([HAVE_GD_JPEG],[1],
                          [Define to 1 if GD supports jpeg.])],
                [],["${GD_LIBS}"])dnl
            AC_CHECK_LIB([gd],[gdImagePng],[
                AC_DEFINE([HAVE_GD_PNG],[1],
                          [Define to 1 if GD supports png.])],
                [],["${GD_LIBS}"])
            GD_LIBS="-lgd ${GD_LIBS}"
        ],[with_gd=no],[${GD_LIBS}])dnl

        CFLAGS="${save_CFLAGS}"
        LDFLAGS="${save_LDFLAGS}"
    fi

    if test "x${with_gd}" = "xno"; then
        GD_CFLAGS="";
        GD_LDFLAGS="";
        GD_LIBS="";
    fi

    AC_SUBST([GD_CFLAGS])dnl
    AC_SUBST([GD_LDFLAGS])dnl
    AC_SUBST([GD_LIBS])dnl
])

dnl# EOF
