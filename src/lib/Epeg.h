/* Epeg.h */

#ifndef _EPEG_H
#define _EPEG_H 1

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif /* HAVE_STDINT_H */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum _Epeg_Colorspace {
	EPEG_GRAY8,
	EPEG_YUV8,
	EPEG_RGB8,
	EPEG_BGR8,
	EPEG_RGBA8,
	EPEG_BGRA8,
	EPEG_ARGB32,
	EPEG_CMYK
} Epeg_Colorspace;

typedef struct _Epeg_Image Epeg_Image;
typedef struct _Epeg_Thumbnail_Info Epeg_Thumbnail_Info;

struct _Epeg_Thumbnail_Info {
	char *uri;
#if defined(HAVE_UINTMAX_T) && !defined(__LP64__)
	uintmax_t mtime;
#else
	unsigned long long int mtime;
#endif /* HAVE_UINTMAX_T && !__LP64__ */
	int w, h;
	char  *mimetype;
};

extern Epeg_Image *epeg_file_open(const char *file);
extern Epeg_Image *epeg_memory_open(unsigned char *data, int size);
extern void epeg_size_get(Epeg_Image *im, int *w, int *h);
extern void epeg_decode_size_set(Epeg_Image *im, int w, int h);
extern void epeg_decode_colorspace_set(Epeg_Image *im,
									   Epeg_Colorspace colorspace);
extern const void *epeg_pixels_get(Epeg_Image *im, int x, int y, int w, int h);
extern void epeg_pixels_free(Epeg_Image *im, const void *data);
extern const char *epeg_comment_get(Epeg_Image *im);
extern void epeg_thumbnail_comments_get(Epeg_Image *im,
										Epeg_Thumbnail_Info *info);
extern void epeg_comment_set(Epeg_Image *im, const char *comment);
extern void epeg_quality_set(Epeg_Image *im, int quality);
extern void epeg_thumbnail_comments_enable(Epeg_Image *im, int onoff);
extern void epeg_file_output_set(Epeg_Image *im, const char *file);
extern void epeg_memory_output_set(Epeg_Image *im, unsigned char **data,
								   int *size);
extern int epeg_encode(Epeg_Image *im);
extern int epeg_trim(Epeg_Image *im);
extern void epeg_close(Epeg_Image *im);
extern void epeg_colorspace_get(Epeg_Image *im, int *space);
extern void epeg_decode_bounds_set(Epeg_Image *im, int x, int y, int w, int h);
extern const void *epeg_pixels_get_as_RGB8(Epeg_Image *im,
										   int x, int y, int w, int h);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_EPEG_H */

/* EOF */
