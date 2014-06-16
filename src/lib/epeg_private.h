/* epeg_private.h */

#ifndef _EPEG_PRIVATE_H
#define _EPEG_PRIVATE_H 1

#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1 /* need this for fmemopen & open_memstream */
#endif /* !_GNU_SOURCE */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <setjmp.h>
#include <jpeglib.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#else
# ifndef __EPEG_PRIVATE_H_NON_AUTOTOOLS_BUILD
#  define __EPEG_PRIVATE_H_NON_AUTOTOOLS_BUILD 1
# endif /* !__EPEG_PRIVATE_H_NON_AUTOTOOLS_BUILD */
# if defined(__GNUC__) && !defined(__STRICT_ANSI__) && !defined(__STDC__)
#  warning "epeg_private.h expects config.h to be included."
# endif /* __GNUC__ && !__STRICT_ANSI__ && !__STDC__ */
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif /* HAVE_STDINT_H */

/* if it starts with an underscore, it is private and goes in this file. */
/* structures: */
typedef struct _epeg_error_mgr *emptr;

struct _epeg_error_mgr
{
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
};

struct _Epeg_Image
{
	struct _epeg_error_mgr jerr;
	struct stat stat_info;
	unsigned char *pixels;
	unsigned char **lines;

	char scaled : 1;

	int error;

	Epeg_Colorspace color_space;

	struct {
		char *file;
		int w, h;
		char *comment;
		FILE *f;
		J_COLOR_SPACE color_space;
		struct jpeg_decompress_struct jinfo;
		struct {
			char *uri;
#if defined(HAVE_UINTMAX_T) && !defined(__LP64__)
			uintmax_t mtime;
#else
			unsigned long long int mtime;
#endif /* HAVE_UINTMAX_T && !__LP64__ */
			int w, h;
			char *mime;
		} thumb_info;
	} in;
	struct {
		char *file;
		struct {
			unsigned char **data;
			int *size;
		} mem;
		int x, y;
		int w, h;
		char *comment;
		FILE *f;
		struct jpeg_compress_struct jinfo;
		int quality;
		char thumbnail_info : 1;
	} out;
};

/* prototypes: */
FILE *_epeg_memfile_read_open(void *data, size_t size);
void _epeg_memfile_read_close(FILE *f);
FILE *_epeg_memfile_write_open(void **data, size_t *size);
void _epeg_memfile_write_close(FILE *f);

#endif /* !_EPEG_PRIVATE_H */

/* EOF */
