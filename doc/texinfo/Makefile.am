## automake Makefile for doc/texinfo

info_TEXINFOS = epeg.texi

epeg_TEXINFOS = version.texi

## remove the "AM_" prefix to override the automake default temporarily
## if it is ever necessary to fix issues with it
AM_TEXI2DVI = texi2dvi --verbose --batch -I . -I ../latex 2>&1

DISTCLEANFILES = stamp-vti

# A list of all the files in the current directory which can be regenerated
MAINTAINERCLEANFILES = Makefile.in @MAINTAINER_MODE_TRUE@ version.texi
