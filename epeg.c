/** 
@file
@brief Epeg JPEG Thumbnailer library
 
These routines are used for the Epeg library.
*/

/**

@mainpage Epeg Library Documentation
@image html  epeg.png
@version 0.9.0
@author Carsten Haitzler <raster@rasterman.com>
@date 2000-2003

@section intro What is Epeg?

An IMMENSELY FAST JPEG thumbnailer library API.

Why write this? It is a convenience library API to using libjpeg to load
JPEG images destined to be turned into thumbnails of the original, saving
information with these thumbnails, retreiving it and managing to load the
image ready for scaling with the minimum of fuss and CPU overhead.

This means that it is insanely fast at loading large JPEG images
and scaling them down to tiny thumbnails. The speedup that it provides
will be proportional to the size difference between the source image
and the output thumbnail size as a count of their pixels.

It makes use of libjpeg features of being able to load an image by only
decoding the DCT coefficients needed to reconstruct an image of the size
desired. This gives a massive speedup. If you do NOT try to access the
pixels in a format other than YUV (or GRAY8 if the source is grascale),
then it also avoids colorspace conversions as well.

Using the library is very easy; look at this example:

@code
#include <stdio.h> /* for printf() */
#include <stdlib.h> /* for exit() */
#include "Epeg.h"

int main(int argc, char **argv)
{
   Epeg_Image *im;

   if (argc != 3) {
        printf("Usage: %s input.jpg thumb.jpg\n", argv[0]);
        exit(0);
   }
   im = epeg_file_open(argv[1]);
   if (!im) {
        printf("Cannot open %s\n", argv[1]);
        exit(-1);
   }
   
   epeg_decode_size_set(im, 128, 96);
   epeg_quality_set(im, 75);
   epeg_thumbnail_comments_enable(im, 1);
   epeg_file_output_set(im, argv[2]);
   epeg_encode(im);
   epeg_close(im);
   
   return 0;
}
@endcode

This program exists as epeg_test.c so that you can compile this program
with as small a line as:

@verbatim
gcc epeg_test.c -o epeg_test `epeg-config --cflags --libs`
@endverbatim

It is a very simple library that just makes life easier when trying to
generate lots of thumbnails for large JPEG images as quickly as possible.
Your milage may vary, but it should save you lots of time and effort in
using libjpeg in general.

@todo Check all input parameters for sanity.
@todo Actually report error conditions properly.

*/
/* epeg_main.c for src/lib for epeg */
/* gets built into the libepeg library */

#include <stdio.h>
#include <stdlib.h>
#include "Epeg.h"
#include "epeg_private.h"

static Epeg_Image*_epeg_open_header(Epeg_Image *im);
static int _epeg_decode(Epeg_Image *im);
static int _epeg_scale(Epeg_Image *im);
static int _epeg_decode_for_trim(Epeg_Image *im);
static int _epeg_trim(Epeg_Image *im);
static int _epeg_encode(Epeg_Image *im);

static void _epeg_fatal_error_handler(j_common_ptr cinfo);

#ifndef MIN
# define MIN(__x,__y) ((__x) < (__y) ? (__x) : (__y))
#endif /* !MIN */
#ifndef MAX
# define MAX(__x,__y) ((__x) > (__y) ? (__x) : (__y))
#endif /* !MAX */

/**
 * Open a JPEG image by filename.
 * @param file The file path to open.
 * @return A handle to the opened JPEG file, with the header decoded.
 *
 * This function opens the file indicated by the @p file parameter, and
 * attempts to decode it as a jpeg file. If this failes, NULL is returned.
 * Otherwise a valid handle to an open JPEG file is returned that can be used
 * by other Epeg calls.
 *
 * The @p file must be a pointer to a valid C string, NUL (0 byte) terminated
 * thats is a relative or absolute file path. If not results are not
 * determined.
 *
 * See also: epeg_memory_open(), epeg_close()
 */
extern Epeg_Image *epeg_file_open(const char *file)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.file = strdup(file);
   im->in.f = fopen(im->in.file, "rb");
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   fstat(fileno(im->in.f), &(im->stat_info));
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Open a JPEG image stored in memory.
 * @param data A pointer to the memory containing the JPEG data.
 * @param size The size of the memory segment containing the JPEG.
 * @return  A handle to the opened JPEG, with the header decoded.
 *
 * This function opens a JPEG file that is stored in memory pointed to by
 * @p data, and that is @p size bytes in size. If successful a valid handle
 * is returned, or on failure NULL is returned.
 *
 * See also: epeg_file_open(), epeg_close()
 */
extern Epeg_Image *epeg_memory_open(unsigned char *data, int size)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.f = _epeg_memfile_read_open(data, (size_t)size);
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Return the original JPEG pixel size.
 * @param im A handle to an opened Epeg image.
 * @param w A pointer to the width value in pixels to be filled in.
 * @param h A pointer to the height value in pixels to be filled in.
 * @return Nothing.
 *
 * Returns the image size in pixels (well not really).
 *
 * See also: epeg_colorspace_get()
 */
extern void epeg_size_get(Epeg_Image *im, int *w, int *h)
{
	if (w) {
		*w = im->in.w;
	}
	if (h) {
		*h = im->in.h;
	}
}

/**
 * Return the original JPEG pixel color space.
 * @param im A handle to an opened Epeg image.
 * @param space A pointer to the color space value to be filled in.
 * @return Nothing.
 *
 * Returns the image color space (not yet though).
 *
 * See also: epeg_size_get()
 */
extern void epeg_colorspace_get(Epeg_Image *im, int *space)
{
	if (space) {
		*space = im->color_space;
	}
}

/**
 * Set the size of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param w The width of the image to decode at, in pixels.
 * @param h The height of the image to decode at, in pixels.
 * @return Nothing.
 *
 * Sets the size at which to decode the JPEG image, giving an optimized load
 * that only decodes the pixels needed.
 *
 * See also: epeg_decode_bounds_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_size_set(Epeg_Image *im, int w, int h)
{
   if (im->pixels) {
	   return;
   }
   if (w < 1) {
	   w = 1;
   } else if (w > im->in.w) {
	   w = im->in.w;
   }
   if (h < 1) {
	   h = 1;
   } else if (h > im->in.h) {
	   h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   im->out.x = 0;
   im->out.y = 0;
}

/**
 * Set the bounds of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param x Boundary X
 * @param y Boundary Y
 * @param w Boundary W
 * @param h Boundary H
 * @return Nothing.
 *
 * Sets the bounds inside which to decode the JPEG image,
 * giving an optimized load that only decodes the bounded pixels.
 * (???)
 *
 * See also: epeg_decode_size_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_bounds_set(Epeg_Image *im, int x, int y, int w, int h)
{
   if (im->pixels) {
      return;
   }
   if (w < 1) {
      w = 1;
   } else if (w > im->in.w) {
      w = im->in.w;
   }
   if (h < 1) {
      h = 1;
   } else if (h > im->in.h) {
      h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   if (x < 0) {
      x = 0;
   }
   if (y < 0) {
      y = 0;
   }
   im->out.x = x;
   im->out.y = y;
}

/**
 * Set the colorspace in which to decode the image.
 * @param im A handle to an opened Epeg image.
 * @param colorspace The colorspace to decode the image in.
 * @return Nothing.
 *
 * This sets the colorspace to decode the image in. The default is EPEG_YUV8,
 * as this is normally the native colorspace of a JPEG file, avoiding any
 * colorspace conversions for a faster load and/or save.
 *
 * See also: epeg_decode_size_set(), epeg_decode_bounds_set()
 */
extern void epeg_decode_colorspace_set(Epeg_Image *im,
                                       Epeg_Colorspace colorspace)
{
	if (im->pixels) {
		return;
	}
	if ((colorspace < EPEG_GRAY8) || (colorspace > EPEG_ARGB32)) {
		return;
	}
	im->color_space = colorspace;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get_as_RGB8()
 */
extern const void *epeg_pixels_get(Epeg_Image *im, int x, int y,  int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
         return NULL;
      }
   }

   if (!im->pixels) {
      return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* go through all 8 values in type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace): */
   if (im->color_space == EPEG_GRAY8) {
      /* this whole block is pretty much the same thing we do for each
       * of the other enumeration values as well... could probably be simplified
       * with a function-like macro or some other sort of meta-programming: */
      unsigned char *pix, *p;

      /* '1': */
      pix = (unsigned char *)malloc((size_t)(w * h * 1L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = s[0];
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_YUV8) {
      unsigned char *pix, *p;

      /* '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGR8) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* reverse order: */
            p[0] = s[2];
            p[1] = s[1];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGBA8) {
      unsigned char *pix, *p;

      /* '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGRA8) {
      unsigned char *pix, *p;

      /* also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = 0xff;
            /* reverse order: */
            p[1] = s[2];
            p[2] = s[1];
            p[3] = s[0];
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_ARGB32) {
      unsigned int *pix, *p;

      /* still also '4': */
      pix = (unsigned int *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (0xff000000 | (unsigned int)(s[0] << 16) | (unsigned int)(s[1] << 8) | (s[2]));
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* another thing that is also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* back to same order again: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } /* end "if-else" that seems like it should really be a "switch" */
   return NULL;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get()
 */
extern const void *epeg_pixels_get_as_RGB8(Epeg_Image *im, int x, int y,
                                           int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
		   return NULL;
	   }
   }

   if (!im->pixels) {
	   return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* unlike the "non-'_as_RGB8'-version" of this function,
    * there are no 'else'-es separating the cases here. Also, we only go through
    * three of the enumeration values of type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace) here: */
   if (im->color_space == EPEG_GRAY8) {
      unsigned char *pix, *p;

      /* get right into the '3' ones: */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[0];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (unsigned char)(MIN(255, (s[0] * s[3]) / 255));
            p[1] = (unsigned char)(MIN(255, (s[1] * s[3]) / 255));
            p[2] = (unsigned char)(MIN(255, (s[2] * s[3]) / 255));
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   return NULL;
}

/**
 * Free requested pixel block from an image.
 * @param im A handle to an opened Epeg image (unused).
 * @param data The pointer to the image pixels.
 * @return Nothing.
 *
 * This frees the data for a block of pixels requested from image @p im.
 * @p data must be a valid (non NULL) pointer to a pixel block taken from the
 * image @p im by epeg_pixels_get() and must be called before the image is
 * closed by epeg_close().
 */
extern void epeg_pixels_free(Epeg_Image *im, const void *data)
{
#pragma unused (im)
   free((void *)data);
}

/**
 * Get the image comment field as a string.
 * @param im A handle to an opened Epeg image.
 * @return A pointer to the loaded image comments.
 *
 * This function returns the comment field as a string (NUL byte terminated)
 * of the loaded image @p im, if there is a comment, or NULL if no comment is
 * saved with the image. Consider the string returned to be read-only.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern const char *epeg_comment_get(Epeg_Image *im)
{
   return im->in.comment;
}

/**
 * Get thumbnail comments of loaded image.
 * @param im A handle to an opened Epeg image.
 * @param info Pointer to a thumbnail info struct to be filled in.
 * @return Nothing.
 *
 * This function retrieves thumbnail comments written by Epeg to any saved
 * JPEG files. If no thumbnail comments were saved, the fields will be 0 in
 * the @p info struct on return.
 *
 * See also: epeg_comment_get(), epeg_thumbnail_comments_enable()
 */
extern void epeg_thumbnail_comments_get(Epeg_Image *im,
                                        Epeg_Thumbnail_Info *info)
{
   if (!info) {
      return;
   }
   info->uri = im->in.thumb_info.uri;
   info->mtime = im->in.thumb_info.mtime;
   info->w = im->in.thumb_info.w;
   info->h = im->in.thumb_info.h;
   info->mimetype = im->in.thumb_info.mime;
}

/**
 * Set the comment field of the image for saving.
 * @param im A handle to an opened Epeg image.
 * @param comment The comment to set.
 * @return Nothing.
 *
 * Set the comment for the image file for when it gets saved. This is a NUL
 * byte terminated C string. If @p comment is NULL the output file will have
 * no comment field.
 *
 * The default comment will be any comment loaded from the input file.
 *
 * See also: epeg_comment_get()
 */
extern void epeg_comment_set(Epeg_Image *im, const char *comment)
{
   if (im->out.comment) {
      free(im->out.comment);
   }
   if (!comment) {
      im->out.comment = NULL;
   } else {
      im->out.comment = strdup(comment);
   }
}

/**
 * Set the encoding quality of the saved image.
 * @param im A handle to an opened Epeg image.
 * @param quality The quality of encoding from 0 to 100.
 * @return Nothing.
 *
 * Set the quality of the output encoded image. Values from 0 to 100
 * inclusive are valid, with 100 being the maximum quality, and 0 being the
 * minimum. If the quality is set equal to or above 90%, the output U and V
 * color planes are encoded at 1:1 with the Y plane.
 *
 * The default quality is 75.
 *
 * See also: epeg_comment_set()
 */
extern void epeg_quality_set(Epeg_Image *im, int quality)
{
   if (quality < 0) {
      quality = 0;
   } else if (quality > 100) {
      quality = 100;
   }
   im->out.quality = quality;
}

/**
 * Enable thumbnail comments in saved image.
 * @param im A handle to an opened Epeg image.
 * @param onoff A boolean on and off enabling flag.
 * @return Nothing.
 *
 * if @p onoff is 1, the output file will have thumbnail comments added to
 * it, and if it is 0, it will not. The default is 0.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern void epeg_thumbnail_comments_enable(Epeg_Image *im, int onoff)
{
   im->out.thumbnail_info = (char)onoff;
}

/**
 * Set the output file path for the image when saved.
 * @param im A handle to an opened Epeg image.
 * @param file The path to the output file.
 * @return Nothing.
 *
 * This sets the output file path name (either a full or relative path name)
 * to where the file will be written when saved. @p file must be a NUL
 * terminated C string containing the path to the file to be saved to. If it is
 * NULL, then the image will not be saved to a file when calling epeg_encode().
 *
 * See also: epeg_memory_output_set(), epeg_encode()
 */
extern void epeg_file_output_set(Epeg_Image *im, const char *file)
{
   if (im->out.file) {
      free(im->out.file);
   }
   if (!file) {
      im->out.file = NULL;
   } else {
      im->out.file = strdup(file);
   }
}

/**
 * Set the output file to be a block of allocated memory.
 * @param im A handle to an opened Epeg image.
 * @param data A pointer to a pointer to a memory block.
 * @param size A pointer to a counter of the size of the memory block.
 * @return Nothing.
 *
 * This sets the output encoding of the image when saved to be allocated
 * memory. After epeg_close() is called the pointer pointed to by @p data
 * and the integer pointed to by @p size will contain the pointer to the
 * memory block and its size in bytes, respecitvely. The memory block can be
 * freed with the free() function call. If the save fails the pointer to the
 * memory block will be unaffected, as will the size.
 *
 * See also: epeg_file_output_set(), epeg_encode()
 */
extern void epeg_memory_output_set(Epeg_Image *im, unsigned char **data,
                                   int *size)
{
   im->out.mem.data = data;
   im->out.mem.size = size;
}

/**
 * This saves the image to its specified destination.
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * This saves the image @p im to its destination specified by
 * epeg_file_output_set() or epeg_memory_output_set(). The image will be
 * encoded at the decoded pixel size, using the quality, comment,
 * and thumbnail comment settings set on the image.
 *
 * See also: epeg_file_output_set(), epeg_memory_output_set()
 */
extern int epeg_encode(Epeg_Image *im)
{
   if (_epeg_decode(im) != 0) {
      return 1;
   }
   if (_epeg_scale(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * FIXME: Document this with a short, sentence-long description of epeg_trim()
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * FIXME: Document this with a longer, paragraph-long description.
 */
extern int epeg_trim(Epeg_Image *im)
{
   if (_epeg_decode_for_trim(im) != 0) {
      return 1;
   }
   if (_epeg_trim(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * Close an image handle.
 * @param im A handle to an opened Epeg image.
 * @return Nothing.
 *
 * This closes an opened image handle and frees all memory associated with it.
 * It does NOT free encoded data generated by epeg_memory_output_set() followed
 * by epeg_encode(), nor does it guarantee to free any data received by
 * epeg_pixels_get(). Once an image handle is closed, consider it invalid.
 *
 * See also: epeg_file_open(), epeg_memory_open()
 */
extern void epeg_close(Epeg_Image *im)
{
   if (im->pixels) {
      free(im->pixels);
   }
   if (im->lines) {
      free(im->lines);
   }
   if (im->in.file) {
      free(im->in.file);
   }
   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if (im->in.f) {
      fclose(im->in.f);
   }
   if (im->in.comment) {
      free(im->in.comment);
   }
   if (im->in.thumb_info.uri) {
      free(im->in.thumb_info.uri);
   }
   if (im->in.thumb_info.mime) {
      free(im->in.thumb_info.mime);
   }
   if (im->out.file) {
      free(im->out.file);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if (im->out.f) {
      fclose(im->out.f);
   }
   if (im->out.comment) {
      free(im->out.comment);
   }
   free(im);
}

/* static internal private-only function; unnecessary to document: */
static Epeg_Image *_epeg_open_header(Epeg_Image *im)
{
   struct jpeg_marker_struct *m;

   im->in.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      error:
      epeg_close(im);
      im = NULL;
      return NULL;
   }

   jpeg_create_decompress(&(im->in.jinfo));
   jpeg_save_markers(&(im->in.jinfo), (JPEG_APP0 + 7), 1024);
   jpeg_save_markers(&(im->in.jinfo), JPEG_COM, 65535);
   jpeg_stdio_src(&(im->in.jinfo), im->in.f);
   jpeg_read_header(&(im->in.jinfo), TRUE);
   im->in.w = (int)im->in.jinfo.image_width;
   im->in.h = (int)im->in.jinfo.image_height;
   if (im->in.w <= 1) {
      goto error;
   }
   if (im->in.h <= 1) {
      goto error;
   }

   im->out.w = im->in.w;
   im->out.h = im->in.h;

   im->color_space = (((im->in.color_space = im->in.jinfo.out_color_space) == JCS_GRAYSCALE) ? EPEG_GRAY8 : EPEG_RGB8);
   if (im->in.color_space == JCS_CMYK) {
      im->color_space = EPEG_CMYK;
   }

   for ((m = im->in.jinfo.marker_list); m; (m = m->next)) {
      if (m->marker == JPEG_COM) {
         if (im->in.comment) {
            free(im->in.comment);
         }
         im->in.comment = (char *)malloc((size_t)(m->data_length + 1));
         if (im->in.comment) {
            memcpy(im->in.comment, m->data, (size_t)m->data_length);
            im->in.comment[m->data_length] = 0;
         }
      } else if (m->marker == (JPEG_APP0 + 7))  {
         if ((m->data_length > 7) &&
             (!strncmp((char *)m->data, "Thumb::", (size_t)7))) {
            char *p, *p2;

            p = (char *)malloc((size_t)(m->data_length + 1));
            if (p) {
               memcpy(p, m->data, (size_t)m->data_length);
               p[m->data_length] = 0;
               p2 = strchr(p, '\n');
               if (p2) {
                  p2[0] = 0;
                  if (!strcmp(p, "Thumb::URI")) {
                     im->in.thumb_info.uri = strdup(p2 + 1);
                  } else if (!strcmp(p, "Thumb::MTime")) {
                     sscanf((p2 + 1), "%llu", &(im->in.thumb_info.mtime));
                  } else if (!strcmp(p, "Thumb::Image::Width")) {
                     im->in.thumb_info.w = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Image::Height")) {
                     im->in.thumb_info.h = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Mimetype")) {
                     im->in.thumb_info.mime = strdup(p2 + 1);
                  }
               } /* end "if (p2)" */
               free(p);
            } /* end "if (p)" */
         }
      }
   } /* end for-loop */
   return im;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode(Epeg_Image *im)
{
   int scale, scalew, scaleh, y;

   if (im->pixels) {
      return 1;
   }

   scalew = (im->in.w / im->out.w);
   scaleh = (im->in.h / im->out.h);

   scale = scalew;
   if (scaleh < scalew) {
      scale = scaleh;
   }

   if (scale > 8) {
      scale = 8;
   } else if (scale < 1) {
      scale = 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = (unsigned int)scale;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_IFAST;

   switch (im->color_space) {
      case EPEG_GRAY8:
			 im->in.jinfo.out_color_space = JCS_GRAYSCALE;
			 im->in.jinfo.output_components = 1;
			 break;

      case EPEG_YUV8:
			 im->in.jinfo.out_color_space = JCS_YCbCr;
			 break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
			 im->in.jinfo.out_color_space = JCS_RGB;
			 break;

      case EPEG_CMYK:
			 im->in.jinfo.out_color_space = JCS_CMYK;
			 im->in.jinfo.output_components = 4;
			 break;

      default:
			 break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      epeg_close(im);
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
	   jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_scale(Epeg_Image *im)
{
   unsigned char *dst, *row, *src;
   int            x, y, w, h, i;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   for ((y = 0); (y < h); y++) {
      row = (im->pixels +
             ((((unsigned int)y * im->in.jinfo.output_height) / (unsigned int)h) * (unsigned int)im->in.jinfo.output_components * im->in.jinfo.output_width));
      dst = (im->pixels +
             ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));

      for ((x = 0); (x < im->out.w); x++) {
         src = (row +
                ((((unsigned int)x * im->in.jinfo.output_width) / (unsigned int)w) * (unsigned int)im->in.jinfo.output_components));

         for ((i = 0); (i < im->in.jinfo.output_components); i++) {
            dst[i] = src[i];
         } /* end inmost for-loop */

         dst += im->in.jinfo.output_components;
      } /* end inner for-loop */
   } /* end outer for-loop */
   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode_for_trim(Epeg_Image *im)
{
   int y;

   if (im->pixels) {
      return 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = 1;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_ISLOW;

   switch (im->color_space) {
      case EPEG_GRAY8:
		   im->in.jinfo.out_color_space = JCS_GRAYSCALE;
		   im->in.jinfo.output_components = 1;
		   break;

      case EPEG_YUV8:
		   im->in.jinfo.out_color_space = JCS_YCbCr;
		   break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
		   im->in.jinfo.out_color_space = JCS_RGB;
		   break;

      case EPEG_CMYK:
		   im->in.jinfo.out_color_space = JCS_CMYK;
		   im->in.jinfo.output_components = 4;
		   break;

      default:
		   break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
      im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
      jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_trim(Epeg_Image *im)
{
   int y, a, b, w, h;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   a = im->out.x;
   b = im->out.y;

   /* dummy condition to use value stored to 'w': */
   if (w == 0) {
	   ;
   }

   for ((y = 0); (y < h); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)((y + b) * im->in.jinfo.output_components) * im->in.jinfo.output_width)
                      + (a * im->in.jinfo.output_components));
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_encode(Epeg_Image *im)
{
   void  *data = NULL;
   size_t size = 0;

   if (im->out.f) {
      return 1;
   }

   if (im->out.file) {
      im->out.f = fopen(im->out.file, "wb");
   } else {
      im->out.f = _epeg_memfile_write_open(&data, &size);
   }
   if (!im->out.f) {
      im->error = 1;
      return 1;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_create_compress(&(im->out.jinfo));
   jpeg_stdio_dest(&(im->out.jinfo), im->out.f);
   im->out.jinfo.image_width = (JDIMENSION)im->out.w;
   im->out.jinfo.image_height = (JDIMENSION)im->out.h;
   im->out.jinfo.input_components = im->in.jinfo.output_components;
   im->out.jinfo.in_color_space = im->in.jinfo.out_color_space;
   im->out.jinfo.dct_method = JDCT_IFAST;
   im->out.jinfo.dct_method = im->in.jinfo.dct_method;
   jpeg_set_defaults(&(im->out.jinfo));
   jpeg_set_quality(&(im->out.jinfo), im->out.quality, TRUE);

   if (im->out.quality >= 90) {
      im->out.jinfo.comp_info[0].h_samp_factor = 1;
      im->out.jinfo.comp_info[0].v_samp_factor = 1;
      im->out.jinfo.comp_info[1].h_samp_factor = 1;
      im->out.jinfo.comp_info[1].v_samp_factor = 1;
      im->out.jinfo.comp_info[2].h_samp_factor = 1;
      im->out.jinfo.comp_info[2].v_samp_factor = 1;
   }
   jpeg_start_compress(&(im->out.jinfo), TRUE);

   if (im->out.comment) {
      jpeg_write_marker(&(im->out.jinfo), JPEG_COM,
                        (const JOCTET *)im->out.comment,
                        (unsigned int)strlen(im->out.comment));
   }

   if (im->out.thumbnail_info) {
      char buf[8192];

      if (im->in.file) {
         snprintf(buf, sizeof(buf), "Thumb::URI\nfile://%s", im->in.file);
         jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7),
                           (const JOCTET *)buf, (unsigned int)strlen(buf));
         snprintf(buf, sizeof(buf), "Thumb::MTime\n%llu",
#if defined(HAVE_UINTMAX_T) && !defined(__LP64__)
                  (uintmax_t)im->stat_info.st_mtime);
#else
               (unsigned long long int)im->stat_info.st_mtime);
#endif /* HAVE_UINTMAX_T && !__LP64__ */
      }
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Width\n%i", im->in.w);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Height\n%i", im->in.h);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Mimetype\nimage/jpeg");
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
   }

   while (im->out.jinfo.next_scanline < im->out.h) {
      jpeg_write_scanlines(&(im->out.jinfo),
                           &(im->lines[im->out.jinfo.next_scanline]), 1);
   }

   jpeg_finish_compress(&(im->out.jinfo));

   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if ((im->in.f) && (im->in.file)) {
      fclose(im->in.f);
   }
   if ((im->in.f) && (!im->in.file)) {
      _epeg_memfile_read_close(im->in.f);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if ((im->out.f) && (im->out.file)) {
      fclose(im->out.f);
   }
   if ((im->out.f) && (!im->out.file)) {
      _epeg_memfile_write_close(im->out.f);
   }
   im->in.f = NULL;
   im->out.f = NULL;

   if (im->out.mem.data) {
	   *(im->out.mem.data) = (unsigned char *)data;
   }
   if (im->out.mem.size) {
	   *(im->out.mem.size) = (int)size;
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static void _epeg_fatal_error_handler(j_common_ptr cinfo)
{
   emptr errmgr;

   errmgr = (emptr)cinfo->err;
   longjmp(errmgr->setjmp_buffer, 1);
   return;
}

/* silence '-Wunused-macros' warnings: */
#ifdef MAX
# undef MAX
#endif /* MAX */

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* epeg_memfile.c */
/* gets built into the libepeg library */

#include <stdio.h>
#include "Epeg.h"
#include "epeg_private.h"

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_read_open(void *data, size_t size)
{
#ifdef HAVE_FMEMOPEN
   return (FILE *)fmemopen(data, size, "r");
#else
   FILE *f;

   f = tmpfile();
   if (!f) {
      return NULL;
   }
   fwrite(data, size, (size_t)1L, f);
   rewind(f);
   return f;
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_read_close(FILE *f)
{
#ifdef HAVE_FMEMOPEN
   fclose(f);
#else
   /* FIXME: make these different halves of the conditional actually different
    * from one another */
   fclose(f);
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only struct and typedef; unnecessary to document: */
typedef struct _Eet_Memfile_Write_Info Eet_Memfile_Write_Info;
struct _Eet_Memfile_Write_Info
{
   FILE *f;
   void **data;
   size_t *size;
};

/* static internal private-only global variables; unnecessary to document: */
static int _epeg_memfile_info_alloc_num = 0;
static int _epeg_memfile_info_num = 0;
static Eet_Memfile_Write_Info *_epeg_memfile_info = NULL;

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_write_open(void **data, size_t *size)
{
#ifdef HAVE_OPEN_MEMSTREAM
   return open_memstream((char **)data, size);
#else
   FILE *f;

   _epeg_memfile_info_num++;
   if (_epeg_memfile_info_num > _epeg_memfile_info_alloc_num) {
      Eet_Memfile_Write_Info *tmp;

      _epeg_memfile_info_alloc_num += 16;
      tmp = (Eet_Memfile_Write_Info *)realloc(_epeg_memfile_info,
                                              ((unsigned long)_epeg_memfile_info_alloc_num *
                                               sizeof(Eet_Memfile_Write_Info)));
      if (!tmp) {
         _epeg_memfile_info_alloc_num -= 16;
         _epeg_memfile_info_num--;
         return NULL;
      }
      _epeg_memfile_info = tmp;
   }
   f = tmpfile();
   if (!f) {
      _epeg_memfile_info_num--;
      return NULL;
   }
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].f = f;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].data = data;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].size = size;
   return f;
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_write_close(FILE *f)
{
#ifdef HAVE_OPEN_MEMSTREAM
   fclose(f);
#else
   int i;

   for ((i = 0); (i < _epeg_memfile_info_num); i++) {
      if (_epeg_memfile_info[i].f == f) {
         int j;

         fseek(f, 0L, SEEK_END);
         (*(_epeg_memfile_info[i].size)) = (size_t)ftell(f);
         rewind(f);
         (*(_epeg_memfile_info[i].data)) = malloc(*(_epeg_memfile_info[i].size));
         if (!(*(_epeg_memfile_info[i].data))) {
            fclose(f);
            (*(_epeg_memfile_info[i].size)) = 0;
            return;
         }
         fread((*(_epeg_memfile_info[i].data)), (*(_epeg_memfile_info[i].size)),
               (size_t)1L, f);
         for ((j = (i + 1)); (j < _epeg_memfile_info_num); j++) {
            _epeg_memfile_info[(j - 1)] = _epeg_memfile_info[j];
         }
         _epeg_memfile_info_num--;
         fclose(f);
         return;
      } /* end "if" */
   } /* end for-loop */
   fclose(f);
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* epeg_main.c for src/lib for epeg */
/* gets built into the libepeg library */

#include <stdio.h>
#include <stdlib.h>
#include "Epeg.h"
#include "epeg_private.h"

static Epeg_Image*_epeg_open_header(Epeg_Image *im);
static int _epeg_decode(Epeg_Image *im);
static int _epeg_scale(Epeg_Image *im);
static int _epeg_decode_for_trim(Epeg_Image *im);
static int _epeg_trim(Epeg_Image *im);
static int _epeg_encode(Epeg_Image *im);

static void _epeg_fatal_error_handler(j_common_ptr cinfo);

#ifndef MIN
# define MIN(__x,__y) ((__x) < (__y) ? (__x) : (__y))
#endif /* !MIN */
#ifndef MAX
# define MAX(__x,__y) ((__x) > (__y) ? (__x) : (__y))
#endif /* !MAX */

/**
 * Open a JPEG image by filename.
 * @param file The file path to open.
 * @return A handle to the opened JPEG file, with the header decoded.
 *
 * This function opens the file indicated by the @p file parameter, and
 * attempts to decode it as a jpeg file. If this failes, NULL is returned.
 * Otherwise a valid handle to an open JPEG file is returned that can be used
 * by other Epeg calls.
 *
 * The @p file must be a pointer to a valid C string, NUL (0 byte) terminated
 * thats is a relative or absolute file path. If not results are not
 * determined.
 *
 * See also: epeg_memory_open(), epeg_close()
 */
extern Epeg_Image *epeg_file_open(const char *file)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.file = strdup(file);
   im->in.f = fopen(im->in.file, "rb");
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   fstat(fileno(im->in.f), &(im->stat_info));
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Open a JPEG image stored in memory.
 * @param data A pointer to the memory containing the JPEG data.
 * @param size The size of the memory segment containing the JPEG.
 * @return  A handle to the opened JPEG, with the header decoded.
 *
 * This function opens a JPEG file that is stored in memory pointed to by
 * @p data, and that is @p size bytes in size. If successful a valid handle
 * is returned, or on failure NULL is returned.
 *
 * See also: epeg_file_open(), epeg_close()
 */
extern Epeg_Image *epeg_memory_open(unsigned char *data, int size)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.f = _epeg_memfile_read_open(data, (size_t)size);
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Return the original JPEG pixel size.
 * @param im A handle to an opened Epeg image.
 * @param w A pointer to the width value in pixels to be filled in.
 * @param h A pointer to the height value in pixels to be filled in.
 * @return Nothing.
 *
 * Returns the image size in pixels (well not really).
 *
 * See also: epeg_colorspace_get()
 */
extern void epeg_size_get(Epeg_Image *im, int *w, int *h)
{
	if (w) {
		*w = im->in.w;
	}
	if (h) {
		*h = im->in.h;
	}
}

/**
 * Return the original JPEG pixel color space.
 * @param im A handle to an opened Epeg image.
 * @param space A pointer to the color space value to be filled in.
 * @return Nothing.
 *
 * Returns the image color space (not yet though).
 *
 * See also: epeg_size_get()
 */
extern void epeg_colorspace_get(Epeg_Image *im, int *space)
{
	if (space) {
		*space = im->color_space;
	}
}

/**
 * Set the size of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param w The width of the image to decode at, in pixels.
 * @param h The height of the image to decode at, in pixels.
 * @return Nothing.
 *
 * Sets the size at which to decode the JPEG image, giving an optimized load
 * that only decodes the pixels needed.
 *
 * See also: epeg_decode_bounds_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_size_set(Epeg_Image *im, int w, int h)
{
   if (im->pixels) {
	   return;
   }
   if (w < 1) {
	   w = 1;
   } else if (w > im->in.w) {
	   w = im->in.w;
   }
   if (h < 1) {
	   h = 1;
   } else if (h > im->in.h) {
	   h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   im->out.x = 0;
   im->out.y = 0;
}

/**
 * Set the bounds of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param x Boundary X
 * @param y Boundary Y
 * @param w Boundary W
 * @param h Boundary H
 * @return Nothing.
 *
 * Sets the bounds inside which to decode the JPEG image,
 * giving an optimized load that only decodes the bounded pixels.
 * (???)
 *
 * See also: epeg_decode_size_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_bounds_set(Epeg_Image *im, int x, int y, int w, int h)
{
   if (im->pixels) {
      return;
   }
   if (w < 1) {
      w = 1;
   } else if (w > im->in.w) {
      w = im->in.w;
   }
   if (h < 1) {
      h = 1;
   } else if (h > im->in.h) {
      h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   if (x < 0) {
      x = 0;
   }
   if (y < 0) {
      y = 0;
   }
   im->out.x = x;
   im->out.y = y;
}

/**
 * Set the colorspace in which to decode the image.
 * @param im A handle to an opened Epeg image.
 * @param colorspace The colorspace to decode the image in.
 * @return Nothing.
 *
 * This sets the colorspace to decode the image in. The default is EPEG_YUV8,
 * as this is normally the native colorspace of a JPEG file, avoiding any
 * colorspace conversions for a faster load and/or save.
 *
 * See also: epeg_decode_size_set(), epeg_decode_bounds_set()
 */
extern void epeg_decode_colorspace_set(Epeg_Image *im,
                                       Epeg_Colorspace colorspace)
{
	if (im->pixels) {
		return;
	}
	if ((colorspace < EPEG_GRAY8) || (colorspace > EPEG_ARGB32)) {
		return;
	}
	im->color_space = colorspace;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get_as_RGB8()
 */
extern const void *epeg_pixels_get(Epeg_Image *im, int x, int y,  int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
         return NULL;
      }
   }

   if (!im->pixels) {
      return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* go through all 8 values in type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace): */
   if (im->color_space == EPEG_GRAY8) {
      /* this whole block is pretty much the same thing we do for each
       * of the other enumeration values as well... could probably be simplified
       * with a function-like macro or some other sort of meta-programming: */
      unsigned char *pix, *p;

      /* '1': */
      pix = (unsigned char *)malloc((size_t)(w * h * 1L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = s[0];
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_YUV8) {
      unsigned char *pix, *p;

      /* '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGR8) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* reverse order: */
            p[0] = s[2];
            p[1] = s[1];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGBA8) {
      unsigned char *pix, *p;

      /* '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGRA8) {
      unsigned char *pix, *p;

      /* also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = 0xff;
            /* reverse order: */
            p[1] = s[2];
            p[2] = s[1];
            p[3] = s[0];
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_ARGB32) {
      unsigned int *pix, *p;

      /* still also '4': */
      pix = (unsigned int *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (0xff000000 | (unsigned int)(s[0] << 16) | (unsigned int)(s[1] << 8) | (s[2]));
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* another thing that is also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* back to same order again: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } /* end "if-else" that seems like it should really be a "switch" */
   return NULL;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get()
 */
extern const void *epeg_pixels_get_as_RGB8(Epeg_Image *im, int x, int y,
                                           int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
		   return NULL;
	   }
   }

   if (!im->pixels) {
	   return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* unlike the "non-'_as_RGB8'-version" of this function,
    * there are no 'else'-es separating the cases here. Also, we only go through
    * three of the enumeration values of type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace) here: */
   if (im->color_space == EPEG_GRAY8) {
      unsigned char *pix, *p;

      /* get right into the '3' ones: */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[0];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (unsigned char)(MIN(255, (s[0] * s[3]) / 255));
            p[1] = (unsigned char)(MIN(255, (s[1] * s[3]) / 255));
            p[2] = (unsigned char)(MIN(255, (s[2] * s[3]) / 255));
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   return NULL;
}

/**
 * Free requested pixel block from an image.
 * @param im A handle to an opened Epeg image (unused).
 * @param data The pointer to the image pixels.
 * @return Nothing.
 *
 * This frees the data for a block of pixels requested from image @p im.
 * @p data must be a valid (non NULL) pointer to a pixel block taken from the
 * image @p im by epeg_pixels_get() and must be called before the image is
 * closed by epeg_close().
 */
extern void epeg_pixels_free(Epeg_Image *im, const void *data)
{
#pragma unused (im)
   free((void *)data);
}

/**
 * Get the image comment field as a string.
 * @param im A handle to an opened Epeg image.
 * @return A pointer to the loaded image comments.
 *
 * This function returns the comment field as a string (NUL byte terminated)
 * of the loaded image @p im, if there is a comment, or NULL if no comment is
 * saved with the image. Consider the string returned to be read-only.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern const char *epeg_comment_get(Epeg_Image *im)
{
   return im->in.comment;
}

/**
 * Get thumbnail comments of loaded image.
 * @param im A handle to an opened Epeg image.
 * @param info Pointer to a thumbnail info struct to be filled in.
 * @return Nothing.
 *
 * This function retrieves thumbnail comments written by Epeg to any saved
 * JPEG files. If no thumbnail comments were saved, the fields will be 0 in
 * the @p info struct on return.
 *
 * See also: epeg_comment_get(), epeg_thumbnail_comments_enable()
 */
extern void epeg_thumbnail_comments_get(Epeg_Image *im,
                                        Epeg_Thumbnail_Info *info)
{
   if (!info) {
      return;
   }
   info->uri = im->in.thumb_info.uri;
   info->mtime = im->in.thumb_info.mtime;
   info->w = im->in.thumb_info.w;
   info->h = im->in.thumb_info.h;
   info->mimetype = im->in.thumb_info.mime;
}

/**
 * Set the comment field of the image for saving.
 * @param im A handle to an opened Epeg image.
 * @param comment The comment to set.
 * @return Nothing.
 *
 * Set the comment for the image file for when it gets saved. This is a NUL
 * byte terminated C string. If @p comment is NULL the output file will have
 * no comment field.
 *
 * The default comment will be any comment loaded from the input file.
 *
 * See also: epeg_comment_get()
 */
extern void epeg_comment_set(Epeg_Image *im, const char *comment)
{
   if (im->out.comment) {
      free(im->out.comment);
   }
   if (!comment) {
      im->out.comment = NULL;
   } else {
      im->out.comment = strdup(comment);
   }
}

/**
 * Set the encoding quality of the saved image.
 * @param im A handle to an opened Epeg image.
 * @param quality The quality of encoding from 0 to 100.
 * @return Nothing.
 *
 * Set the quality of the output encoded image. Values from 0 to 100
 * inclusive are valid, with 100 being the maximum quality, and 0 being the
 * minimum. If the quality is set equal to or above 90%, the output U and V
 * color planes are encoded at 1:1 with the Y plane.
 *
 * The default quality is 75.
 *
 * See also: epeg_comment_set()
 */
extern void epeg_quality_set(Epeg_Image *im, int quality)
{
   if (quality < 0) {
      quality = 0;
   } else if (quality > 100) {
      quality = 100;
   }
   im->out.quality = quality;
}

/**
 * Enable thumbnail comments in saved image.
 * @param im A handle to an opened Epeg image.
 * @param onoff A boolean on and off enabling flag.
 * @return Nothing.
 *
 * if @p onoff is 1, the output file will have thumbnail comments added to
 * it, and if it is 0, it will not. The default is 0.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern void epeg_thumbnail_comments_enable(Epeg_Image *im, int onoff)
{
   im->out.thumbnail_info = (char)onoff;
}

/**
 * Set the output file path for the image when saved.
 * @param im A handle to an opened Epeg image.
 * @param file The path to the output file.
 * @return Nothing.
 *
 * This sets the output file path name (either a full or relative path name)
 * to where the file will be written when saved. @p file must be a NUL
 * terminated C string containing the path to the file to be saved to. If it is
 * NULL, then the image will not be saved to a file when calling epeg_encode().
 *
 * See also: epeg_memory_output_set(), epeg_encode()
 */
extern void epeg_file_output_set(Epeg_Image *im, const char *file)
{
   if (im->out.file) {
      free(im->out.file);
   }
   if (!file) {
      im->out.file = NULL;
   } else {
      im->out.file = strdup(file);
   }
}

/**
 * Set the output file to be a block of allocated memory.
 * @param im A handle to an opened Epeg image.
 * @param data A pointer to a pointer to a memory block.
 * @param size A pointer to a counter of the size of the memory block.
 * @return Nothing.
 *
 * This sets the output encoding of the image when saved to be allocated
 * memory. After epeg_close() is called the pointer pointed to by @p data
 * and the integer pointed to by @p size will contain the pointer to the
 * memory block and its size in bytes, respecitvely. The memory block can be
 * freed with the free() function call. If the save fails the pointer to the
 * memory block will be unaffected, as will the size.
 *
 * See also: epeg_file_output_set(), epeg_encode()
 */
extern void epeg_memory_output_set(Epeg_Image *im, unsigned char **data,
                                   int *size)
{
   im->out.mem.data = data;
   im->out.mem.size = size;
}

/**
 * This saves the image to its specified destination.
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * This saves the image @p im to its destination specified by
 * epeg_file_output_set() or epeg_memory_output_set(). The image will be
 * encoded at the decoded pixel size, using the quality, comment,
 * and thumbnail comment settings set on the image.
 *
 * See also: epeg_file_output_set(), epeg_memory_output_set()
 */
extern int epeg_encode(Epeg_Image *im)
{
   if (_epeg_decode(im) != 0) {
      return 1;
   }
   if (_epeg_scale(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * FIXME: Document this with a short, sentence-long description of epeg_trim()
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * FIXME: Document this with a longer, paragraph-long description.
 */
extern int epeg_trim(Epeg_Image *im)
{
   if (_epeg_decode_for_trim(im) != 0) {
      return 1;
   }
   if (_epeg_trim(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * Close an image handle.
 * @param im A handle to an opened Epeg image.
 * @return Nothing.
 *
 * This closes an opened image handle and frees all memory associated with it.
 * It does NOT free encoded data generated by epeg_memory_output_set() followed
 * by epeg_encode(), nor does it guarantee to free any data received by
 * epeg_pixels_get(). Once an image handle is closed, consider it invalid.
 *
 * See also: epeg_file_open(), epeg_memory_open()
 */
extern void epeg_close(Epeg_Image *im)
{
   if (im->pixels) {
      free(im->pixels);
   }
   if (im->lines) {
      free(im->lines);
   }
   if (im->in.file) {
      free(im->in.file);
   }
   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if (im->in.f) {
      fclose(im->in.f);
   }
   if (im->in.comment) {
      free(im->in.comment);
   }
   if (im->in.thumb_info.uri) {
      free(im->in.thumb_info.uri);
   }
   if (im->in.thumb_info.mime) {
      free(im->in.thumb_info.mime);
   }
   if (im->out.file) {
      free(im->out.file);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if (im->out.f) {
      fclose(im->out.f);
   }
   if (im->out.comment) {
      free(im->out.comment);
   }
   free(im);
}

/* static internal private-only function; unnecessary to document: */
static Epeg_Image *_epeg_open_header(Epeg_Image *im)
{
   struct jpeg_marker_struct *m;

   im->in.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      error:
      epeg_close(im);
      im = NULL;
      return NULL;
   }

   jpeg_create_decompress(&(im->in.jinfo));
   jpeg_save_markers(&(im->in.jinfo), (JPEG_APP0 + 7), 1024);
   jpeg_save_markers(&(im->in.jinfo), JPEG_COM, 65535);
   jpeg_stdio_src(&(im->in.jinfo), im->in.f);
   jpeg_read_header(&(im->in.jinfo), TRUE);
   im->in.w = (int)im->in.jinfo.image_width;
   im->in.h = (int)im->in.jinfo.image_height;
   if (im->in.w <= 1) {
      goto error;
   }
   if (im->in.h <= 1) {
      goto error;
   }

   im->out.w = im->in.w;
   im->out.h = im->in.h;

   im->color_space = (((im->in.color_space = im->in.jinfo.out_color_space) == JCS_GRAYSCALE) ? EPEG_GRAY8 : EPEG_RGB8);
   if (im->in.color_space == JCS_CMYK) {
      im->color_space = EPEG_CMYK;
   }

   for ((m = im->in.jinfo.marker_list); m; (m = m->next)) {
      if (m->marker == JPEG_COM) {
         if (im->in.comment) {
            free(im->in.comment);
         }
         im->in.comment = (char *)malloc((size_t)(m->data_length + 1));
         if (im->in.comment) {
            memcpy(im->in.comment, m->data, (size_t)m->data_length);
            im->in.comment[m->data_length] = 0;
         }
      } else if (m->marker == (JPEG_APP0 + 7))  {
         if ((m->data_length > 7) &&
             (!strncmp((char *)m->data, "Thumb::", (size_t)7))) {
            char *p, *p2;

            p = (char *)malloc((size_t)(m->data_length + 1));
            if (p) {
               memcpy(p, m->data, (size_t)m->data_length);
               p[m->data_length] = 0;
               p2 = strchr(p, '\n');
               if (p2) {
                  p2[0] = 0;
                  if (!strcmp(p, "Thumb::URI")) {
                     im->in.thumb_info.uri = strdup(p2 + 1);
                  } else if (!strcmp(p, "Thumb::MTime")) {
                     sscanf((p2 + 1), "%llu", &(im->in.thumb_info.mtime));
                  } else if (!strcmp(p, "Thumb::Image::Width")) {
                     im->in.thumb_info.w = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Image::Height")) {
                     im->in.thumb_info.h = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Mimetype")) {
                     im->in.thumb_info.mime = strdup(p2 + 1);
                  }
               } /* end "if (p2)" */
               free(p);
            } /* end "if (p)" */
         }
      }
   } /* end for-loop */
   return im;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode(Epeg_Image *im)
{
   int scale, scalew, scaleh, y;

   if (im->pixels) {
      return 1;
   }

   scalew = (im->in.w / im->out.w);
   scaleh = (im->in.h / im->out.h);

   scale = scalew;
   if (scaleh < scalew) {
      scale = scaleh;
   }

   if (scale > 8) {
      scale = 8;
   } else if (scale < 1) {
      scale = 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = (unsigned int)scale;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_IFAST;

   switch (im->color_space) {
      case EPEG_GRAY8:
			 im->in.jinfo.out_color_space = JCS_GRAYSCALE;
			 im->in.jinfo.output_components = 1;
			 break;

      case EPEG_YUV8:
			 im->in.jinfo.out_color_space = JCS_YCbCr;
			 break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
			 im->in.jinfo.out_color_space = JCS_RGB;
			 break;

      case EPEG_CMYK:
			 im->in.jinfo.out_color_space = JCS_CMYK;
			 im->in.jinfo.output_components = 4;
			 break;

      default:
			 break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      epeg_close(im);
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
	   jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_scale(Epeg_Image *im)
{
   unsigned char *dst, *row, *src;
   int            x, y, w, h, i;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   for ((y = 0); (y < h); y++) {
      row = (im->pixels +
             ((((unsigned int)y * im->in.jinfo.output_height) / (unsigned int)h) * (unsigned int)im->in.jinfo.output_components * im->in.jinfo.output_width));
      dst = (im->pixels +
             ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));

      for ((x = 0); (x < im->out.w); x++) {
         src = (row +
                ((((unsigned int)x * im->in.jinfo.output_width) / (unsigned int)w) * (unsigned int)im->in.jinfo.output_components));

         for ((i = 0); (i < im->in.jinfo.output_components); i++) {
            dst[i] = src[i];
         } /* end inmost for-loop */

         dst += im->in.jinfo.output_components;
      } /* end inner for-loop */
   } /* end outer for-loop */
   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode_for_trim(Epeg_Image *im)
{
   int y;

   if (im->pixels) {
      return 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = 1;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_ISLOW;

   switch (im->color_space) {
      case EPEG_GRAY8:
		   im->in.jinfo.out_color_space = JCS_GRAYSCALE;
		   im->in.jinfo.output_components = 1;
		   break;

      case EPEG_YUV8:
		   im->in.jinfo.out_color_space = JCS_YCbCr;
		   break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
		   im->in.jinfo.out_color_space = JCS_RGB;
		   break;

      case EPEG_CMYK:
		   im->in.jinfo.out_color_space = JCS_CMYK;
		   im->in.jinfo.output_components = 4;
		   break;

      default:
		   break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
      im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
      jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_trim(Epeg_Image *im)
{
   int y, a, b, w, h;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   a = im->out.x;
   b = im->out.y;

   /* dummy condition to use value stored to 'w': */
   if (w == 0) {
	   ;
   }

   for ((y = 0); (y < h); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)((y + b) * im->in.jinfo.output_components) * im->in.jinfo.output_width)
                      + (a * im->in.jinfo.output_components));
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_encode(Epeg_Image *im)
{
   void  *data = NULL;
   size_t size = 0;

   if (im->out.f) {
      return 1;
   }

   if (im->out.file) {
      im->out.f = fopen(im->out.file, "wb");
   } else {
      im->out.f = _epeg_memfile_write_open(&data, &size);
   }
   if (!im->out.f) {
      im->error = 1;
      return 1;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_create_compress(&(im->out.jinfo));
   jpeg_stdio_dest(&(im->out.jinfo), im->out.f);
   im->out.jinfo.image_width = (JDIMENSION)im->out.w;
   im->out.jinfo.image_height = (JDIMENSION)im->out.h;
   im->out.jinfo.input_components = im->in.jinfo.output_components;
   im->out.jinfo.in_color_space = im->in.jinfo.out_color_space;
   im->out.jinfo.dct_method = JDCT_IFAST;
   im->out.jinfo.dct_method = im->in.jinfo.dct_method;
   jpeg_set_defaults(&(im->out.jinfo));
   jpeg_set_quality(&(im->out.jinfo), im->out.quality, TRUE);

   if (im->out.quality >= 90) {
      im->out.jinfo.comp_info[0].h_samp_factor = 1;
      im->out.jinfo.comp_info[0].v_samp_factor = 1;
      im->out.jinfo.comp_info[1].h_samp_factor = 1;
      im->out.jinfo.comp_info[1].v_samp_factor = 1;
      im->out.jinfo.comp_info[2].h_samp_factor = 1;
      im->out.jinfo.comp_info[2].v_samp_factor = 1;
   }
   jpeg_start_compress(&(im->out.jinfo), TRUE);

   if (im->out.comment) {
      jpeg_write_marker(&(im->out.jinfo), JPEG_COM,
                        (const JOCTET *)im->out.comment,
                        (unsigned int)strlen(im->out.comment));
   }

   if (im->out.thumbnail_info) {
      char buf[8192];

      if (im->in.file) {
         snprintf(buf, sizeof(buf), "Thumb::URI\nfile://%s", im->in.file);
         jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7),
                           (const JOCTET *)buf, (unsigned int)strlen(buf));
         snprintf(buf, sizeof(buf), "Thumb::MTime\n%llu",
#if defined(HAVE_UINTMAX_T) && !defined(__LP64__)
                  (uintmax_t)im->stat_info.st_mtime);
#else
               (unsigned long long int)im->stat_info.st_mtime);
#endif /* HAVE_UINTMAX_T && !__LP64__ */
      }
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Width\n%i", im->in.w);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Height\n%i", im->in.h);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Mimetype\nimage/jpeg");
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
   }

   while (im->out.jinfo.next_scanline < im->out.h) {
      jpeg_write_scanlines(&(im->out.jinfo),
                           &(im->lines[im->out.jinfo.next_scanline]), 1);
   }

   jpeg_finish_compress(&(im->out.jinfo));

   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if ((im->in.f) && (im->in.file)) {
      fclose(im->in.f);
   }
   if ((im->in.f) && (!im->in.file)) {
      _epeg_memfile_read_close(im->in.f);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if ((im->out.f) && (im->out.file)) {
      fclose(im->out.f);
   }
   if ((im->out.f) && (!im->out.file)) {
      _epeg_memfile_write_close(im->out.f);
   }
   im->in.f = NULL;
   im->out.f = NULL;

   if (im->out.mem.data) {
	   *(im->out.mem.data) = (unsigned char *)data;
   }
   if (im->out.mem.size) {
	   *(im->out.mem.size) = (int)size;
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static void _epeg_fatal_error_handler(j_common_ptr cinfo)
{
   emptr errmgr;

   errmgr = (emptr)cinfo->err;
   longjmp(errmgr->setjmp_buffer, 1);
   return;
}

/* silence '-Wunused-macros' warnings: */
#ifdef MAX
# undef MAX
#endif /* MAX */

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* epeg_memfile.c */
/* gets built into the libepeg library */

#include <stdio.h>
#include "Epeg.h"
#include "epeg_private.h"

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_read_open(void *data, size_t size)
{
#ifdef HAVE_FMEMOPEN
   return (FILE *)fmemopen(data, size, "r");
#else
   FILE *f;

   f = tmpfile();
   if (!f) {
      return NULL;
   }
   fwrite(data, size, (size_t)1L, f);
   rewind(f);
   return f;
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_read_close(FILE *f)
{
#ifdef HAVE_FMEMOPEN
   fclose(f);
#else
   /* FIXME: make these different halves of the conditional actually different
    * from one another */
   fclose(f);
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only struct and typedef; unnecessary to document: */
typedef struct _Eet_Memfile_Write_Info Eet_Memfile_Write_Info;
struct _Eet_Memfile_Write_Info
{
   FILE *f;
   void **data;
   size_t *size;
};

/* static internal private-only global variables; unnecessary to document: */
static int _epeg_memfile_info_alloc_num = 0;
static int _epeg_memfile_info_num = 0;
static Eet_Memfile_Write_Info *_epeg_memfile_info = NULL;

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_write_open(void **data, size_t *size)
{
#ifdef HAVE_OPEN_MEMSTREAM
   return open_memstream((char **)data, size);
#else
   FILE *f;

   _epeg_memfile_info_num++;
   if (_epeg_memfile_info_num > _epeg_memfile_info_alloc_num) {
      Eet_Memfile_Write_Info *tmp;

      _epeg_memfile_info_alloc_num += 16;
      tmp = (Eet_Memfile_Write_Info *)realloc(_epeg_memfile_info,
                                              ((unsigned long)_epeg_memfile_info_alloc_num *
                                               sizeof(Eet_Memfile_Write_Info)));
      if (!tmp) {
         _epeg_memfile_info_alloc_num -= 16;
         _epeg_memfile_info_num--;
         return NULL;
      }
      _epeg_memfile_info = tmp;
   }
   f = tmpfile();
   if (!f) {
      _epeg_memfile_info_num--;
      return NULL;
   }
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].f = f;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].data = data;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].size = size;
   return f;
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_write_close(FILE *f)
{
#ifdef HAVE_OPEN_MEMSTREAM
   fclose(f);
#else
   int i;

   for ((i = 0); (i < _epeg_memfile_info_num); i++) {
      if (_epeg_memfile_info[i].f == f) {
         int j;

         fseek(f, 0L, SEEK_END);
         (*(_epeg_memfile_info[i].size)) = (size_t)ftell(f);
         rewind(f);
         (*(_epeg_memfile_info[i].data)) = malloc(*(_epeg_memfile_info[i].size));
         if (!(*(_epeg_memfile_info[i].data))) {
            fclose(f);
            (*(_epeg_memfile_info[i].size)) = 0;
            return;
         }
         fread((*(_epeg_memfile_info[i].data)), (*(_epeg_memfile_info[i].size)),
               (size_t)1L, f);
         for ((j = (i + 1)); (j < _epeg_memfile_info_num); j++) {
            _epeg_memfile_info[(j - 1)] = _epeg_memfile_info[j];
         }
         _epeg_memfile_info_num--;
         fclose(f);
         return;
      } /* end "if" */
   } /* end for-loop */
   fclose(f);
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* epeg_main.c for src/lib for epeg */
/* gets built into the libepeg library */

#include <stdio.h>
#include <stdlib.h>
#include "Epeg.h"
#include "epeg_private.h"

static Epeg_Image*_epeg_open_header(Epeg_Image *im);
static int _epeg_decode(Epeg_Image *im);
static int _epeg_scale(Epeg_Image *im);
static int _epeg_decode_for_trim(Epeg_Image *im);
static int _epeg_trim(Epeg_Image *im);
static int _epeg_encode(Epeg_Image *im);

static void _epeg_fatal_error_handler(j_common_ptr cinfo);

#ifndef MIN
# define MIN(__x,__y) ((__x) < (__y) ? (__x) : (__y))
#endif /* !MIN */
#ifndef MAX
# define MAX(__x,__y) ((__x) > (__y) ? (__x) : (__y))
#endif /* !MAX */

/**
 * Open a JPEG image by filename.
 * @param file The file path to open.
 * @return A handle to the opened JPEG file, with the header decoded.
 *
 * This function opens the file indicated by the @p file parameter, and
 * attempts to decode it as a jpeg file. If this failes, NULL is returned.
 * Otherwise a valid handle to an open JPEG file is returned that can be used
 * by other Epeg calls.
 *
 * The @p file must be a pointer to a valid C string, NUL (0 byte) terminated
 * thats is a relative or absolute file path. If not results are not
 * determined.
 *
 * See also: epeg_memory_open(), epeg_close()
 */
extern Epeg_Image *epeg_file_open(const char *file)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.file = strdup(file);
   im->in.f = fopen(im->in.file, "rb");
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   fstat(fileno(im->in.f), &(im->stat_info));
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Open a JPEG image stored in memory.
 * @param data A pointer to the memory containing the JPEG data.
 * @param size The size of the memory segment containing the JPEG.
 * @return  A handle to the opened JPEG, with the header decoded.
 *
 * This function opens a JPEG file that is stored in memory pointed to by
 * @p data, and that is @p size bytes in size. If successful a valid handle
 * is returned, or on failure NULL is returned.
 *
 * See also: epeg_file_open(), epeg_close()
 */
extern Epeg_Image *epeg_memory_open(unsigned char *data, int size)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.f = _epeg_memfile_read_open(data, (size_t)size);
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Return the original JPEG pixel size.
 * @param im A handle to an opened Epeg image.
 * @param w A pointer to the width value in pixels to be filled in.
 * @param h A pointer to the height value in pixels to be filled in.
 * @return Nothing.
 *
 * Returns the image size in pixels (well not really).
 *
 * See also: epeg_colorspace_get()
 */
extern void epeg_size_get(Epeg_Image *im, int *w, int *h)
{
	if (w) {
		*w = im->in.w;
	}
	if (h) {
		*h = im->in.h;
	}
}

/**
 * Return the original JPEG pixel color space.
 * @param im A handle to an opened Epeg image.
 * @param space A pointer to the color space value to be filled in.
 * @return Nothing.
 *
 * Returns the image color space (not yet though).
 *
 * See also: epeg_size_get()
 */
extern void epeg_colorspace_get(Epeg_Image *im, int *space)
{
	if (space) {
		*space = im->color_space;
	}
}

/**
 * Set the size of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param w The width of the image to decode at, in pixels.
 * @param h The height of the image to decode at, in pixels.
 * @return Nothing.
 *
 * Sets the size at which to decode the JPEG image, giving an optimized load
 * that only decodes the pixels needed.
 *
 * See also: epeg_decode_bounds_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_size_set(Epeg_Image *im, int w, int h)
{
   if (im->pixels) {
	   return;
   }
   if (w < 1) {
	   w = 1;
   } else if (w > im->in.w) {
	   w = im->in.w;
   }
   if (h < 1) {
	   h = 1;
   } else if (h > im->in.h) {
	   h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   im->out.x = 0;
   im->out.y = 0;
}

/**
 * Set the bounds of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param x Boundary X
 * @param y Boundary Y
 * @param w Boundary W
 * @param h Boundary H
 * @return Nothing.
 *
 * Sets the bounds inside which to decode the JPEG image,
 * giving an optimized load that only decodes the bounded pixels.
 * (???)
 *
 * See also: epeg_decode_size_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_bounds_set(Epeg_Image *im, int x, int y, int w, int h)
{
   if (im->pixels) {
      return;
   }
   if (w < 1) {
      w = 1;
   } else if (w > im->in.w) {
      w = im->in.w;
   }
   if (h < 1) {
      h = 1;
   } else if (h > im->in.h) {
      h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   if (x < 0) {
      x = 0;
   }
   if (y < 0) {
      y = 0;
   }
   im->out.x = x;
   im->out.y = y;
}

/**
 * Set the colorspace in which to decode the image.
 * @param im A handle to an opened Epeg image.
 * @param colorspace The colorspace to decode the image in.
 * @return Nothing.
 *
 * This sets the colorspace to decode the image in. The default is EPEG_YUV8,
 * as this is normally the native colorspace of a JPEG file, avoiding any
 * colorspace conversions for a faster load and/or save.
 *
 * See also: epeg_decode_size_set(), epeg_decode_bounds_set()
 */
extern void epeg_decode_colorspace_set(Epeg_Image *im,
                                       Epeg_Colorspace colorspace)
{
	if (im->pixels) {
		return;
	}
	if ((colorspace < EPEG_GRAY8) || (colorspace > EPEG_ARGB32)) {
		return;
	}
	im->color_space = colorspace;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get_as_RGB8()
 */
extern const void *epeg_pixels_get(Epeg_Image *im, int x, int y,  int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
         return NULL;
      }
   }

   if (!im->pixels) {
      return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* go through all 8 values in type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace): */
   if (im->color_space == EPEG_GRAY8) {
      /* this whole block is pretty much the same thing we do for each
       * of the other enumeration values as well... could probably be simplified
       * with a function-like macro or some other sort of meta-programming: */
      unsigned char *pix, *p;

      /* '1': */
      pix = (unsigned char *)malloc((size_t)(w * h * 1L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = s[0];
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_YUV8) {
      unsigned char *pix, *p;

      /* '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGR8) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* reverse order: */
            p[0] = s[2];
            p[1] = s[1];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGBA8) {
      unsigned char *pix, *p;

      /* '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGRA8) {
      unsigned char *pix, *p;

      /* also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = 0xff;
            /* reverse order: */
            p[1] = s[2];
            p[2] = s[1];
            p[3] = s[0];
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_ARGB32) {
      unsigned int *pix, *p;

      /* still also '4': */
      pix = (unsigned int *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (0xff000000 | (unsigned int)(s[0] << 16) | (unsigned int)(s[1] << 8) | (s[2]));
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* another thing that is also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* back to same order again: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } /* end "if-else" that seems like it should really be a "switch" */
   return NULL;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get()
 */
extern const void *epeg_pixels_get_as_RGB8(Epeg_Image *im, int x, int y,
                                           int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
		   return NULL;
	   }
   }

   if (!im->pixels) {
	   return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* unlike the "non-'_as_RGB8'-version" of this function,
    * there are no 'else'-es separating the cases here. Also, we only go through
    * three of the enumeration values of type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace) here: */
   if (im->color_space == EPEG_GRAY8) {
      unsigned char *pix, *p;

      /* get right into the '3' ones: */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[0];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (unsigned char)(MIN(255, (s[0] * s[3]) / 255));
            p[1] = (unsigned char)(MIN(255, (s[1] * s[3]) / 255));
            p[2] = (unsigned char)(MIN(255, (s[2] * s[3]) / 255));
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   return NULL;
}

/**
 * Free requested pixel block from an image.
 * @param im A handle to an opened Epeg image (unused).
 * @param data The pointer to the image pixels.
 * @return Nothing.
 *
 * This frees the data for a block of pixels requested from image @p im.
 * @p data must be a valid (non NULL) pointer to a pixel block taken from the
 * image @p im by epeg_pixels_get() and must be called before the image is
 * closed by epeg_close().
 */
extern void epeg_pixels_free(Epeg_Image *im, const void *data)
{
#pragma unused (im)
   free((void *)data);
}

/**
 * Get the image comment field as a string.
 * @param im A handle to an opened Epeg image.
 * @return A pointer to the loaded image comments.
 *
 * This function returns the comment field as a string (NUL byte terminated)
 * of the loaded image @p im, if there is a comment, or NULL if no comment is
 * saved with the image. Consider the string returned to be read-only.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern const char *epeg_comment_get(Epeg_Image *im)
{
   return im->in.comment;
}

/**
 * Get thumbnail comments of loaded image.
 * @param im A handle to an opened Epeg image.
 * @param info Pointer to a thumbnail info struct to be filled in.
 * @return Nothing.
 *
 * This function retrieves thumbnail comments written by Epeg to any saved
 * JPEG files. If no thumbnail comments were saved, the fields will be 0 in
 * the @p info struct on return.
 *
 * See also: epeg_comment_get(), epeg_thumbnail_comments_enable()
 */
extern void epeg_thumbnail_comments_get(Epeg_Image *im,
                                        Epeg_Thumbnail_Info *info)
{
   if (!info) {
      return;
   }
   info->uri = im->in.thumb_info.uri;
   info->mtime = im->in.thumb_info.mtime;
   info->w = im->in.thumb_info.w;
   info->h = im->in.thumb_info.h;
   info->mimetype = im->in.thumb_info.mime;
}

/**
 * Set the comment field of the image for saving.
 * @param im A handle to an opened Epeg image.
 * @param comment The comment to set.
 * @return Nothing.
 *
 * Set the comment for the image file for when it gets saved. This is a NUL
 * byte terminated C string. If @p comment is NULL the output file will have
 * no comment field.
 *
 * The default comment will be any comment loaded from the input file.
 *
 * See also: epeg_comment_get()
 */
extern void epeg_comment_set(Epeg_Image *im, const char *comment)
{
   if (im->out.comment) {
      free(im->out.comment);
   }
   if (!comment) {
      im->out.comment = NULL;
   } else {
      im->out.comment = strdup(comment);
   }
}

/**
 * Set the encoding quality of the saved image.
 * @param im A handle to an opened Epeg image.
 * @param quality The quality of encoding from 0 to 100.
 * @return Nothing.
 *
 * Set the quality of the output encoded image. Values from 0 to 100
 * inclusive are valid, with 100 being the maximum quality, and 0 being the
 * minimum. If the quality is set equal to or above 90%, the output U and V
 * color planes are encoded at 1:1 with the Y plane.
 *
 * The default quality is 75.
 *
 * See also: epeg_comment_set()
 */
extern void epeg_quality_set(Epeg_Image *im, int quality)
{
   if (quality < 0) {
      quality = 0;
   } else if (quality > 100) {
      quality = 100;
   }
   im->out.quality = quality;
}

/**
 * Enable thumbnail comments in saved image.
 * @param im A handle to an opened Epeg image.
 * @param onoff A boolean on and off enabling flag.
 * @return Nothing.
 *
 * if @p onoff is 1, the output file will have thumbnail comments added to
 * it, and if it is 0, it will not. The default is 0.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern void epeg_thumbnail_comments_enable(Epeg_Image *im, int onoff)
{
   im->out.thumbnail_info = (char)onoff;
}

/**
 * Set the output file path for the image when saved.
 * @param im A handle to an opened Epeg image.
 * @param file The path to the output file.
 * @return Nothing.
 *
 * This sets the output file path name (either a full or relative path name)
 * to where the file will be written when saved. @p file must be a NUL
 * terminated C string containing the path to the file to be saved to. If it is
 * NULL, then the image will not be saved to a file when calling epeg_encode().
 *
 * See also: epeg_memory_output_set(), epeg_encode()
 */
extern void epeg_file_output_set(Epeg_Image *im, const char *file)
{
   if (im->out.file) {
      free(im->out.file);
   }
   if (!file) {
      im->out.file = NULL;
   } else {
      im->out.file = strdup(file);
   }
}

/**
 * Set the output file to be a block of allocated memory.
 * @param im A handle to an opened Epeg image.
 * @param data A pointer to a pointer to a memory block.
 * @param size A pointer to a counter of the size of the memory block.
 * @return Nothing.
 *
 * This sets the output encoding of the image when saved to be allocated
 * memory. After epeg_close() is called the pointer pointed to by @p data
 * and the integer pointed to by @p size will contain the pointer to the
 * memory block and its size in bytes, respecitvely. The memory block can be
 * freed with the free() function call. If the save fails the pointer to the
 * memory block will be unaffected, as will the size.
 *
 * See also: epeg_file_output_set(), epeg_encode()
 */
extern void epeg_memory_output_set(Epeg_Image *im, unsigned char **data,
                                   int *size)
{
   im->out.mem.data = data;
   im->out.mem.size = size;
}

/**
 * This saves the image to its specified destination.
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * This saves the image @p im to its destination specified by
 * epeg_file_output_set() or epeg_memory_output_set(). The image will be
 * encoded at the decoded pixel size, using the quality, comment,
 * and thumbnail comment settings set on the image.
 *
 * See also: epeg_file_output_set(), epeg_memory_output_set()
 */
extern int epeg_encode(Epeg_Image *im)
{
   if (_epeg_decode(im) != 0) {
      return 1;
   }
   if (_epeg_scale(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * FIXME: Document this with a short, sentence-long description of epeg_trim()
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * FIXME: Document this with a longer, paragraph-long description.
 */
extern int epeg_trim(Epeg_Image *im)
{
   if (_epeg_decode_for_trim(im) != 0) {
      return 1;
   }
   if (_epeg_trim(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * Close an image handle.
 * @param im A handle to an opened Epeg image.
 * @return Nothing.
 *
 * This closes an opened image handle and frees all memory associated with it.
 * It does NOT free encoded data generated by epeg_memory_output_set() followed
 * by epeg_encode(), nor does it guarantee to free any data received by
 * epeg_pixels_get(). Once an image handle is closed, consider it invalid.
 *
 * See also: epeg_file_open(), epeg_memory_open()
 */
extern void epeg_close(Epeg_Image *im)
{
   if (im->pixels) {
      free(im->pixels);
   }
   if (im->lines) {
      free(im->lines);
   }
   if (im->in.file) {
      free(im->in.file);
   }
   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if (im->in.f) {
      fclose(im->in.f);
   }
   if (im->in.comment) {
      free(im->in.comment);
   }
   if (im->in.thumb_info.uri) {
      free(im->in.thumb_info.uri);
   }
   if (im->in.thumb_info.mime) {
      free(im->in.thumb_info.mime);
   }
   if (im->out.file) {
      free(im->out.file);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if (im->out.f) {
      fclose(im->out.f);
   }
   if (im->out.comment) {
      free(im->out.comment);
   }
   free(im);
}

/* static internal private-only function; unnecessary to document: */
static Epeg_Image *_epeg_open_header(Epeg_Image *im)
{
   struct jpeg_marker_struct *m;

   im->in.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      error:
      epeg_close(im);
      im = NULL;
      return NULL;
   }

   jpeg_create_decompress(&(im->in.jinfo));
   jpeg_save_markers(&(im->in.jinfo), (JPEG_APP0 + 7), 1024);
   jpeg_save_markers(&(im->in.jinfo), JPEG_COM, 65535);
   jpeg_stdio_src(&(im->in.jinfo), im->in.f);
   jpeg_read_header(&(im->in.jinfo), TRUE);
   im->in.w = (int)im->in.jinfo.image_width;
   im->in.h = (int)im->in.jinfo.image_height;
   if (im->in.w <= 1) {
      goto error;
   }
   if (im->in.h <= 1) {
      goto error;
   }

   im->out.w = im->in.w;
   im->out.h = im->in.h;

   im->color_space = (((im->in.color_space = im->in.jinfo.out_color_space) == JCS_GRAYSCALE) ? EPEG_GRAY8 : EPEG_RGB8);
   if (im->in.color_space == JCS_CMYK) {
      im->color_space = EPEG_CMYK;
   }

   for ((m = im->in.jinfo.marker_list); m; (m = m->next)) {
      if (m->marker == JPEG_COM) {
         if (im->in.comment) {
            free(im->in.comment);
         }
         im->in.comment = (char *)malloc((size_t)(m->data_length + 1));
         if (im->in.comment) {
            memcpy(im->in.comment, m->data, (size_t)m->data_length);
            im->in.comment[m->data_length] = 0;
         }
      } else if (m->marker == (JPEG_APP0 + 7))  {
         if ((m->data_length > 7) &&
             (!strncmp((char *)m->data, "Thumb::", (size_t)7))) {
            char *p, *p2;

            p = (char *)malloc((size_t)(m->data_length + 1));
            if (p) {
               memcpy(p, m->data, (size_t)m->data_length);
               p[m->data_length] = 0;
               p2 = strchr(p, '\n');
               if (p2) {
                  p2[0] = 0;
                  if (!strcmp(p, "Thumb::URI")) {
                     im->in.thumb_info.uri = strdup(p2 + 1);
                  } else if (!strcmp(p, "Thumb::MTime")) {
                     sscanf((p2 + 1), "%llu", &(im->in.thumb_info.mtime));
                  } else if (!strcmp(p, "Thumb::Image::Width")) {
                     im->in.thumb_info.w = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Image::Height")) {
                     im->in.thumb_info.h = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Mimetype")) {
                     im->in.thumb_info.mime = strdup(p2 + 1);
                  }
               } /* end "if (p2)" */
               free(p);
            } /* end "if (p)" */
         }
      }
   } /* end for-loop */
   return im;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode(Epeg_Image *im)
{
   int scale, scalew, scaleh, y;

   if (im->pixels) {
      return 1;
   }

   scalew = (im->in.w / im->out.w);
   scaleh = (im->in.h / im->out.h);

   scale = scalew;
   if (scaleh < scalew) {
      scale = scaleh;
   }

   if (scale > 8) {
      scale = 8;
   } else if (scale < 1) {
      scale = 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = (unsigned int)scale;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_IFAST;

   switch (im->color_space) {
      case EPEG_GRAY8:
			 im->in.jinfo.out_color_space = JCS_GRAYSCALE;
			 im->in.jinfo.output_components = 1;
			 break;

      case EPEG_YUV8:
			 im->in.jinfo.out_color_space = JCS_YCbCr;
			 break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
			 im->in.jinfo.out_color_space = JCS_RGB;
			 break;

      case EPEG_CMYK:
			 im->in.jinfo.out_color_space = JCS_CMYK;
			 im->in.jinfo.output_components = 4;
			 break;

      default:
			 break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      epeg_close(im);
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
	   jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_scale(Epeg_Image *im)
{
   unsigned char *dst, *row, *src;
   int            x, y, w, h, i;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   for ((y = 0); (y < h); y++) {
      row = (im->pixels +
             ((((unsigned int)y * im->in.jinfo.output_height) / (unsigned int)h) * (unsigned int)im->in.jinfo.output_components * im->in.jinfo.output_width));
      dst = (im->pixels +
             ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));

      for ((x = 0); (x < im->out.w); x++) {
         src = (row +
                ((((unsigned int)x * im->in.jinfo.output_width) / (unsigned int)w) * (unsigned int)im->in.jinfo.output_components));

         for ((i = 0); (i < im->in.jinfo.output_components); i++) {
            dst[i] = src[i];
         } /* end inmost for-loop */

         dst += im->in.jinfo.output_components;
      } /* end inner for-loop */
   } /* end outer for-loop */
   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode_for_trim(Epeg_Image *im)
{
   int y;

   if (im->pixels) {
      return 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = 1;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_ISLOW;

   switch (im->color_space) {
      case EPEG_GRAY8:
		   im->in.jinfo.out_color_space = JCS_GRAYSCALE;
		   im->in.jinfo.output_components = 1;
		   break;

      case EPEG_YUV8:
		   im->in.jinfo.out_color_space = JCS_YCbCr;
		   break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
		   im->in.jinfo.out_color_space = JCS_RGB;
		   break;

      case EPEG_CMYK:
		   im->in.jinfo.out_color_space = JCS_CMYK;
		   im->in.jinfo.output_components = 4;
		   break;

      default:
		   break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
      im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
      jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_trim(Epeg_Image *im)
{
   int y, a, b, w, h;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   a = im->out.x;
   b = im->out.y;

   /* dummy condition to use value stored to 'w': */
   if (w == 0) {
	   ;
   }

   for ((y = 0); (y < h); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)((y + b) * im->in.jinfo.output_components) * im->in.jinfo.output_width)
                      + (a * im->in.jinfo.output_components));
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_encode(Epeg_Image *im)
{
   void  *data = NULL;
   size_t size = 0;

   if (im->out.f) {
      return 1;
   }

   if (im->out.file) {
      im->out.f = fopen(im->out.file, "wb");
   } else {
      im->out.f = _epeg_memfile_write_open(&data, &size);
   }
   if (!im->out.f) {
      im->error = 1;
      return 1;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_create_compress(&(im->out.jinfo));
   jpeg_stdio_dest(&(im->out.jinfo), im->out.f);
   im->out.jinfo.image_width = (JDIMENSION)im->out.w;
   im->out.jinfo.image_height = (JDIMENSION)im->out.h;
   im->out.jinfo.input_components = im->in.jinfo.output_components;
   im->out.jinfo.in_color_space = im->in.jinfo.out_color_space;
   im->out.jinfo.dct_method = JDCT_IFAST;
   im->out.jinfo.dct_method = im->in.jinfo.dct_method;
   jpeg_set_defaults(&(im->out.jinfo));
   jpeg_set_quality(&(im->out.jinfo), im->out.quality, TRUE);

   if (im->out.quality >= 90) {
      im->out.jinfo.comp_info[0].h_samp_factor = 1;
      im->out.jinfo.comp_info[0].v_samp_factor = 1;
      im->out.jinfo.comp_info[1].h_samp_factor = 1;
      im->out.jinfo.comp_info[1].v_samp_factor = 1;
      im->out.jinfo.comp_info[2].h_samp_factor = 1;
      im->out.jinfo.comp_info[2].v_samp_factor = 1;
   }
   jpeg_start_compress(&(im->out.jinfo), TRUE);

   if (im->out.comment) {
      jpeg_write_marker(&(im->out.jinfo), JPEG_COM,
                        (const JOCTET *)im->out.comment,
                        (unsigned int)strlen(im->out.comment));
   }

   if (im->out.thumbnail_info) {
      char buf[8192];

      if (im->in.file) {
         snprintf(buf, sizeof(buf), "Thumb::URI\nfile://%s", im->in.file);
         jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7),
                           (const JOCTET *)buf, (unsigned int)strlen(buf));
         snprintf(buf, sizeof(buf), "Thumb::MTime\n%llu",
#if defined(HAVE_UINTMAX_T) && !defined(__LP64__)
                  (uintmax_t)im->stat_info.st_mtime);
#else
               (unsigned long long int)im->stat_info.st_mtime);
#endif /* HAVE_UINTMAX_T && !__LP64__ */
      }
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Width\n%i", im->in.w);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Height\n%i", im->in.h);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Mimetype\nimage/jpeg");
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
   }

   while (im->out.jinfo.next_scanline < im->out.h) {
      jpeg_write_scanlines(&(im->out.jinfo),
                           &(im->lines[im->out.jinfo.next_scanline]), 1);
   }

   jpeg_finish_compress(&(im->out.jinfo));

   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if ((im->in.f) && (im->in.file)) {
      fclose(im->in.f);
   }
   if ((im->in.f) && (!im->in.file)) {
      _epeg_memfile_read_close(im->in.f);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if ((im->out.f) && (im->out.file)) {
      fclose(im->out.f);
   }
   if ((im->out.f) && (!im->out.file)) {
      _epeg_memfile_write_close(im->out.f);
   }
   im->in.f = NULL;
   im->out.f = NULL;

   if (im->out.mem.data) {
	   *(im->out.mem.data) = (unsigned char *)data;
   }
   if (im->out.mem.size) {
	   *(im->out.mem.size) = (int)size;
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static void _epeg_fatal_error_handler(j_common_ptr cinfo)
{
   emptr errmgr;

   errmgr = (emptr)cinfo->err;
   longjmp(errmgr->setjmp_buffer, 1);
   return;
}

/* silence '-Wunused-macros' warnings: */
#ifdef MAX
# undef MAX
#endif /* MAX */

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* epeg_memfile.c */
/* gets built into the libepeg library */

#include <stdio.h>
#include "Epeg.h"
#include "epeg_private.h"

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_read_open(void *data, size_t size)
{
#ifdef HAVE_FMEMOPEN
   return (FILE *)fmemopen(data, size, "r");
#else
   FILE *f;

   f = tmpfile();
   if (!f) {
      return NULL;
   }
   fwrite(data, size, (size_t)1L, f);
   rewind(f);
   return f;
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_read_close(FILE *f)
{
#ifdef HAVE_FMEMOPEN
   fclose(f);
#else
   /* FIXME: make these different halves of the conditional actually different
    * from one another */
   fclose(f);
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only struct and typedef; unnecessary to document: */
typedef struct _Eet_Memfile_Write_Info Eet_Memfile_Write_Info;
struct _Eet_Memfile_Write_Info
{
   FILE *f;
   void **data;
   size_t *size;
};

/* static internal private-only global variables; unnecessary to document: */
static int _epeg_memfile_info_alloc_num = 0;
static int _epeg_memfile_info_num = 0;
static Eet_Memfile_Write_Info *_epeg_memfile_info = NULL;

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_write_open(void **data, size_t *size)
{
#ifdef HAVE_OPEN_MEMSTREAM
   return open_memstream((char **)data, size);
#else
   FILE *f;

   _epeg_memfile_info_num++;
   if (_epeg_memfile_info_num > _epeg_memfile_info_alloc_num) {
      Eet_Memfile_Write_Info *tmp;

      _epeg_memfile_info_alloc_num += 16;
      tmp = (Eet_Memfile_Write_Info *)realloc(_epeg_memfile_info,
                                              ((unsigned long)_epeg_memfile_info_alloc_num *
                                               sizeof(Eet_Memfile_Write_Info)));
      if (!tmp) {
         _epeg_memfile_info_alloc_num -= 16;
         _epeg_memfile_info_num--;
         return NULL;
      }
      _epeg_memfile_info = tmp;
   }
   f = tmpfile();
   if (!f) {
      _epeg_memfile_info_num--;
      return NULL;
   }
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].f = f;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].data = data;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].size = size;
   return f;
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_write_close(FILE *f)
{
#ifdef HAVE_OPEN_MEMSTREAM
   fclose(f);
#else
   int i;

   for ((i = 0); (i < _epeg_memfile_info_num); i++) {
      if (_epeg_memfile_info[i].f == f) {
         int j;

         fseek(f, 0L, SEEK_END);
         (*(_epeg_memfile_info[i].size)) = (size_t)ftell(f);
         rewind(f);
         (*(_epeg_memfile_info[i].data)) = malloc(*(_epeg_memfile_info[i].size));
         if (!(*(_epeg_memfile_info[i].data))) {
            fclose(f);
            (*(_epeg_memfile_info[i].size)) = 0;
            return;
         }
         fread((*(_epeg_memfile_info[i].data)), (*(_epeg_memfile_info[i].size)),
               (size_t)1L, f);
         for ((j = (i + 1)); (j < _epeg_memfile_info_num); j++) {
            _epeg_memfile_info[(j - 1)] = _epeg_memfile_info[j];
         }
         _epeg_memfile_info_num--;
         fclose(f);
         return;
      } /* end "if" */
   } /* end for-loop */
   fclose(f);
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_main.c': */
/* epeg_main.c for src/lib for epeg */
/* gets built into the libepeg library */

#include <stdio.h>
#include <stdlib.h>
#include "Epeg.h"
#include "epeg_private.h"

static Epeg_Image*_epeg_open_header(Epeg_Image *im);
static int _epeg_decode(Epeg_Image *im);
static int _epeg_scale(Epeg_Image *im);
static int _epeg_decode_for_trim(Epeg_Image *im);
static int _epeg_trim(Epeg_Image *im);
static int _epeg_encode(Epeg_Image *im);

static void _epeg_fatal_error_handler(j_common_ptr cinfo);

#ifndef MIN
# define MIN(__x,__y) ((__x) < (__y) ? (__x) : (__y))
#endif /* !MIN */
#ifndef MAX
# define MAX(__x,__y) ((__x) > (__y) ? (__x) : (__y))
#endif /* !MAX */

/**
 * Open a JPEG image by filename.
 * @param file The file path to open.
 * @return A handle to the opened JPEG file, with the header decoded.
 *
 * This function opens the file indicated by the @p file parameter, and
 * attempts to decode it as a jpeg file. If this failes, NULL is returned.
 * Otherwise a valid handle to an open JPEG file is returned that can be used
 * by other Epeg calls.
 *
 * The @p file must be a pointer to a valid C string, NUL (0 byte) terminated
 * thats is a relative or absolute file path. If not results are not
 * determined.
 *
 * See also: epeg_memory_open(), epeg_close()
 */
extern Epeg_Image *epeg_file_open(const char *file)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.file = strdup(file);
   im->in.f = fopen(im->in.file, "rb");
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   fstat(fileno(im->in.f), &(im->stat_info));
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Open a JPEG image stored in memory.
 * @param data A pointer to the memory containing the JPEG data.
 * @param size The size of the memory segment containing the JPEG.
 * @return  A handle to the opened JPEG, with the header decoded.
 *
 * This function opens a JPEG file that is stored in memory pointed to by
 * @p data, and that is @p size bytes in size. If successful a valid handle
 * is returned, or on failure NULL is returned.
 *
 * See also: epeg_file_open(), epeg_close()
 */
extern Epeg_Image *epeg_memory_open(unsigned char *data, int size)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.f = _epeg_memfile_read_open(data, (size_t)size);
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Return the original JPEG pixel size.
 * @param im A handle to an opened Epeg image.
 * @param w A pointer to the width value in pixels to be filled in.
 * @param h A pointer to the height value in pixels to be filled in.
 * @return Nothing.
 *
 * Returns the image size in pixels (well not really).
 *
 * See also: epeg_colorspace_get()
 */
extern void epeg_size_get(Epeg_Image *im, int *w, int *h)
{
	if (w) {
		*w = im->in.w;
	}
	if (h) {
		*h = im->in.h;
	}
}

/**
 * Return the original JPEG pixel color space.
 * @param im A handle to an opened Epeg image.
 * @param space A pointer to the color space value to be filled in.
 * @return Nothing.
 *
 * Returns the image color space (not yet though).
 *
 * See also: epeg_size_get()
 */
extern void epeg_colorspace_get(Epeg_Image *im, int *space)
{
	if (space) {
		*space = im->color_space;
	}
}

/**
 * Set the size of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param w The width of the image to decode at, in pixels.
 * @param h The height of the image to decode at, in pixels.
 * @return Nothing.
 *
 * Sets the size at which to decode the JPEG image, giving an optimized load
 * that only decodes the pixels needed.
 *
 * See also: epeg_decode_bounds_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_size_set(Epeg_Image *im, int w, int h)
{
   if (im->pixels) {
	   return;
   }
   if (w < 1) {
	   w = 1;
   } else if (w > im->in.w) {
	   w = im->in.w;
   }
   if (h < 1) {
	   h = 1;
   } else if (h > im->in.h) {
	   h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   im->out.x = 0;
   im->out.y = 0;
}

/**
 * Set the bounds of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param x Boundary X
 * @param y Boundary Y
 * @param w Boundary W
 * @param h Boundary H
 * @return Nothing.
 *
 * Sets the bounds inside which to decode the JPEG image,
 * giving an optimized load that only decodes the bounded pixels.
 * (???)
 *
 * See also: epeg_decode_size_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_bounds_set(Epeg_Image *im, int x, int y, int w, int h)
{
   if (im->pixels) {
      return;
   }
   if (w < 1) {
      w = 1;
   } else if (w > im->in.w) {
      w = im->in.w;
   }
   if (h < 1) {
      h = 1;
   } else if (h > im->in.h) {
      h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   if (x < 0) {
      x = 0;
   }
   if (y < 0) {
      y = 0;
   }
   im->out.x = x;
   im->out.y = y;
}

/**
 * Set the colorspace in which to decode the image.
 * @param im A handle to an opened Epeg image.
 * @param colorspace The colorspace to decode the image in.
 * @return Nothing.
 *
 * This sets the colorspace to decode the image in. The default is EPEG_YUV8,
 * as this is normally the native colorspace of a JPEG file, avoiding any
 * colorspace conversions for a faster load and/or save.
 *
 * See also: epeg_decode_size_set(), epeg_decode_bounds_set()
 */
extern void epeg_decode_colorspace_set(Epeg_Image *im,
                                       Epeg_Colorspace colorspace)
{
	if (im->pixels) {
		return;
	}
	if ((colorspace < EPEG_GRAY8) || (colorspace > EPEG_ARGB32)) {
		return;
	}
	im->color_space = colorspace;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get_as_RGB8()
 */
extern const void *epeg_pixels_get(Epeg_Image *im, int x, int y,  int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
         return NULL;
      }
   }

   if (!im->pixels) {
      return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* go through all 8 values in type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace): */
   if (im->color_space == EPEG_GRAY8) {
      /* this whole block is pretty much the same thing we do for each
       * of the other enumeration values as well... could probably be simplified
       * with a function-like macro or some other sort of meta-programming: */
      unsigned char *pix, *p;

      /* '1': */
      pix = (unsigned char *)malloc((size_t)(w * h * 1L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = s[0];
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_YUV8) {
      unsigned char *pix, *p;

      /* '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGR8) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* reverse order: */
            p[0] = s[2];
            p[1] = s[1];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGBA8) {
      unsigned char *pix, *p;

      /* '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGRA8) {
      unsigned char *pix, *p;

      /* also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = 0xff;
            /* reverse order: */
            p[1] = s[2];
            p[2] = s[1];
            p[3] = s[0];
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_ARGB32) {
      unsigned int *pix, *p;

      /* still also '4': */
      pix = (unsigned int *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (0xff000000 | (unsigned int)(s[0] << 16) | (unsigned int)(s[1] << 8) | (s[2]));
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* another thing that is also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* back to same order again: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } /* end "if-else" that seems like it should really be a "switch" */
   return NULL;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get()
 */
extern const void *epeg_pixels_get_as_RGB8(Epeg_Image *im, int x, int y,
                                           int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
		   return NULL;
	   }
   }

   if (!im->pixels) {
	   return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* unlike the "non-'_as_RGB8'-version" of this function,
    * there are no 'else'-es separating the cases here. Also, we only go through
    * three of the enumeration values of type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace) here: */
   if (im->color_space == EPEG_GRAY8) {
      unsigned char *pix, *p;

      /* get right into the '3' ones: */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[0];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (unsigned char)(MIN(255, (s[0] * s[3]) / 255));
            p[1] = (unsigned char)(MIN(255, (s[1] * s[3]) / 255));
            p[2] = (unsigned char)(MIN(255, (s[2] * s[3]) / 255));
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   return NULL;
}

/**
 * Free requested pixel block from an image.
 * @param im A handle to an opened Epeg image (unused).
 * @param data The pointer to the image pixels.
 * @return Nothing.
 *
 * This frees the data for a block of pixels requested from image @p im.
 * @p data must be a valid (non NULL) pointer to a pixel block taken from the
 * image @p im by epeg_pixels_get() and must be called before the image is
 * closed by epeg_close().
 */
extern void epeg_pixels_free(Epeg_Image *im, const void *data)
{
#pragma unused (im)
   free((void *)data);
}

/**
 * Get the image comment field as a string.
 * @param im A handle to an opened Epeg image.
 * @return A pointer to the loaded image comments.
 *
 * This function returns the comment field as a string (NUL byte terminated)
 * of the loaded image @p im, if there is a comment, or NULL if no comment is
 * saved with the image. Consider the string returned to be read-only.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern const char *epeg_comment_get(Epeg_Image *im)
{
   return im->in.comment;
}

/**
 * Get thumbnail comments of loaded image.
 * @param im A handle to an opened Epeg image.
 * @param info Pointer to a thumbnail info struct to be filled in.
 * @return Nothing.
 *
 * This function retrieves thumbnail comments written by Epeg to any saved
 * JPEG files. If no thumbnail comments were saved, the fields will be 0 in
 * the @p info struct on return.
 *
 * See also: epeg_comment_get(), epeg_thumbnail_comments_enable()
 */
extern void epeg_thumbnail_comments_get(Epeg_Image *im,
                                        Epeg_Thumbnail_Info *info)
{
   if (!info) {
      return;
   }
   info->uri = im->in.thumb_info.uri;
   info->mtime = im->in.thumb_info.mtime;
   info->w = im->in.thumb_info.w;
   info->h = im->in.thumb_info.h;
   info->mimetype = im->in.thumb_info.mime;
}

/**
 * Set the comment field of the image for saving.
 * @param im A handle to an opened Epeg image.
 * @param comment The comment to set.
 * @return Nothing.
 *
 * Set the comment for the image file for when it gets saved. This is a NUL
 * byte terminated C string. If @p comment is NULL the output file will have
 * no comment field.
 *
 * The default comment will be any comment loaded from the input file.
 *
 * See also: epeg_comment_get()
 */
extern void epeg_comment_set(Epeg_Image *im, const char *comment)
{
   if (im->out.comment) {
      free(im->out.comment);
   }
   if (!comment) {
      im->out.comment = NULL;
   } else {
      im->out.comment = strdup(comment);
   }
}

/**
 * Set the encoding quality of the saved image.
 * @param im A handle to an opened Epeg image.
 * @param quality The quality of encoding from 0 to 100.
 * @return Nothing.
 *
 * Set the quality of the output encoded image. Values from 0 to 100
 * inclusive are valid, with 100 being the maximum quality, and 0 being the
 * minimum. If the quality is set equal to or above 90%, the output U and V
 * color planes are encoded at 1:1 with the Y plane.
 *
 * The default quality is 75.
 *
 * See also: epeg_comment_set()
 */
extern void epeg_quality_set(Epeg_Image *im, int quality)
{
   if (quality < 0) {
      quality = 0;
   } else if (quality > 100) {
      quality = 100;
   }
   im->out.quality = quality;
}

/**
 * Enable thumbnail comments in saved image.
 * @param im A handle to an opened Epeg image.
 * @param onoff A boolean on and off enabling flag.
 * @return Nothing.
 *
 * if @p onoff is 1, the output file will have thumbnail comments added to
 * it, and if it is 0, it will not. The default is 0.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern void epeg_thumbnail_comments_enable(Epeg_Image *im, int onoff)
{
   im->out.thumbnail_info = (char)onoff;
}

/**
 * Set the output file path for the image when saved.
 * @param im A handle to an opened Epeg image.
 * @param file The path to the output file.
 * @return Nothing.
 *
 * This sets the output file path name (either a full or relative path name)
 * to where the file will be written when saved. @p file must be a NUL
 * terminated C string containing the path to the file to be saved to. If it is
 * NULL, then the image will not be saved to a file when calling epeg_encode().
 *
 * See also: epeg_memory_output_set(), epeg_encode()
 */
extern void epeg_file_output_set(Epeg_Image *im, const char *file)
{
   if (im->out.file) {
      free(im->out.file);
   }
   if (!file) {
      im->out.file = NULL;
   } else {
      im->out.file = strdup(file);
   }
}

/**
 * Set the output file to be a block of allocated memory.
 * @param im A handle to an opened Epeg image.
 * @param data A pointer to a pointer to a memory block.
 * @param size A pointer to a counter of the size of the memory block.
 * @return Nothing.
 *
 * This sets the output encoding of the image when saved to be allocated
 * memory. After epeg_close() is called the pointer pointed to by @p data
 * and the integer pointed to by @p size will contain the pointer to the
 * memory block and its size in bytes, respecitvely. The memory block can be
 * freed with the free() function call. If the save fails the pointer to the
 * memory block will be unaffected, as will the size.
 *
 * See also: epeg_file_output_set(), epeg_encode()
 */
extern void epeg_memory_output_set(Epeg_Image *im, unsigned char **data,
                                   int *size)
{
   im->out.mem.data = data;
   im->out.mem.size = size;
}

/**
 * This saves the image to its specified destination.
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * This saves the image @p im to its destination specified by
 * epeg_file_output_set() or epeg_memory_output_set(). The image will be
 * encoded at the decoded pixel size, using the quality, comment,
 * and thumbnail comment settings set on the image.
 *
 * See also: epeg_file_output_set(), epeg_memory_output_set()
 */
extern int epeg_encode(Epeg_Image *im)
{
   if (_epeg_decode(im) != 0) {
      return 1;
   }
   if (_epeg_scale(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * FIXME: Document this with a short, sentence-long description of epeg_trim()
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * FIXME: Document this with a longer, paragraph-long description.
 */
extern int epeg_trim(Epeg_Image *im)
{
   if (_epeg_decode_for_trim(im) != 0) {
      return 1;
   }
   if (_epeg_trim(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * Close an image handle.
 * @param im A handle to an opened Epeg image.
 * @return Nothing.
 *
 * This closes an opened image handle and frees all memory associated with it.
 * It does NOT free encoded data generated by epeg_memory_output_set() followed
 * by epeg_encode(), nor does it guarantee to free any data received by
 * epeg_pixels_get(). Once an image handle is closed, consider it invalid.
 *
 * See also: epeg_file_open(), epeg_memory_open()
 */
extern void epeg_close(Epeg_Image *im)
{
   if (im->pixels) {
      free(im->pixels);
   }
   if (im->lines) {
      free(im->lines);
   }
   if (im->in.file) {
      free(im->in.file);
   }
   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if (im->in.f) {
      fclose(im->in.f);
   }
   if (im->in.comment) {
      free(im->in.comment);
   }
   if (im->in.thumb_info.uri) {
      free(im->in.thumb_info.uri);
   }
   if (im->in.thumb_info.mime) {
      free(im->in.thumb_info.mime);
   }
   if (im->out.file) {
      free(im->out.file);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if (im->out.f) {
      fclose(im->out.f);
   }
   if (im->out.comment) {
      free(im->out.comment);
   }
   free(im);
}

/* static internal private-only function; unnecessary to document: */
static Epeg_Image *_epeg_open_header(Epeg_Image *im)
{
   struct jpeg_marker_struct *m;

   im->in.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      error:
      epeg_close(im);
      im = NULL;
      return NULL;
   }

   jpeg_create_decompress(&(im->in.jinfo));
   jpeg_save_markers(&(im->in.jinfo), (JPEG_APP0 + 7), 1024);
   jpeg_save_markers(&(im->in.jinfo), JPEG_COM, 65535);
   jpeg_stdio_src(&(im->in.jinfo), im->in.f);
   jpeg_read_header(&(im->in.jinfo), TRUE);
   im->in.w = (int)im->in.jinfo.image_width;
   im->in.h = (int)im->in.jinfo.image_height;
   if (im->in.w <= 1) {
      goto error;
   }
   if (im->in.h <= 1) {
      goto error;
   }

   im->out.w = im->in.w;
   im->out.h = im->in.h;

   im->color_space = (((im->in.color_space = im->in.jinfo.out_color_space) == JCS_GRAYSCALE) ? EPEG_GRAY8 : EPEG_RGB8);
   if (im->in.color_space == JCS_CMYK) {
      im->color_space = EPEG_CMYK;
   }

   for ((m = im->in.jinfo.marker_list); m; (m = m->next)) {
      if (m->marker == JPEG_COM) {
         if (im->in.comment) {
            free(im->in.comment);
         }
         im->in.comment = (char *)malloc((size_t)(m->data_length + 1));
         if (im->in.comment) {
            memcpy(im->in.comment, m->data, (size_t)m->data_length);
            im->in.comment[m->data_length] = 0;
         }
      } else if (m->marker == (JPEG_APP0 + 7))  {
         if ((m->data_length > 7) &&
             (!strncmp((char *)m->data, "Thumb::", (size_t)7))) {
            char *p, *p2;

            p = (char *)malloc((size_t)(m->data_length + 1));
            if (p) {
               memcpy(p, m->data, (size_t)m->data_length);
               p[m->data_length] = 0;
               p2 = strchr(p, '\n');
               if (p2) {
                  p2[0] = 0;
                  if (!strcmp(p, "Thumb::URI")) {
                     im->in.thumb_info.uri = strdup(p2 + 1);
                  } else if (!strcmp(p, "Thumb::MTime")) {
                     sscanf((p2 + 1), "%llu", &(im->in.thumb_info.mtime));
                  } else if (!strcmp(p, "Thumb::Image::Width")) {
                     im->in.thumb_info.w = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Image::Height")) {
                     im->in.thumb_info.h = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Mimetype")) {
                     im->in.thumb_info.mime = strdup(p2 + 1);
                  }
               } /* end "if (p2)" */
               free(p);
            } /* end "if (p)" */
         }
      }
   } /* end for-loop */
   return im;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode(Epeg_Image *im)
{
   int scale, scalew, scaleh, y;

   if (im->pixels) {
      return 1;
   }

   scalew = (im->in.w / im->out.w);
   scaleh = (im->in.h / im->out.h);

   scale = scalew;
   if (scaleh < scalew) {
      scale = scaleh;
   }

   if (scale > 8) {
      scale = 8;
   } else if (scale < 1) {
      scale = 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = (unsigned int)scale;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_IFAST;

   switch (im->color_space) {
      case EPEG_GRAY8:
			 im->in.jinfo.out_color_space = JCS_GRAYSCALE;
			 im->in.jinfo.output_components = 1;
			 break;

      case EPEG_YUV8:
			 im->in.jinfo.out_color_space = JCS_YCbCr;
			 break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
			 im->in.jinfo.out_color_space = JCS_RGB;
			 break;

      case EPEG_CMYK:
			 im->in.jinfo.out_color_space = JCS_CMYK;
			 im->in.jinfo.output_components = 4;
			 break;

      default:
			 break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      epeg_close(im);
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
	   jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_scale(Epeg_Image *im)
{
   unsigned char *dst, *row, *src;
   int            x, y, w, h, i;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   for ((y = 0); (y < h); y++) {
      row = (im->pixels +
             ((((unsigned int)y * im->in.jinfo.output_height) / (unsigned int)h) * (unsigned int)im->in.jinfo.output_components * im->in.jinfo.output_width));
      dst = (im->pixels +
             ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));

      for ((x = 0); (x < im->out.w); x++) {
         src = (row +
                ((((unsigned int)x * im->in.jinfo.output_width) / (unsigned int)w) * (unsigned int)im->in.jinfo.output_components));

         for ((i = 0); (i < im->in.jinfo.output_components); i++) {
            dst[i] = src[i];
         } /* end inmost for-loop */

         dst += im->in.jinfo.output_components;
      } /* end inner for-loop */
   } /* end outer for-loop */
   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode_for_trim(Epeg_Image *im)
{
   int y;

   if (im->pixels) {
      return 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = 1;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_ISLOW;

   switch (im->color_space) {
      case EPEG_GRAY8:
		   im->in.jinfo.out_color_space = JCS_GRAYSCALE;
		   im->in.jinfo.output_components = 1;
		   break;

      case EPEG_YUV8:
		   im->in.jinfo.out_color_space = JCS_YCbCr;
		   break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
		   im->in.jinfo.out_color_space = JCS_RGB;
		   break;

      case EPEG_CMYK:
		   im->in.jinfo.out_color_space = JCS_CMYK;
		   im->in.jinfo.output_components = 4;
		   break;

      default:
		   break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
      im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
      jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_trim(Epeg_Image *im)
{
   int y, a, b, w, h;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   a = im->out.x;
   b = im->out.y;

   /* dummy condition to use value stored to 'w': */
   if (w == 0) {
	   ;
   }

   for ((y = 0); (y < h); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)((y + b) * im->in.jinfo.output_components) * im->in.jinfo.output_width)
                      + (a * im->in.jinfo.output_components));
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_encode(Epeg_Image *im)
{
   void  *data = NULL;
   size_t size = 0;

   if (im->out.f) {
      return 1;
   }

   if (im->out.file) {
      im->out.f = fopen(im->out.file, "wb");
   } else {
      im->out.f = _epeg_memfile_write_open(&data, &size);
   }
   if (!im->out.f) {
      im->error = 1;
      return 1;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_create_compress(&(im->out.jinfo));
   jpeg_stdio_dest(&(im->out.jinfo), im->out.f);
   im->out.jinfo.image_width = (JDIMENSION)im->out.w;
   im->out.jinfo.image_height = (JDIMENSION)im->out.h;
   im->out.jinfo.input_components = im->in.jinfo.output_components;
   im->out.jinfo.in_color_space = im->in.jinfo.out_color_space;
   im->out.jinfo.dct_method = JDCT_IFAST;
   im->out.jinfo.dct_method = im->in.jinfo.dct_method;
   jpeg_set_defaults(&(im->out.jinfo));
   jpeg_set_quality(&(im->out.jinfo), im->out.quality, TRUE);

   if (im->out.quality >= 90) {
      im->out.jinfo.comp_info[0].h_samp_factor = 1;
      im->out.jinfo.comp_info[0].v_samp_factor = 1;
      im->out.jinfo.comp_info[1].h_samp_factor = 1;
      im->out.jinfo.comp_info[1].v_samp_factor = 1;
      im->out.jinfo.comp_info[2].h_samp_factor = 1;
      im->out.jinfo.comp_info[2].v_samp_factor = 1;
   }
   jpeg_start_compress(&(im->out.jinfo), TRUE);

   if (im->out.comment) {
      jpeg_write_marker(&(im->out.jinfo), JPEG_COM,
                        (const JOCTET *)im->out.comment,
                        (unsigned int)strlen(im->out.comment));
   }

   if (im->out.thumbnail_info) {
      char buf[8192];

      if (im->in.file) {
         snprintf(buf, sizeof(buf), "Thumb::URI\nfile://%s", im->in.file);
         jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7),
                           (const JOCTET *)buf, (unsigned int)strlen(buf));
         snprintf(buf, sizeof(buf), "Thumb::MTime\n%llu",
#if defined(HAVE_UINTMAX_T) && !defined(__LP64__)
                  (uintmax_t)im->stat_info.st_mtime);
#else
               (unsigned long long int)im->stat_info.st_mtime);
#endif /* HAVE_UINTMAX_T && !__LP64__ */
      }
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Width\n%i", im->in.w);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Height\n%i", im->in.h);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Mimetype\nimage/jpeg");
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
   }

   while (im->out.jinfo.next_scanline < im->out.h) {
      jpeg_write_scanlines(&(im->out.jinfo),
                           &(im->lines[im->out.jinfo.next_scanline]), 1);
   }

   jpeg_finish_compress(&(im->out.jinfo));

   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if ((im->in.f) && (im->in.file)) {
      fclose(im->in.f);
   }
   if ((im->in.f) && (!im->in.file)) {
      _epeg_memfile_read_close(im->in.f);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if ((im->out.f) && (im->out.file)) {
      fclose(im->out.f);
   }
   if ((im->out.f) && (!im->out.file)) {
      _epeg_memfile_write_close(im->out.f);
   }
   im->in.f = NULL;
   im->out.f = NULL;

   if (im->out.mem.data) {
	   *(im->out.mem.data) = (unsigned char *)data;
   }
   if (im->out.mem.size) {
	   *(im->out.mem.size) = (int)size;
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static void _epeg_fatal_error_handler(j_common_ptr cinfo)
{
   emptr errmgr;

   errmgr = (emptr)cinfo->err;
   longjmp(errmgr->setjmp_buffer, 1);
   return;
}

/* silence '-Wunused-macros' warnings: */
#ifdef MAX
# undef MAX
#endif /* MAX */

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_memfile.c': */
/* epeg_memfile.c */
/* gets built into the libepeg library */

#include <stdio.h>
#include "Epeg.h"
#include "epeg_private.h"

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_read_open(void *data, size_t size)
{
#ifdef HAVE_FMEMOPEN
   return (FILE *)fmemopen(data, size, "r");
#else
   FILE *f;

   f = tmpfile();
   if (!f) {
      return NULL;
   }
   fwrite(data, size, (size_t)1L, f);
   rewind(f);
   return f;
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_read_close(FILE *f)
{
#ifdef HAVE_FMEMOPEN
   fclose(f);
#else
   /* FIXME: make these different halves of the conditional actually different
    * from one another */
   fclose(f);
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only struct and typedef; unnecessary to document: */
typedef struct _Eet_Memfile_Write_Info Eet_Memfile_Write_Info;
struct _Eet_Memfile_Write_Info
{
   FILE *f;
   void **data;
   size_t *size;
};

/* static internal private-only global variables; unnecessary to document: */
static int _epeg_memfile_info_alloc_num = 0;
static int _epeg_memfile_info_num = 0;
static Eet_Memfile_Write_Info *_epeg_memfile_info = NULL;

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_write_open(void **data, size_t *size)
{
#ifdef HAVE_OPEN_MEMSTREAM
   return open_memstream((char **)data, size);
#else
   FILE *f;

   _epeg_memfile_info_num++;
   if (_epeg_memfile_info_num > _epeg_memfile_info_alloc_num) {
      Eet_Memfile_Write_Info *tmp;

      _epeg_memfile_info_alloc_num += 16;
      tmp = (Eet_Memfile_Write_Info *)realloc(_epeg_memfile_info,
                                              ((unsigned long)_epeg_memfile_info_alloc_num *
                                               sizeof(Eet_Memfile_Write_Info)));
      if (!tmp) {
         _epeg_memfile_info_alloc_num -= 16;
         _epeg_memfile_info_num--;
         return NULL;
      }
      _epeg_memfile_info = tmp;
   }
   f = tmpfile();
   if (!f) {
      _epeg_memfile_info_num--;
      return NULL;
   }
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].f = f;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].data = data;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].size = size;
   return f;
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_write_close(FILE *f)
{
#ifdef HAVE_OPEN_MEMSTREAM
   fclose(f);
#else
   int i;

   for ((i = 0); (i < _epeg_memfile_info_num); i++) {
      if (_epeg_memfile_info[i].f == f) {
         int j;

         fseek(f, 0L, SEEK_END);
         (*(_epeg_memfile_info[i].size)) = (size_t)ftell(f);
         rewind(f);
         (*(_epeg_memfile_info[i].data)) = malloc(*(_epeg_memfile_info[i].size));
         if (!(*(_epeg_memfile_info[i].data))) {
            fclose(f);
            (*(_epeg_memfile_info[i].size)) = 0;
            return;
         }
         fread((*(_epeg_memfile_info[i].data)), (*(_epeg_memfile_info[i].size)),
               (size_t)1L, f);
         for ((j = (i + 1)); (j < _epeg_memfile_info_num); j++) {
            _epeg_memfile_info[(j - 1)] = _epeg_memfile_info[j];
         }
         _epeg_memfile_info_num--;
         fclose(f);
         return;
      } /* end "if" */
   } /* end for-loop */
   fclose(f);
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_main.c': */
/* epeg_main.c for src/lib for epeg */
/* gets built into the libepeg library */

#include <stdio.h>
#include <stdlib.h>
#include "Epeg.h"
#include "epeg_private.h"

static Epeg_Image*_epeg_open_header(Epeg_Image *im);
static int _epeg_decode(Epeg_Image *im);
static int _epeg_scale(Epeg_Image *im);
static int _epeg_decode_for_trim(Epeg_Image *im);
static int _epeg_trim(Epeg_Image *im);
static int _epeg_encode(Epeg_Image *im);

static void _epeg_fatal_error_handler(j_common_ptr cinfo);

#ifndef MIN
# define MIN(__x,__y) ((__x) < (__y) ? (__x) : (__y))
#endif /* !MIN */
#ifndef MAX
# define MAX(__x,__y) ((__x) > (__y) ? (__x) : (__y))
#endif /* !MAX */

/**
 * Open a JPEG image by filename.
 * @param file The file path to open.
 * @return A handle to the opened JPEG file, with the header decoded.
 *
 * This function opens the file indicated by the @p file parameter, and
 * attempts to decode it as a jpeg file. If this failes, NULL is returned.
 * Otherwise a valid handle to an open JPEG file is returned that can be used
 * by other Epeg calls.
 *
 * The @p file must be a pointer to a valid C string, NUL (0 byte) terminated
 * thats is a relative or absolute file path. If not results are not
 * determined.
 *
 * See also: epeg_memory_open(), epeg_close()
 */
extern Epeg_Image *epeg_file_open(const char *file)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.file = strdup(file);
   im->in.f = fopen(im->in.file, "rb");
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   fstat(fileno(im->in.f), &(im->stat_info));
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Open a JPEG image stored in memory.
 * @param data A pointer to the memory containing the JPEG data.
 * @param size The size of the memory segment containing the JPEG.
 * @return  A handle to the opened JPEG, with the header decoded.
 *
 * This function opens a JPEG file that is stored in memory pointed to by
 * @p data, and that is @p size bytes in size. If successful a valid handle
 * is returned, or on failure NULL is returned.
 *
 * See also: epeg_file_open(), epeg_close()
 */
extern Epeg_Image *epeg_memory_open(unsigned char *data, int size)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.f = _epeg_memfile_read_open(data, (size_t)size);
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Return the original JPEG pixel size.
 * @param im A handle to an opened Epeg image.
 * @param w A pointer to the width value in pixels to be filled in.
 * @param h A pointer to the height value in pixels to be filled in.
 * @return Nothing.
 *
 * Returns the image size in pixels (well not really).
 *
 * See also: epeg_colorspace_get()
 */
extern void epeg_size_get(Epeg_Image *im, int *w, int *h)
{
	if (w) {
		*w = im->in.w;
	}
	if (h) {
		*h = im->in.h;
	}
}

/**
 * Return the original JPEG pixel color space.
 * @param im A handle to an opened Epeg image.
 * @param space A pointer to the color space value to be filled in.
 * @return Nothing.
 *
 * Returns the image color space (not yet though).
 *
 * See also: epeg_size_get()
 */
extern void epeg_colorspace_get(Epeg_Image *im, int *space)
{
	if (space) {
		*space = im->color_space;
	}
}

/**
 * Set the size of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param w The width of the image to decode at, in pixels.
 * @param h The height of the image to decode at, in pixels.
 * @return Nothing.
 *
 * Sets the size at which to decode the JPEG image, giving an optimized load
 * that only decodes the pixels needed.
 *
 * See also: epeg_decode_bounds_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_size_set(Epeg_Image *im, int w, int h)
{
   if (im->pixels) {
	   return;
   }
   if (w < 1) {
	   w = 1;
   } else if (w > im->in.w) {
	   w = im->in.w;
   }
   if (h < 1) {
	   h = 1;
   } else if (h > im->in.h) {
	   h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   im->out.x = 0;
   im->out.y = 0;
}

/**
 * Set the bounds of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param x Boundary X
 * @param y Boundary Y
 * @param w Boundary W
 * @param h Boundary H
 * @return Nothing.
 *
 * Sets the bounds inside which to decode the JPEG image,
 * giving an optimized load that only decodes the bounded pixels.
 * (???)
 *
 * See also: epeg_decode_size_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_bounds_set(Epeg_Image *im, int x, int y, int w, int h)
{
   if (im->pixels) {
      return;
   }
   if (w < 1) {
      w = 1;
   } else if (w > im->in.w) {
      w = im->in.w;
   }
   if (h < 1) {
      h = 1;
   } else if (h > im->in.h) {
      h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   if (x < 0) {
      x = 0;
   }
   if (y < 0) {
      y = 0;
   }
   im->out.x = x;
   im->out.y = y;
}

/**
 * Set the colorspace in which to decode the image.
 * @param im A handle to an opened Epeg image.
 * @param colorspace The colorspace to decode the image in.
 * @return Nothing.
 *
 * This sets the colorspace to decode the image in. The default is EPEG_YUV8,
 * as this is normally the native colorspace of a JPEG file, avoiding any
 * colorspace conversions for a faster load and/or save.
 *
 * See also: epeg_decode_size_set(), epeg_decode_bounds_set()
 */
extern void epeg_decode_colorspace_set(Epeg_Image *im,
                                       Epeg_Colorspace colorspace)
{
	if (im->pixels) {
		return;
	}
	if ((colorspace < EPEG_GRAY8) || (colorspace > EPEG_ARGB32)) {
		return;
	}
	im->color_space = colorspace;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get_as_RGB8()
 */
extern const void *epeg_pixels_get(Epeg_Image *im, int x, int y,  int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
         return NULL;
      }
   }

   if (!im->pixels) {
      return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* go through all 8 values in type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace): */
   if (im->color_space == EPEG_GRAY8) {
      /* this whole block is pretty much the same thing we do for each
       * of the other enumeration values as well... could probably be simplified
       * with a function-like macro or some other sort of meta-programming: */
      unsigned char *pix, *p;

      /* '1': */
      pix = (unsigned char *)malloc((size_t)(w * h * 1L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = s[0];
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_YUV8) {
      unsigned char *pix, *p;

      /* '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGR8) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* reverse order: */
            p[0] = s[2];
            p[1] = s[1];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGBA8) {
      unsigned char *pix, *p;

      /* '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGRA8) {
      unsigned char *pix, *p;

      /* also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = 0xff;
            /* reverse order: */
            p[1] = s[2];
            p[2] = s[1];
            p[3] = s[0];
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_ARGB32) {
      unsigned int *pix, *p;

      /* still also '4': */
      pix = (unsigned int *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (0xff000000 | (unsigned int)(s[0] << 16) | (unsigned int)(s[1] << 8) | (s[2]));
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* another thing that is also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* back to same order again: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } /* end "if-else" that seems like it should really be a "switch" */
   return NULL;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get()
 */
extern const void *epeg_pixels_get_as_RGB8(Epeg_Image *im, int x, int y,
                                           int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
		   return NULL;
	   }
   }

   if (!im->pixels) {
	   return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* unlike the "non-'_as_RGB8'-version" of this function,
    * there are no 'else'-es separating the cases here. Also, we only go through
    * three of the enumeration values of type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace) here: */
   if (im->color_space == EPEG_GRAY8) {
      unsigned char *pix, *p;

      /* get right into the '3' ones: */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[0];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (unsigned char)(MIN(255, (s[0] * s[3]) / 255));
            p[1] = (unsigned char)(MIN(255, (s[1] * s[3]) / 255));
            p[2] = (unsigned char)(MIN(255, (s[2] * s[3]) / 255));
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   return NULL;
}

/**
 * Free requested pixel block from an image.
 * @param im A handle to an opened Epeg image (unused).
 * @param data The pointer to the image pixels.
 * @return Nothing.
 *
 * This frees the data for a block of pixels requested from image @p im.
 * @p data must be a valid (non NULL) pointer to a pixel block taken from the
 * image @p im by epeg_pixels_get() and must be called before the image is
 * closed by epeg_close().
 */
extern void epeg_pixels_free(Epeg_Image *im, const void *data)
{
#pragma unused (im)
   free((void *)data);
}

/**
 * Get the image comment field as a string.
 * @param im A handle to an opened Epeg image.
 * @return A pointer to the loaded image comments.
 *
 * This function returns the comment field as a string (NUL byte terminated)
 * of the loaded image @p im, if there is a comment, or NULL if no comment is
 * saved with the image. Consider the string returned to be read-only.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern const char *epeg_comment_get(Epeg_Image *im)
{
   return im->in.comment;
}

/**
 * Get thumbnail comments of loaded image.
 * @param im A handle to an opened Epeg image.
 * @param info Pointer to a thumbnail info struct to be filled in.
 * @return Nothing.
 *
 * This function retrieves thumbnail comments written by Epeg to any saved
 * JPEG files. If no thumbnail comments were saved, the fields will be 0 in
 * the @p info struct on return.
 *
 * See also: epeg_comment_get(), epeg_thumbnail_comments_enable()
 */
extern void epeg_thumbnail_comments_get(Epeg_Image *im,
                                        Epeg_Thumbnail_Info *info)
{
   if (!info) {
      return;
   }
   info->uri = im->in.thumb_info.uri;
   info->mtime = im->in.thumb_info.mtime;
   info->w = im->in.thumb_info.w;
   info->h = im->in.thumb_info.h;
   info->mimetype = im->in.thumb_info.mime;
}

/**
 * Set the comment field of the image for saving.
 * @param im A handle to an opened Epeg image.
 * @param comment The comment to set.
 * @return Nothing.
 *
 * Set the comment for the image file for when it gets saved. This is a NUL
 * byte terminated C string. If @p comment is NULL the output file will have
 * no comment field.
 *
 * The default comment will be any comment loaded from the input file.
 *
 * See also: epeg_comment_get()
 */
extern void epeg_comment_set(Epeg_Image *im, const char *comment)
{
   if (im->out.comment) {
      free(im->out.comment);
   }
   if (!comment) {
      im->out.comment = NULL;
   } else {
      im->out.comment = strdup(comment);
   }
}

/**
 * Set the encoding quality of the saved image.
 * @param im A handle to an opened Epeg image.
 * @param quality The quality of encoding from 0 to 100.
 * @return Nothing.
 *
 * Set the quality of the output encoded image. Values from 0 to 100
 * inclusive are valid, with 100 being the maximum quality, and 0 being the
 * minimum. If the quality is set equal to or above 90%, the output U and V
 * color planes are encoded at 1:1 with the Y plane.
 *
 * The default quality is 75.
 *
 * See also: epeg_comment_set()
 */
extern void epeg_quality_set(Epeg_Image *im, int quality)
{
   if (quality < 0) {
      quality = 0;
   } else if (quality > 100) {
      quality = 100;
   }
   im->out.quality = quality;
}

/**
 * Enable thumbnail comments in saved image.
 * @param im A handle to an opened Epeg image.
 * @param onoff A boolean on and off enabling flag.
 * @return Nothing.
 *
 * if @p onoff is 1, the output file will have thumbnail comments added to
 * it, and if it is 0, it will not. The default is 0.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern void epeg_thumbnail_comments_enable(Epeg_Image *im, int onoff)
{
   im->out.thumbnail_info = (char)onoff;
}

/**
 * Set the output file path for the image when saved.
 * @param im A handle to an opened Epeg image.
 * @param file The path to the output file.
 * @return Nothing.
 *
 * This sets the output file path name (either a full or relative path name)
 * to where the file will be written when saved. @p file must be a NUL
 * terminated C string containing the path to the file to be saved to. If it is
 * NULL, then the image will not be saved to a file when calling epeg_encode().
 *
 * See also: epeg_memory_output_set(), epeg_encode()
 */
extern void epeg_file_output_set(Epeg_Image *im, const char *file)
{
   if (im->out.file) {
      free(im->out.file);
   }
   if (!file) {
      im->out.file = NULL;
   } else {
      im->out.file = strdup(file);
   }
}

/**
 * Set the output file to be a block of allocated memory.
 * @param im A handle to an opened Epeg image.
 * @param data A pointer to a pointer to a memory block.
 * @param size A pointer to a counter of the size of the memory block.
 * @return Nothing.
 *
 * This sets the output encoding of the image when saved to be allocated
 * memory. After epeg_close() is called the pointer pointed to by @p data
 * and the integer pointed to by @p size will contain the pointer to the
 * memory block and its size in bytes, respecitvely. The memory block can be
 * freed with the free() function call. If the save fails the pointer to the
 * memory block will be unaffected, as will the size.
 *
 * See also: epeg_file_output_set(), epeg_encode()
 */
extern void epeg_memory_output_set(Epeg_Image *im, unsigned char **data,
                                   int *size)
{
   im->out.mem.data = data;
   im->out.mem.size = size;
}

/**
 * This saves the image to its specified destination.
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * This saves the image @p im to its destination specified by
 * epeg_file_output_set() or epeg_memory_output_set(). The image will be
 * encoded at the decoded pixel size, using the quality, comment,
 * and thumbnail comment settings set on the image.
 *
 * See also: epeg_file_output_set(), epeg_memory_output_set()
 */
extern int epeg_encode(Epeg_Image *im)
{
   if (_epeg_decode(im) != 0) {
      return 1;
   }
   if (_epeg_scale(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * FIXME: Document this with a short, sentence-long description of epeg_trim()
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * FIXME: Document this with a longer, paragraph-long description.
 */
extern int epeg_trim(Epeg_Image *im)
{
   if (_epeg_decode_for_trim(im) != 0) {
      return 1;
   }
   if (_epeg_trim(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * Close an image handle.
 * @param im A handle to an opened Epeg image.
 * @return Nothing.
 *
 * This closes an opened image handle and frees all memory associated with it.
 * It does NOT free encoded data generated by epeg_memory_output_set() followed
 * by epeg_encode(), nor does it guarantee to free any data received by
 * epeg_pixels_get(). Once an image handle is closed, consider it invalid.
 *
 * See also: epeg_file_open(), epeg_memory_open()
 */
extern void epeg_close(Epeg_Image *im)
{
   if (im->pixels) {
      free(im->pixels);
   }
   if (im->lines) {
      free(im->lines);
   }
   if (im->in.file) {
      free(im->in.file);
   }
   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if (im->in.f) {
      fclose(im->in.f);
   }
   if (im->in.comment) {
      free(im->in.comment);
   }
   if (im->in.thumb_info.uri) {
      free(im->in.thumb_info.uri);
   }
   if (im->in.thumb_info.mime) {
      free(im->in.thumb_info.mime);
   }
   if (im->out.file) {
      free(im->out.file);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if (im->out.f) {
      fclose(im->out.f);
   }
   if (im->out.comment) {
      free(im->out.comment);
   }
   free(im);
}

/* static internal private-only function; unnecessary to document: */
static Epeg_Image *_epeg_open_header(Epeg_Image *im)
{
   struct jpeg_marker_struct *m;

   im->in.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      error:
      epeg_close(im);
      im = NULL;
      return NULL;
   }

   jpeg_create_decompress(&(im->in.jinfo));
   jpeg_save_markers(&(im->in.jinfo), (JPEG_APP0 + 7), 1024);
   jpeg_save_markers(&(im->in.jinfo), JPEG_COM, 65535);
   jpeg_stdio_src(&(im->in.jinfo), im->in.f);
   jpeg_read_header(&(im->in.jinfo), TRUE);
   im->in.w = (int)im->in.jinfo.image_width;
   im->in.h = (int)im->in.jinfo.image_height;
   if (im->in.w <= 1) {
      goto error;
   }
   if (im->in.h <= 1) {
      goto error;
   }

   im->out.w = im->in.w;
   im->out.h = im->in.h;

   im->color_space = (((im->in.color_space = im->in.jinfo.out_color_space) == JCS_GRAYSCALE) ? EPEG_GRAY8 : EPEG_RGB8);
   if (im->in.color_space == JCS_CMYK) {
      im->color_space = EPEG_CMYK;
   }

   for ((m = im->in.jinfo.marker_list); m; (m = m->next)) {
      if (m->marker == JPEG_COM) {
         if (im->in.comment) {
            free(im->in.comment);
         }
         im->in.comment = (char *)malloc((size_t)(m->data_length + 1));
         if (im->in.comment) {
            memcpy(im->in.comment, m->data, (size_t)m->data_length);
            im->in.comment[m->data_length] = 0;
         }
      } else if (m->marker == (JPEG_APP0 + 7))  {
         if ((m->data_length > 7) &&
             (!strncmp((char *)m->data, "Thumb::", (size_t)7))) {
            char *p, *p2;

            p = (char *)malloc((size_t)(m->data_length + 1));
            if (p) {
               memcpy(p, m->data, (size_t)m->data_length);
               p[m->data_length] = 0;
               p2 = strchr(p, '\n');
               if (p2) {
                  p2[0] = 0;
                  if (!strcmp(p, "Thumb::URI")) {
                     im->in.thumb_info.uri = strdup(p2 + 1);
                  } else if (!strcmp(p, "Thumb::MTime")) {
                     sscanf((p2 + 1), "%llu", &(im->in.thumb_info.mtime));
                  } else if (!strcmp(p, "Thumb::Image::Width")) {
                     im->in.thumb_info.w = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Image::Height")) {
                     im->in.thumb_info.h = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Mimetype")) {
                     im->in.thumb_info.mime = strdup(p2 + 1);
                  }
               } /* end "if (p2)" */
               free(p);
            } /* end "if (p)" */
         }
      }
   } /* end for-loop */
   return im;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode(Epeg_Image *im)
{
   int scale, scalew, scaleh, y;

   if (im->pixels) {
      return 1;
   }

   scalew = (im->in.w / im->out.w);
   scaleh = (im->in.h / im->out.h);

   scale = scalew;
   if (scaleh < scalew) {
      scale = scaleh;
   }

   if (scale > 8) {
      scale = 8;
   } else if (scale < 1) {
      scale = 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = (unsigned int)scale;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_IFAST;

   switch (im->color_space) {
      case EPEG_GRAY8:
			 im->in.jinfo.out_color_space = JCS_GRAYSCALE;
			 im->in.jinfo.output_components = 1;
			 break;

      case EPEG_YUV8:
			 im->in.jinfo.out_color_space = JCS_YCbCr;
			 break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
			 im->in.jinfo.out_color_space = JCS_RGB;
			 break;

      case EPEG_CMYK:
			 im->in.jinfo.out_color_space = JCS_CMYK;
			 im->in.jinfo.output_components = 4;
			 break;

      default:
			 break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      epeg_close(im);
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
	   jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_scale(Epeg_Image *im)
{
   unsigned char *dst, *row, *src;
   int            x, y, w, h, i;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   for ((y = 0); (y < h); y++) {
      row = (im->pixels +
             ((((unsigned int)y * im->in.jinfo.output_height) / (unsigned int)h) * (unsigned int)im->in.jinfo.output_components * im->in.jinfo.output_width));
      dst = (im->pixels +
             ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));

      for ((x = 0); (x < im->out.w); x++) {
         src = (row +
                ((((unsigned int)x * im->in.jinfo.output_width) / (unsigned int)w) * (unsigned int)im->in.jinfo.output_components));

         for ((i = 0); (i < im->in.jinfo.output_components); i++) {
            dst[i] = src[i];
         } /* end inmost for-loop */

         dst += im->in.jinfo.output_components;
      } /* end inner for-loop */
   } /* end outer for-loop */
   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode_for_trim(Epeg_Image *im)
{
   int y;

   if (im->pixels) {
      return 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = 1;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_ISLOW;

   switch (im->color_space) {
      case EPEG_GRAY8:
		   im->in.jinfo.out_color_space = JCS_GRAYSCALE;
		   im->in.jinfo.output_components = 1;
		   break;

      case EPEG_YUV8:
		   im->in.jinfo.out_color_space = JCS_YCbCr;
		   break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
		   im->in.jinfo.out_color_space = JCS_RGB;
		   break;

      case EPEG_CMYK:
		   im->in.jinfo.out_color_space = JCS_CMYK;
		   im->in.jinfo.output_components = 4;
		   break;

      default:
		   break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
      im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
      jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_trim(Epeg_Image *im)
{
   int y, a, b, w, h;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   a = im->out.x;
   b = im->out.y;

   /* dummy condition to use value stored to 'w': */
   if (w == 0) {
	   ;
   }

   for ((y = 0); (y < h); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)((y + b) * im->in.jinfo.output_components) * im->in.jinfo.output_width)
                      + (a * im->in.jinfo.output_components));
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_encode(Epeg_Image *im)
{
   void  *data = NULL;
   size_t size = 0;

   if (im->out.f) {
      return 1;
   }

   if (im->out.file) {
      im->out.f = fopen(im->out.file, "wb");
   } else {
      im->out.f = _epeg_memfile_write_open(&data, &size);
   }
   if (!im->out.f) {
      im->error = 1;
      return 1;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_create_compress(&(im->out.jinfo));
   jpeg_stdio_dest(&(im->out.jinfo), im->out.f);
   im->out.jinfo.image_width = (JDIMENSION)im->out.w;
   im->out.jinfo.image_height = (JDIMENSION)im->out.h;
   im->out.jinfo.input_components = im->in.jinfo.output_components;
   im->out.jinfo.in_color_space = im->in.jinfo.out_color_space;
   im->out.jinfo.dct_method = JDCT_IFAST;
   im->out.jinfo.dct_method = im->in.jinfo.dct_method;
   jpeg_set_defaults(&(im->out.jinfo));
   jpeg_set_quality(&(im->out.jinfo), im->out.quality, TRUE);

   if (im->out.quality >= 90) {
      im->out.jinfo.comp_info[0].h_samp_factor = 1;
      im->out.jinfo.comp_info[0].v_samp_factor = 1;
      im->out.jinfo.comp_info[1].h_samp_factor = 1;
      im->out.jinfo.comp_info[1].v_samp_factor = 1;
      im->out.jinfo.comp_info[2].h_samp_factor = 1;
      im->out.jinfo.comp_info[2].v_samp_factor = 1;
   }
   jpeg_start_compress(&(im->out.jinfo), TRUE);

   if (im->out.comment) {
      jpeg_write_marker(&(im->out.jinfo), JPEG_COM,
                        (const JOCTET *)im->out.comment,
                        (unsigned int)strlen(im->out.comment));
   }

   if (im->out.thumbnail_info) {
      char buf[8192];

      if (im->in.file) {
         snprintf(buf, sizeof(buf), "Thumb::URI\nfile://%s", im->in.file);
         jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7),
                           (const JOCTET *)buf, (unsigned int)strlen(buf));
         snprintf(buf, sizeof(buf), "Thumb::MTime\n%llu",
#if defined(HAVE_UINTMAX_T) && !defined(__LP64__)
                  (uintmax_t)im->stat_info.st_mtime);
#else
               (unsigned long long int)im->stat_info.st_mtime);
#endif /* HAVE_UINTMAX_T && !__LP64__ */
      }
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Width\n%i", im->in.w);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Height\n%i", im->in.h);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Mimetype\nimage/jpeg");
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
   }

   while (im->out.jinfo.next_scanline < im->out.h) {
      jpeg_write_scanlines(&(im->out.jinfo),
                           &(im->lines[im->out.jinfo.next_scanline]), 1);
   }

   jpeg_finish_compress(&(im->out.jinfo));

   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if ((im->in.f) && (im->in.file)) {
      fclose(im->in.f);
   }
   if ((im->in.f) && (!im->in.file)) {
      _epeg_memfile_read_close(im->in.f);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if ((im->out.f) && (im->out.file)) {
      fclose(im->out.f);
   }
   if ((im->out.f) && (!im->out.file)) {
      _epeg_memfile_write_close(im->out.f);
   }
   im->in.f = NULL;
   im->out.f = NULL;

   if (im->out.mem.data) {
	   *(im->out.mem.data) = (unsigned char *)data;
   }
   if (im->out.mem.size) {
	   *(im->out.mem.size) = (int)size;
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static void _epeg_fatal_error_handler(j_common_ptr cinfo)
{
   emptr errmgr;

   errmgr = (emptr)cinfo->err;
   longjmp(errmgr->setjmp_buffer, 1);
   return;
}

/* silence '-Wunused-macros' warnings: */
#ifdef MAX
# undef MAX
#endif /* MAX */

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_memfile.c': */
/* epeg_memfile.c */
/* gets built into the libepeg library */

#include <stdio.h>
#include "Epeg.h"
#include "epeg_private.h"

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_read_open(void *data, size_t size)
{
#ifdef HAVE_FMEMOPEN
   return (FILE *)fmemopen(data, size, "r");
#else
   FILE *f;

   f = tmpfile();
   if (!f) {
      return NULL;
   }
   fwrite(data, size, (size_t)1L, f);
   rewind(f);
   return f;
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_read_close(FILE *f)
{
#ifdef HAVE_FMEMOPEN
   fclose(f);
#else
   /* FIXME: make these different halves of the conditional actually different
    * from one another */
   fclose(f);
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only struct and typedef; unnecessary to document: */
typedef struct _Eet_Memfile_Write_Info Eet_Memfile_Write_Info;
struct _Eet_Memfile_Write_Info
{
   FILE *f;
   void **data;
   size_t *size;
};

/* static internal private-only global variables; unnecessary to document: */
static int _epeg_memfile_info_alloc_num = 0;
static int _epeg_memfile_info_num = 0;
static Eet_Memfile_Write_Info *_epeg_memfile_info = NULL;

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_write_open(void **data, size_t *size)
{
#ifdef HAVE_OPEN_MEMSTREAM
   return open_memstream((char **)data, size);
#else
   FILE *f;

   _epeg_memfile_info_num++;
   if (_epeg_memfile_info_num > _epeg_memfile_info_alloc_num) {
      Eet_Memfile_Write_Info *tmp;

      _epeg_memfile_info_alloc_num += 16;
      tmp = (Eet_Memfile_Write_Info *)realloc(_epeg_memfile_info,
                                              ((unsigned long)_epeg_memfile_info_alloc_num *
                                               sizeof(Eet_Memfile_Write_Info)));
      if (!tmp) {
         _epeg_memfile_info_alloc_num -= 16;
         _epeg_memfile_info_num--;
         return NULL;
      }
      _epeg_memfile_info = tmp;
   }
   f = tmpfile();
   if (!f) {
      _epeg_memfile_info_num--;
      return NULL;
   }
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].f = f;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].data = data;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].size = size;
   return f;
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_write_close(FILE *f)
{
#ifdef HAVE_OPEN_MEMSTREAM
   fclose(f);
#else
   int i;

   for ((i = 0); (i < _epeg_memfile_info_num); i++) {
      if (_epeg_memfile_info[i].f == f) {
         int j;

         fseek(f, 0L, SEEK_END);
         (*(_epeg_memfile_info[i].size)) = (size_t)ftell(f);
         rewind(f);
         (*(_epeg_memfile_info[i].data)) = malloc(*(_epeg_memfile_info[i].size));
         if (!(*(_epeg_memfile_info[i].data))) {
            fclose(f);
            (*(_epeg_memfile_info[i].size)) = 0;
            return;
         }
         fread((*(_epeg_memfile_info[i].data)), (*(_epeg_memfile_info[i].size)),
               (size_t)1L, f);
         for ((j = (i + 1)); (j < _epeg_memfile_info_num); j++) {
            _epeg_memfile_info[(j - 1)] = _epeg_memfile_info[j];
         }
         _epeg_memfile_info_num--;
         fclose(f);
         return;
      } /* end "if" */
   } /* end for-loop */
   fclose(f);
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_main.c': */
/* epeg_main.c for src/lib for epeg */
/* gets built into the libepeg library */

#include <stdio.h>
#include <stdlib.h>
#include "Epeg.h"
#include "epeg_private.h"

static Epeg_Image*_epeg_open_header(Epeg_Image *im);
static int _epeg_decode(Epeg_Image *im);
static int _epeg_scale(Epeg_Image *im);
static int _epeg_decode_for_trim(Epeg_Image *im);
static int _epeg_trim(Epeg_Image *im);
static int _epeg_encode(Epeg_Image *im);

static void _epeg_fatal_error_handler(j_common_ptr cinfo);

#ifndef MIN
# define MIN(__x,__y) ((__x) < (__y) ? (__x) : (__y))
#endif /* !MIN */
#ifndef MAX
# define MAX(__x,__y) ((__x) > (__y) ? (__x) : (__y))
#endif /* !MAX */

/**
 * Open a JPEG image by filename.
 * @param file The file path to open.
 * @return A handle to the opened JPEG file, with the header decoded.
 *
 * This function opens the file indicated by the @p file parameter, and
 * attempts to decode it as a jpeg file. If this failes, NULL is returned.
 * Otherwise a valid handle to an open JPEG file is returned that can be used
 * by other Epeg calls.
 *
 * The @p file must be a pointer to a valid C string, NUL (0 byte) terminated
 * thats is a relative or absolute file path. If not results are not
 * determined.
 *
 * See also: epeg_memory_open(), epeg_close()
 */
extern Epeg_Image *epeg_file_open(const char *file)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.file = strdup(file);
   im->in.f = fopen(im->in.file, "rb");
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   fstat(fileno(im->in.f), &(im->stat_info));
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Open a JPEG image stored in memory.
 * @param data A pointer to the memory containing the JPEG data.
 * @param size The size of the memory segment containing the JPEG.
 * @return  A handle to the opened JPEG, with the header decoded.
 *
 * This function opens a JPEG file that is stored in memory pointed to by
 * @p data, and that is @p size bytes in size. If successful a valid handle
 * is returned, or on failure NULL is returned.
 *
 * See also: epeg_file_open(), epeg_close()
 */
extern Epeg_Image *epeg_memory_open(unsigned char *data, int size)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.f = _epeg_memfile_read_open(data, (size_t)size);
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Return the original JPEG pixel size.
 * @param im A handle to an opened Epeg image.
 * @param w A pointer to the width value in pixels to be filled in.
 * @param h A pointer to the height value in pixels to be filled in.
 * @return Nothing.
 *
 * Returns the image size in pixels (well not really).
 *
 * See also: epeg_colorspace_get()
 */
extern void epeg_size_get(Epeg_Image *im, int *w, int *h)
{
	if (w) {
		*w = im->in.w;
	}
	if (h) {
		*h = im->in.h;
	}
}

/**
 * Return the original JPEG pixel color space.
 * @param im A handle to an opened Epeg image.
 * @param space A pointer to the color space value to be filled in.
 * @return Nothing.
 *
 * Returns the image color space (not yet though).
 *
 * See also: epeg_size_get()
 */
extern void epeg_colorspace_get(Epeg_Image *im, int *space)
{
	if (space) {
		*space = im->color_space;
	}
}

/**
 * Set the size of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param w The width of the image to decode at, in pixels.
 * @param h The height of the image to decode at, in pixels.
 * @return Nothing.
 *
 * Sets the size at which to decode the JPEG image, giving an optimized load
 * that only decodes the pixels needed.
 *
 * See also: epeg_decode_bounds_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_size_set(Epeg_Image *im, int w, int h)
{
   if (im->pixels) {
	   return;
   }
   if (w < 1) {
	   w = 1;
   } else if (w > im->in.w) {
	   w = im->in.w;
   }
   if (h < 1) {
	   h = 1;
   } else if (h > im->in.h) {
	   h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   im->out.x = 0;
   im->out.y = 0;
}

/**
 * Set the bounds of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param x Boundary X
 * @param y Boundary Y
 * @param w Boundary W
 * @param h Boundary H
 * @return Nothing.
 *
 * Sets the bounds inside which to decode the JPEG image,
 * giving an optimized load that only decodes the bounded pixels.
 * (???)
 *
 * See also: epeg_decode_size_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_bounds_set(Epeg_Image *im, int x, int y, int w, int h)
{
   if (im->pixels) {
      return;
   }
   if (w < 1) {
      w = 1;
   } else if (w > im->in.w) {
      w = im->in.w;
   }
   if (h < 1) {
      h = 1;
   } else if (h > im->in.h) {
      h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   if (x < 0) {
      x = 0;
   }
   if (y < 0) {
      y = 0;
   }
   im->out.x = x;
   im->out.y = y;
}

/**
 * Set the colorspace in which to decode the image.
 * @param im A handle to an opened Epeg image.
 * @param colorspace The colorspace to decode the image in.
 * @return Nothing.
 *
 * This sets the colorspace to decode the image in. The default is EPEG_YUV8,
 * as this is normally the native colorspace of a JPEG file, avoiding any
 * colorspace conversions for a faster load and/or save.
 *
 * See also: epeg_decode_size_set(), epeg_decode_bounds_set()
 */
extern void epeg_decode_colorspace_set(Epeg_Image *im,
                                       Epeg_Colorspace colorspace)
{
	if (im->pixels) {
		return;
	}
	if ((colorspace < EPEG_GRAY8) || (colorspace > EPEG_ARGB32)) {
		return;
	}
	im->color_space = colorspace;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get_as_RGB8()
 */
extern const void *epeg_pixels_get(Epeg_Image *im, int x, int y,  int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
         return NULL;
      }
   }

   if (!im->pixels) {
      return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* go through all 8 values in type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace): */
   if (im->color_space == EPEG_GRAY8) {
      /* this whole block is pretty much the same thing we do for each
       * of the other enumeration values as well... could probably be simplified
       * with a function-like macro or some other sort of meta-programming: */
      unsigned char *pix, *p;

      /* '1': */
      pix = (unsigned char *)malloc((size_t)(w * h * 1L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = s[0];
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_YUV8) {
      unsigned char *pix, *p;

      /* '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGR8) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* reverse order: */
            p[0] = s[2];
            p[1] = s[1];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGBA8) {
      unsigned char *pix, *p;

      /* '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGRA8) {
      unsigned char *pix, *p;

      /* also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = 0xff;
            /* reverse order: */
            p[1] = s[2];
            p[2] = s[1];
            p[3] = s[0];
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_ARGB32) {
      unsigned int *pix, *p;

      /* still also '4': */
      pix = (unsigned int *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (0xff000000 | (unsigned int)(s[0] << 16) | (unsigned int)(s[1] << 8) | (s[2]));
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* another thing that is also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* back to same order again: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } /* end "if-else" that seems like it should really be a "switch" */
   return NULL;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get()
 */
extern const void *epeg_pixels_get_as_RGB8(Epeg_Image *im, int x, int y,
                                           int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
		   return NULL;
	   }
   }

   if (!im->pixels) {
	   return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* unlike the "non-'_as_RGB8'-version" of this function,
    * there are no 'else'-es separating the cases here. Also, we only go through
    * three of the enumeration values of type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace) here: */
   if (im->color_space == EPEG_GRAY8) {
      unsigned char *pix, *p;

      /* get right into the '3' ones: */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[0];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (unsigned char)(MIN(255, (s[0] * s[3]) / 255));
            p[1] = (unsigned char)(MIN(255, (s[1] * s[3]) / 255));
            p[2] = (unsigned char)(MIN(255, (s[2] * s[3]) / 255));
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   return NULL;
}

/**
 * Free requested pixel block from an image.
 * @param im A handle to an opened Epeg image (unused).
 * @param data The pointer to the image pixels.
 * @return Nothing.
 *
 * This frees the data for a block of pixels requested from image @p im.
 * @p data must be a valid (non NULL) pointer to a pixel block taken from the
 * image @p im by epeg_pixels_get() and must be called before the image is
 * closed by epeg_close().
 */
extern void epeg_pixels_free(Epeg_Image *im, const void *data)
{
#pragma unused (im)
   free((void *)data);
}

/**
 * Get the image comment field as a string.
 * @param im A handle to an opened Epeg image.
 * @return A pointer to the loaded image comments.
 *
 * This function returns the comment field as a string (NUL byte terminated)
 * of the loaded image @p im, if there is a comment, or NULL if no comment is
 * saved with the image. Consider the string returned to be read-only.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern const char *epeg_comment_get(Epeg_Image *im)
{
   return im->in.comment;
}

/**
 * Get thumbnail comments of loaded image.
 * @param im A handle to an opened Epeg image.
 * @param info Pointer to a thumbnail info struct to be filled in.
 * @return Nothing.
 *
 * This function retrieves thumbnail comments written by Epeg to any saved
 * JPEG files. If no thumbnail comments were saved, the fields will be 0 in
 * the @p info struct on return.
 *
 * See also: epeg_comment_get(), epeg_thumbnail_comments_enable()
 */
extern void epeg_thumbnail_comments_get(Epeg_Image *im,
                                        Epeg_Thumbnail_Info *info)
{
   if (!info) {
      return;
   }
   info->uri = im->in.thumb_info.uri;
   info->mtime = im->in.thumb_info.mtime;
   info->w = im->in.thumb_info.w;
   info->h = im->in.thumb_info.h;
   info->mimetype = im->in.thumb_info.mime;
}

/**
 * Set the comment field of the image for saving.
 * @param im A handle to an opened Epeg image.
 * @param comment The comment to set.
 * @return Nothing.
 *
 * Set the comment for the image file for when it gets saved. This is a NUL
 * byte terminated C string. If @p comment is NULL the output file will have
 * no comment field.
 *
 * The default comment will be any comment loaded from the input file.
 *
 * See also: epeg_comment_get()
 */
extern void epeg_comment_set(Epeg_Image *im, const char *comment)
{
   if (im->out.comment) {
      free(im->out.comment);
   }
   if (!comment) {
      im->out.comment = NULL;
   } else {
      im->out.comment = strdup(comment);
   }
}

/**
 * Set the encoding quality of the saved image.
 * @param im A handle to an opened Epeg image.
 * @param quality The quality of encoding from 0 to 100.
 * @return Nothing.
 *
 * Set the quality of the output encoded image. Values from 0 to 100
 * inclusive are valid, with 100 being the maximum quality, and 0 being the
 * minimum. If the quality is set equal to or above 90%, the output U and V
 * color planes are encoded at 1:1 with the Y plane.
 *
 * The default quality is 75.
 *
 * See also: epeg_comment_set()
 */
extern void epeg_quality_set(Epeg_Image *im, int quality)
{
   if (quality < 0) {
      quality = 0;
   } else if (quality > 100) {
      quality = 100;
   }
   im->out.quality = quality;
}

/**
 * Enable thumbnail comments in saved image.
 * @param im A handle to an opened Epeg image.
 * @param onoff A boolean on and off enabling flag.
 * @return Nothing.
 *
 * if @p onoff is 1, the output file will have thumbnail comments added to
 * it, and if it is 0, it will not. The default is 0.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern void epeg_thumbnail_comments_enable(Epeg_Image *im, int onoff)
{
   im->out.thumbnail_info = (char)onoff;
}

/**
 * Set the output file path for the image when saved.
 * @param im A handle to an opened Epeg image.
 * @param file The path to the output file.
 * @return Nothing.
 *
 * This sets the output file path name (either a full or relative path name)
 * to where the file will be written when saved. @p file must be a NUL
 * terminated C string containing the path to the file to be saved to. If it is
 * NULL, then the image will not be saved to a file when calling epeg_encode().
 *
 * See also: epeg_memory_output_set(), epeg_encode()
 */
extern void epeg_file_output_set(Epeg_Image *im, const char *file)
{
   if (im->out.file) {
      free(im->out.file);
   }
   if (!file) {
      im->out.file = NULL;
   } else {
      im->out.file = strdup(file);
   }
}

/**
 * Set the output file to be a block of allocated memory.
 * @param im A handle to an opened Epeg image.
 * @param data A pointer to a pointer to a memory block.
 * @param size A pointer to a counter of the size of the memory block.
 * @return Nothing.
 *
 * This sets the output encoding of the image when saved to be allocated
 * memory. After epeg_close() is called the pointer pointed to by @p data
 * and the integer pointed to by @p size will contain the pointer to the
 * memory block and its size in bytes, respecitvely. The memory block can be
 * freed with the free() function call. If the save fails the pointer to the
 * memory block will be unaffected, as will the size.
 *
 * See also: epeg_file_output_set(), epeg_encode()
 */
extern void epeg_memory_output_set(Epeg_Image *im, unsigned char **data,
                                   int *size)
{
   im->out.mem.data = data;
   im->out.mem.size = size;
}

/**
 * This saves the image to its specified destination.
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * This saves the image @p im to its destination specified by
 * epeg_file_output_set() or epeg_memory_output_set(). The image will be
 * encoded at the decoded pixel size, using the quality, comment,
 * and thumbnail comment settings set on the image.
 *
 * See also: epeg_file_output_set(), epeg_memory_output_set()
 */
extern int epeg_encode(Epeg_Image *im)
{
   if (_epeg_decode(im) != 0) {
      return 1;
   }
   if (_epeg_scale(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * FIXME: Document this with a short, sentence-long description of epeg_trim()
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * FIXME: Document this with a longer, paragraph-long description.
 */
extern int epeg_trim(Epeg_Image *im)
{
   if (_epeg_decode_for_trim(im) != 0) {
      return 1;
   }
   if (_epeg_trim(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * Close an image handle.
 * @param im A handle to an opened Epeg image.
 * @return Nothing.
 *
 * This closes an opened image handle and frees all memory associated with it.
 * It does NOT free encoded data generated by epeg_memory_output_set() followed
 * by epeg_encode(), nor does it guarantee to free any data received by
 * epeg_pixels_get(). Once an image handle is closed, consider it invalid.
 *
 * See also: epeg_file_open(), epeg_memory_open()
 */
extern void epeg_close(Epeg_Image *im)
{
   if (im->pixels) {
      free(im->pixels);
   }
   if (im->lines) {
      free(im->lines);
   }
   if (im->in.file) {
      free(im->in.file);
   }
   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if (im->in.f) {
      fclose(im->in.f);
   }
   if (im->in.comment) {
      free(im->in.comment);
   }
   if (im->in.thumb_info.uri) {
      free(im->in.thumb_info.uri);
   }
   if (im->in.thumb_info.mime) {
      free(im->in.thumb_info.mime);
   }
   if (im->out.file) {
      free(im->out.file);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if (im->out.f) {
      fclose(im->out.f);
   }
   if (im->out.comment) {
      free(im->out.comment);
   }
   free(im);
}

/* static internal private-only function; unnecessary to document: */
static Epeg_Image *_epeg_open_header(Epeg_Image *im)
{
   struct jpeg_marker_struct *m;

   im->in.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      error:
      epeg_close(im);
      im = NULL;
      return NULL;
   }

   jpeg_create_decompress(&(im->in.jinfo));
   jpeg_save_markers(&(im->in.jinfo), (JPEG_APP0 + 7), 1024);
   jpeg_save_markers(&(im->in.jinfo), JPEG_COM, 65535);
   jpeg_stdio_src(&(im->in.jinfo), im->in.f);
   jpeg_read_header(&(im->in.jinfo), TRUE);
   im->in.w = (int)im->in.jinfo.image_width;
   im->in.h = (int)im->in.jinfo.image_height;
   if (im->in.w <= 1) {
      goto error;
   }
   if (im->in.h <= 1) {
      goto error;
   }

   im->out.w = im->in.w;
   im->out.h = im->in.h;

   im->color_space = (((im->in.color_space = im->in.jinfo.out_color_space) == JCS_GRAYSCALE) ? EPEG_GRAY8 : EPEG_RGB8);
   if (im->in.color_space == JCS_CMYK) {
      im->color_space = EPEG_CMYK;
   }

   for ((m = im->in.jinfo.marker_list); m; (m = m->next)) {
      if (m->marker == JPEG_COM) {
         if (im->in.comment) {
            free(im->in.comment);
         }
         im->in.comment = (char *)malloc((size_t)(m->data_length + 1));
         if (im->in.comment) {
            memcpy(im->in.comment, m->data, (size_t)m->data_length);
            im->in.comment[m->data_length] = 0;
         }
      } else if (m->marker == (JPEG_APP0 + 7))  {
         if ((m->data_length > 7) &&
             (!strncmp((char *)m->data, "Thumb::", (size_t)7))) {
            char *p, *p2;

            p = (char *)malloc((size_t)(m->data_length + 1));
            if (p) {
               memcpy(p, m->data, (size_t)m->data_length);
               p[m->data_length] = 0;
               p2 = strchr(p, '\n');
               if (p2) {
                  p2[0] = 0;
                  if (!strcmp(p, "Thumb::URI")) {
                     im->in.thumb_info.uri = strdup(p2 + 1);
                  } else if (!strcmp(p, "Thumb::MTime")) {
                     sscanf((p2 + 1), "%llu", &(im->in.thumb_info.mtime));
                  } else if (!strcmp(p, "Thumb::Image::Width")) {
                     im->in.thumb_info.w = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Image::Height")) {
                     im->in.thumb_info.h = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Mimetype")) {
                     im->in.thumb_info.mime = strdup(p2 + 1);
                  }
               } /* end "if (p2)" */
               free(p);
            } /* end "if (p)" */
         }
      }
   } /* end for-loop */
   return im;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode(Epeg_Image *im)
{
   int scale, scalew, scaleh, y;

   if (im->pixels) {
      return 1;
   }

   scalew = (im->in.w / im->out.w);
   scaleh = (im->in.h / im->out.h);

   scale = scalew;
   if (scaleh < scalew) {
      scale = scaleh;
   }

   if (scale > 8) {
      scale = 8;
   } else if (scale < 1) {
      scale = 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = (unsigned int)scale;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_IFAST;

   switch (im->color_space) {
      case EPEG_GRAY8:
			 im->in.jinfo.out_color_space = JCS_GRAYSCALE;
			 im->in.jinfo.output_components = 1;
			 break;

      case EPEG_YUV8:
			 im->in.jinfo.out_color_space = JCS_YCbCr;
			 break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
			 im->in.jinfo.out_color_space = JCS_RGB;
			 break;

      case EPEG_CMYK:
			 im->in.jinfo.out_color_space = JCS_CMYK;
			 im->in.jinfo.output_components = 4;
			 break;

      default:
			 break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      epeg_close(im);
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
	   jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_scale(Epeg_Image *im)
{
   unsigned char *dst, *row, *src;
   int            x, y, w, h, i;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   for ((y = 0); (y < h); y++) {
      row = (im->pixels +
             ((((unsigned int)y * im->in.jinfo.output_height) / (unsigned int)h) * (unsigned int)im->in.jinfo.output_components * im->in.jinfo.output_width));
      dst = (im->pixels +
             ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));

      for ((x = 0); (x < im->out.w); x++) {
         src = (row +
                ((((unsigned int)x * im->in.jinfo.output_width) / (unsigned int)w) * (unsigned int)im->in.jinfo.output_components));

         for ((i = 0); (i < im->in.jinfo.output_components); i++) {
            dst[i] = src[i];
         } /* end inmost for-loop */

         dst += im->in.jinfo.output_components;
      } /* end inner for-loop */
   } /* end outer for-loop */
   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode_for_trim(Epeg_Image *im)
{
   int y;

   if (im->pixels) {
      return 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = 1;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_ISLOW;

   switch (im->color_space) {
      case EPEG_GRAY8:
		   im->in.jinfo.out_color_space = JCS_GRAYSCALE;
		   im->in.jinfo.output_components = 1;
		   break;

      case EPEG_YUV8:
		   im->in.jinfo.out_color_space = JCS_YCbCr;
		   break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
		   im->in.jinfo.out_color_space = JCS_RGB;
		   break;

      case EPEG_CMYK:
		   im->in.jinfo.out_color_space = JCS_CMYK;
		   im->in.jinfo.output_components = 4;
		   break;

      default:
		   break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
      im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
      jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_trim(Epeg_Image *im)
{
   int y, a, b, w, h;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   a = im->out.x;
   b = im->out.y;

   /* dummy condition to use value stored to 'w': */
   if (w == 0) {
	   ;
   }

   for ((y = 0); (y < h); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)((y + b) * im->in.jinfo.output_components) * im->in.jinfo.output_width)
                      + (a * im->in.jinfo.output_components));
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_encode(Epeg_Image *im)
{
   void  *data = NULL;
   size_t size = 0;

   if (im->out.f) {
      return 1;
   }

   if (im->out.file) {
      im->out.f = fopen(im->out.file, "wb");
   } else {
      im->out.f = _epeg_memfile_write_open(&data, &size);
   }
   if (!im->out.f) {
      im->error = 1;
      return 1;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_create_compress(&(im->out.jinfo));
   jpeg_stdio_dest(&(im->out.jinfo), im->out.f);
   im->out.jinfo.image_width = (JDIMENSION)im->out.w;
   im->out.jinfo.image_height = (JDIMENSION)im->out.h;
   im->out.jinfo.input_components = im->in.jinfo.output_components;
   im->out.jinfo.in_color_space = im->in.jinfo.out_color_space;
   im->out.jinfo.dct_method = JDCT_IFAST;
   im->out.jinfo.dct_method = im->in.jinfo.dct_method;
   jpeg_set_defaults(&(im->out.jinfo));
   jpeg_set_quality(&(im->out.jinfo), im->out.quality, TRUE);

   if (im->out.quality >= 90) {
      im->out.jinfo.comp_info[0].h_samp_factor = 1;
      im->out.jinfo.comp_info[0].v_samp_factor = 1;
      im->out.jinfo.comp_info[1].h_samp_factor = 1;
      im->out.jinfo.comp_info[1].v_samp_factor = 1;
      im->out.jinfo.comp_info[2].h_samp_factor = 1;
      im->out.jinfo.comp_info[2].v_samp_factor = 1;
   }
   jpeg_start_compress(&(im->out.jinfo), TRUE);

   if (im->out.comment) {
      jpeg_write_marker(&(im->out.jinfo), JPEG_COM,
                        (const JOCTET *)im->out.comment,
                        (unsigned int)strlen(im->out.comment));
   }

   if (im->out.thumbnail_info) {
      char buf[8192];

      if (im->in.file) {
         snprintf(buf, sizeof(buf), "Thumb::URI\nfile://%s", im->in.file);
         jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7),
                           (const JOCTET *)buf, (unsigned int)strlen(buf));
         snprintf(buf, sizeof(buf), "Thumb::MTime\n%llu",
#if defined(HAVE_UINTMAX_T) && !defined(__LP64__)
                  (uintmax_t)im->stat_info.st_mtime);
#else
               (unsigned long long int)im->stat_info.st_mtime);
#endif /* HAVE_UINTMAX_T && !__LP64__ */
      }
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Width\n%i", im->in.w);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Height\n%i", im->in.h);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Mimetype\nimage/jpeg");
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
   }

   while (im->out.jinfo.next_scanline < im->out.h) {
      jpeg_write_scanlines(&(im->out.jinfo),
                           &(im->lines[im->out.jinfo.next_scanline]), 1);
   }

   jpeg_finish_compress(&(im->out.jinfo));

   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if ((im->in.f) && (im->in.file)) {
      fclose(im->in.f);
   }
   if ((im->in.f) && (!im->in.file)) {
      _epeg_memfile_read_close(im->in.f);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if ((im->out.f) && (im->out.file)) {
      fclose(im->out.f);
   }
   if ((im->out.f) && (!im->out.file)) {
      _epeg_memfile_write_close(im->out.f);
   }
   im->in.f = NULL;
   im->out.f = NULL;

   if (im->out.mem.data) {
	   *(im->out.mem.data) = (unsigned char *)data;
   }
   if (im->out.mem.size) {
	   *(im->out.mem.size) = (int)size;
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static void _epeg_fatal_error_handler(j_common_ptr cinfo)
{
   emptr errmgr;

   errmgr = (emptr)cinfo->err;
   longjmp(errmgr->setjmp_buffer, 1);
   return;
}

/* silence '-Wunused-macros' warnings: */
#ifdef MAX
# undef MAX
#endif /* MAX */

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_memfile.c': */
/* epeg_memfile.c */
/* gets built into the libepeg library */

#include <stdio.h>
#include "Epeg.h"
#include "epeg_private.h"

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_read_open(void *data, size_t size)
{
#ifdef HAVE_FMEMOPEN
   return (FILE *)fmemopen(data, size, "r");
#else
   FILE *f;

   f = tmpfile();
   if (!f) {
      return NULL;
   }
   fwrite(data, size, (size_t)1L, f);
   rewind(f);
   return f;
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_read_close(FILE *f)
{
#ifdef HAVE_FMEMOPEN
   fclose(f);
#else
   /* FIXME: make these different halves of the conditional actually different
    * from one another */
   fclose(f);
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only struct and typedef; unnecessary to document: */
typedef struct _Eet_Memfile_Write_Info Eet_Memfile_Write_Info;
struct _Eet_Memfile_Write_Info
{
   FILE *f;
   void **data;
   size_t *size;
};

/* static internal private-only global variables; unnecessary to document: */
static int _epeg_memfile_info_alloc_num = 0;
static int _epeg_memfile_info_num = 0;
static Eet_Memfile_Write_Info *_epeg_memfile_info = NULL;

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_write_open(void **data, size_t *size)
{
#ifdef HAVE_OPEN_MEMSTREAM
   return open_memstream((char **)data, size);
#else
   FILE *f;

   _epeg_memfile_info_num++;
   if (_epeg_memfile_info_num > _epeg_memfile_info_alloc_num) {
      Eet_Memfile_Write_Info *tmp;

      _epeg_memfile_info_alloc_num += 16;
      tmp = (Eet_Memfile_Write_Info *)realloc(_epeg_memfile_info,
                                              ((unsigned long)_epeg_memfile_info_alloc_num *
                                               sizeof(Eet_Memfile_Write_Info)));
      if (!tmp) {
         _epeg_memfile_info_alloc_num -= 16;
         _epeg_memfile_info_num--;
         return NULL;
      }
      _epeg_memfile_info = tmp;
   }
   f = tmpfile();
   if (!f) {
      _epeg_memfile_info_num--;
      return NULL;
   }
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].f = f;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].data = data;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].size = size;
   return f;
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_write_close(FILE *f)
{
#ifdef HAVE_OPEN_MEMSTREAM
   fclose(f);
#else
   int i;

   for ((i = 0); (i < _epeg_memfile_info_num); i++) {
      if (_epeg_memfile_info[i].f == f) {
         int j;

         fseek(f, 0L, SEEK_END);
         (*(_epeg_memfile_info[i].size)) = (size_t)ftell(f);
         rewind(f);
         (*(_epeg_memfile_info[i].data)) = malloc(*(_epeg_memfile_info[i].size));
         if (!(*(_epeg_memfile_info[i].data))) {
            fclose(f);
            (*(_epeg_memfile_info[i].size)) = 0;
            return;
         }
         fread((*(_epeg_memfile_info[i].data)), (*(_epeg_memfile_info[i].size)),
               (size_t)1L, f);
         for ((j = (i + 1)); (j < _epeg_memfile_info_num); j++) {
            _epeg_memfile_info[(j - 1)] = _epeg_memfile_info[j];
         }
         _epeg_memfile_info_num--;
         fclose(f);
         return;
      } /* end "if" */
   } /* end for-loop */
   fclose(f);
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_main.c': */
/* epeg_main.c for src/lib for epeg */
/* gets built into the libepeg library */

#include <stdio.h>
#include <stdlib.h>
#include "Epeg.h"
#include "epeg_private.h"

static Epeg_Image*_epeg_open_header(Epeg_Image *im);
static int _epeg_decode(Epeg_Image *im);
static int _epeg_scale(Epeg_Image *im);
static int _epeg_decode_for_trim(Epeg_Image *im);
static int _epeg_trim(Epeg_Image *im);
static int _epeg_encode(Epeg_Image *im);

static void _epeg_fatal_error_handler(j_common_ptr cinfo);

#ifndef MIN
# define MIN(__x,__y) ((__x) < (__y) ? (__x) : (__y))
#endif /* !MIN */
#ifndef MAX
# define MAX(__x,__y) ((__x) > (__y) ? (__x) : (__y))
#endif /* !MAX */

/**
 * Open a JPEG image by filename.
 * @param file The file path to open.
 * @return A handle to the opened JPEG file, with the header decoded.
 *
 * This function opens the file indicated by the @p file parameter, and
 * attempts to decode it as a jpeg file. If this failes, NULL is returned.
 * Otherwise a valid handle to an open JPEG file is returned that can be used
 * by other Epeg calls.
 *
 * The @p file must be a pointer to a valid C string, NUL (0 byte) terminated
 * thats is a relative or absolute file path. If not results are not
 * determined.
 *
 * See also: epeg_memory_open(), epeg_close()
 */
extern Epeg_Image *epeg_file_open(const char *file)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.file = strdup(file);
   im->in.f = fopen(im->in.file, "rb");
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   fstat(fileno(im->in.f), &(im->stat_info));
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Open a JPEG image stored in memory.
 * @param data A pointer to the memory containing the JPEG data.
 * @param size The size of the memory segment containing the JPEG.
 * @return  A handle to the opened JPEG, with the header decoded.
 *
 * This function opens a JPEG file that is stored in memory pointed to by
 * @p data, and that is @p size bytes in size. If successful a valid handle
 * is returned, or on failure NULL is returned.
 *
 * See also: epeg_file_open(), epeg_close()
 */
extern Epeg_Image *epeg_memory_open(unsigned char *data, int size)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.f = _epeg_memfile_read_open(data, (size_t)size);
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Return the original JPEG pixel size.
 * @param im A handle to an opened Epeg image.
 * @param w A pointer to the width value in pixels to be filled in.
 * @param h A pointer to the height value in pixels to be filled in.
 * @return Nothing.
 *
 * Returns the image size in pixels (well not really).
 *
 * See also: epeg_colorspace_get()
 */
extern void epeg_size_get(Epeg_Image *im, int *w, int *h)
{
	if (w) {
		*w = im->in.w;
	}
	if (h) {
		*h = im->in.h;
	}
}

/**
 * Return the original JPEG pixel color space.
 * @param im A handle to an opened Epeg image.
 * @param space A pointer to the color space value to be filled in.
 * @return Nothing.
 *
 * Returns the image color space (not yet though).
 *
 * See also: epeg_size_get()
 */
extern void epeg_colorspace_get(Epeg_Image *im, int *space)
{
	if (space) {
		*space = im->color_space;
	}
}

/**
 * Set the size of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param w The width of the image to decode at, in pixels.
 * @param h The height of the image to decode at, in pixels.
 * @return Nothing.
 *
 * Sets the size at which to decode the JPEG image, giving an optimized load
 * that only decodes the pixels needed.
 *
 * See also: epeg_decode_bounds_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_size_set(Epeg_Image *im, int w, int h)
{
   if (im->pixels) {
	   return;
   }
   if (w < 1) {
	   w = 1;
   } else if (w > im->in.w) {
	   w = im->in.w;
   }
   if (h < 1) {
	   h = 1;
   } else if (h > im->in.h) {
	   h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   im->out.x = 0;
   im->out.y = 0;
}

/**
 * Set the bounds of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param x Boundary X
 * @param y Boundary Y
 * @param w Boundary W
 * @param h Boundary H
 * @return Nothing.
 *
 * Sets the bounds inside which to decode the JPEG image,
 * giving an optimized load that only decodes the bounded pixels.
 * (???)
 *
 * See also: epeg_decode_size_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_bounds_set(Epeg_Image *im, int x, int y, int w, int h)
{
   if (im->pixels) {
      return;
   }
   if (w < 1) {
      w = 1;
   } else if (w > im->in.w) {
      w = im->in.w;
   }
   if (h < 1) {
      h = 1;
   } else if (h > im->in.h) {
      h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   if (x < 0) {
      x = 0;
   }
   if (y < 0) {
      y = 0;
   }
   im->out.x = x;
   im->out.y = y;
}

/**
 * Set the colorspace in which to decode the image.
 * @param im A handle to an opened Epeg image.
 * @param colorspace The colorspace to decode the image in.
 * @return Nothing.
 *
 * This sets the colorspace to decode the image in. The default is EPEG_YUV8,
 * as this is normally the native colorspace of a JPEG file, avoiding any
 * colorspace conversions for a faster load and/or save.
 *
 * See also: epeg_decode_size_set(), epeg_decode_bounds_set()
 */
extern void epeg_decode_colorspace_set(Epeg_Image *im,
                                       Epeg_Colorspace colorspace)
{
	if (im->pixels) {
		return;
	}
	if ((colorspace < EPEG_GRAY8) || (colorspace > EPEG_ARGB32)) {
		return;
	}
	im->color_space = colorspace;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get_as_RGB8()
 */
extern const void *epeg_pixels_get(Epeg_Image *im, int x, int y,  int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
         return NULL;
      }
   }

   if (!im->pixels) {
      return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* go through all 8 values in type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace): */
   if (im->color_space == EPEG_GRAY8) {
      /* this whole block is pretty much the same thing we do for each
       * of the other enumeration values as well... could probably be simplified
       * with a function-like macro or some other sort of meta-programming: */
      unsigned char *pix, *p;

      /* '1': */
      pix = (unsigned char *)malloc((size_t)(w * h * 1L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = s[0];
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_YUV8) {
      unsigned char *pix, *p;

      /* '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGR8) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* reverse order: */
            p[0] = s[2];
            p[1] = s[1];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGBA8) {
      unsigned char *pix, *p;

      /* '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGRA8) {
      unsigned char *pix, *p;

      /* also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = 0xff;
            /* reverse order: */
            p[1] = s[2];
            p[2] = s[1];
            p[3] = s[0];
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_ARGB32) {
      unsigned int *pix, *p;

      /* still also '4': */
      pix = (unsigned int *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (0xff000000 | (unsigned int)(s[0] << 16) | (unsigned int)(s[1] << 8) | (s[2]));
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* another thing that is also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* back to same order again: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } /* end "if-else" that seems like it should really be a "switch" */
   return NULL;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get()
 */
extern const void *epeg_pixels_get_as_RGB8(Epeg_Image *im, int x, int y,
                                           int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
		   return NULL;
	   }
   }

   if (!im->pixels) {
	   return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* unlike the "non-'_as_RGB8'-version" of this function,
    * there are no 'else'-es separating the cases here. Also, we only go through
    * three of the enumeration values of type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace) here: */
   if (im->color_space == EPEG_GRAY8) {
      unsigned char *pix, *p;

      /* get right into the '3' ones: */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[0];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (unsigned char)(MIN(255, (s[0] * s[3]) / 255));
            p[1] = (unsigned char)(MIN(255, (s[1] * s[3]) / 255));
            p[2] = (unsigned char)(MIN(255, (s[2] * s[3]) / 255));
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   return NULL;
}

/**
 * Free requested pixel block from an image.
 * @param im A handle to an opened Epeg image (unused).
 * @param data The pointer to the image pixels.
 * @return Nothing.
 *
 * This frees the data for a block of pixels requested from image @p im.
 * @p data must be a valid (non NULL) pointer to a pixel block taken from the
 * image @p im by epeg_pixels_get() and must be called before the image is
 * closed by epeg_close().
 */
extern void epeg_pixels_free(Epeg_Image *im, const void *data)
{
#pragma unused (im)
   free((void *)data);
}

/**
 * Get the image comment field as a string.
 * @param im A handle to an opened Epeg image.
 * @return A pointer to the loaded image comments.
 *
 * This function returns the comment field as a string (NUL byte terminated)
 * of the loaded image @p im, if there is a comment, or NULL if no comment is
 * saved with the image. Consider the string returned to be read-only.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern const char *epeg_comment_get(Epeg_Image *im)
{
   return im->in.comment;
}

/**
 * Get thumbnail comments of loaded image.
 * @param im A handle to an opened Epeg image.
 * @param info Pointer to a thumbnail info struct to be filled in.
 * @return Nothing.
 *
 * This function retrieves thumbnail comments written by Epeg to any saved
 * JPEG files. If no thumbnail comments were saved, the fields will be 0 in
 * the @p info struct on return.
 *
 * See also: epeg_comment_get(), epeg_thumbnail_comments_enable()
 */
extern void epeg_thumbnail_comments_get(Epeg_Image *im,
                                        Epeg_Thumbnail_Info *info)
{
   if (!info) {
      return;
   }
   info->uri = im->in.thumb_info.uri;
   info->mtime = im->in.thumb_info.mtime;
   info->w = im->in.thumb_info.w;
   info->h = im->in.thumb_info.h;
   info->mimetype = im->in.thumb_info.mime;
}

/**
 * Set the comment field of the image for saving.
 * @param im A handle to an opened Epeg image.
 * @param comment The comment to set.
 * @return Nothing.
 *
 * Set the comment for the image file for when it gets saved. This is a NUL
 * byte terminated C string. If @p comment is NULL the output file will have
 * no comment field.
 *
 * The default comment will be any comment loaded from the input file.
 *
 * See also: epeg_comment_get()
 */
extern void epeg_comment_set(Epeg_Image *im, const char *comment)
{
   if (im->out.comment) {
      free(im->out.comment);
   }
   if (!comment) {
      im->out.comment = NULL;
   } else {
      im->out.comment = strdup(comment);
   }
}

/**
 * Set the encoding quality of the saved image.
 * @param im A handle to an opened Epeg image.
 * @param quality The quality of encoding from 0 to 100.
 * @return Nothing.
 *
 * Set the quality of the output encoded image. Values from 0 to 100
 * inclusive are valid, with 100 being the maximum quality, and 0 being the
 * minimum. If the quality is set equal to or above 90%, the output U and V
 * color planes are encoded at 1:1 with the Y plane.
 *
 * The default quality is 75.
 *
 * See also: epeg_comment_set()
 */
extern void epeg_quality_set(Epeg_Image *im, int quality)
{
   if (quality < 0) {
      quality = 0;
   } else if (quality > 100) {
      quality = 100;
   }
   im->out.quality = quality;
}

/**
 * Enable thumbnail comments in saved image.
 * @param im A handle to an opened Epeg image.
 * @param onoff A boolean on and off enabling flag.
 * @return Nothing.
 *
 * if @p onoff is 1, the output file will have thumbnail comments added to
 * it, and if it is 0, it will not. The default is 0.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern void epeg_thumbnail_comments_enable(Epeg_Image *im, int onoff)
{
   im->out.thumbnail_info = (char)onoff;
}

/**
 * Set the output file path for the image when saved.
 * @param im A handle to an opened Epeg image.
 * @param file The path to the output file.
 * @return Nothing.
 *
 * This sets the output file path name (either a full or relative path name)
 * to where the file will be written when saved. @p file must be a NUL
 * terminated C string containing the path to the file to be saved to. If it is
 * NULL, then the image will not be saved to a file when calling epeg_encode().
 *
 * See also: epeg_memory_output_set(), epeg_encode()
 */
extern void epeg_file_output_set(Epeg_Image *im, const char *file)
{
   if (im->out.file) {
      free(im->out.file);
   }
   if (!file) {
      im->out.file = NULL;
   } else {
      im->out.file = strdup(file);
   }
}

/**
 * Set the output file to be a block of allocated memory.
 * @param im A handle to an opened Epeg image.
 * @param data A pointer to a pointer to a memory block.
 * @param size A pointer to a counter of the size of the memory block.
 * @return Nothing.
 *
 * This sets the output encoding of the image when saved to be allocated
 * memory. After epeg_close() is called the pointer pointed to by @p data
 * and the integer pointed to by @p size will contain the pointer to the
 * memory block and its size in bytes, respecitvely. The memory block can be
 * freed with the free() function call. If the save fails the pointer to the
 * memory block will be unaffected, as will the size.
 *
 * See also: epeg_file_output_set(), epeg_encode()
 */
extern void epeg_memory_output_set(Epeg_Image *im, unsigned char **data,
                                   int *size)
{
   im->out.mem.data = data;
   im->out.mem.size = size;
}

/**
 * This saves the image to its specified destination.
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * This saves the image @p im to its destination specified by
 * epeg_file_output_set() or epeg_memory_output_set(). The image will be
 * encoded at the decoded pixel size, using the quality, comment,
 * and thumbnail comment settings set on the image.
 *
 * See also: epeg_file_output_set(), epeg_memory_output_set()
 */
extern int epeg_encode(Epeg_Image *im)
{
   if (_epeg_decode(im) != 0) {
      return 1;
   }
   if (_epeg_scale(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * FIXME: Document this with a short, sentence-long description of epeg_trim()
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * FIXME: Document this with a longer, paragraph-long description.
 */
extern int epeg_trim(Epeg_Image *im)
{
   if (_epeg_decode_for_trim(im) != 0) {
      return 1;
   }
   if (_epeg_trim(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * Close an image handle.
 * @param im A handle to an opened Epeg image.
 * @return Nothing.
 *
 * This closes an opened image handle and frees all memory associated with it.
 * It does NOT free encoded data generated by epeg_memory_output_set() followed
 * by epeg_encode(), nor does it guarantee to free any data received by
 * epeg_pixels_get(). Once an image handle is closed, consider it invalid.
 *
 * See also: epeg_file_open(), epeg_memory_open()
 */
extern void epeg_close(Epeg_Image *im)
{
   if (im->pixels) {
      free(im->pixels);
   }
   if (im->lines) {
      free(im->lines);
   }
   if (im->in.file) {
      free(im->in.file);
   }
   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if (im->in.f) {
      fclose(im->in.f);
   }
   if (im->in.comment) {
      free(im->in.comment);
   }
   if (im->in.thumb_info.uri) {
      free(im->in.thumb_info.uri);
   }
   if (im->in.thumb_info.mime) {
      free(im->in.thumb_info.mime);
   }
   if (im->out.file) {
      free(im->out.file);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if (im->out.f) {
      fclose(im->out.f);
   }
   if (im->out.comment) {
      free(im->out.comment);
   }
   free(im);
}

/* static internal private-only function; unnecessary to document: */
static Epeg_Image *_epeg_open_header(Epeg_Image *im)
{
   struct jpeg_marker_struct *m;

   im->in.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      error:
      epeg_close(im);
      im = NULL;
      return NULL;
   }

   jpeg_create_decompress(&(im->in.jinfo));
   jpeg_save_markers(&(im->in.jinfo), (JPEG_APP0 + 7), 1024);
   jpeg_save_markers(&(im->in.jinfo), JPEG_COM, 65535);
   jpeg_stdio_src(&(im->in.jinfo), im->in.f);
   jpeg_read_header(&(im->in.jinfo), TRUE);
   im->in.w = (int)im->in.jinfo.image_width;
   im->in.h = (int)im->in.jinfo.image_height;
   if (im->in.w <= 1) {
      goto error;
   }
   if (im->in.h <= 1) {
      goto error;
   }

   im->out.w = im->in.w;
   im->out.h = im->in.h;

   im->color_space = (((im->in.color_space = im->in.jinfo.out_color_space) == JCS_GRAYSCALE) ? EPEG_GRAY8 : EPEG_RGB8);
   if (im->in.color_space == JCS_CMYK) {
      im->color_space = EPEG_CMYK;
   }

   for ((m = im->in.jinfo.marker_list); m; (m = m->next)) {
      if (m->marker == JPEG_COM) {
         if (im->in.comment) {
            free(im->in.comment);
         }
         im->in.comment = (char *)malloc((size_t)(m->data_length + 1));
         if (im->in.comment) {
            memcpy(im->in.comment, m->data, (size_t)m->data_length);
            im->in.comment[m->data_length] = 0;
         }
      } else if (m->marker == (JPEG_APP0 + 7))  {
         if ((m->data_length > 7) &&
             (!strncmp((char *)m->data, "Thumb::", (size_t)7))) {
            char *p, *p2;

            p = (char *)malloc((size_t)(m->data_length + 1));
            if (p) {
               memcpy(p, m->data, (size_t)m->data_length);
               p[m->data_length] = 0;
               p2 = strchr(p, '\n');
               if (p2) {
                  p2[0] = 0;
                  if (!strcmp(p, "Thumb::URI")) {
                     im->in.thumb_info.uri = strdup(p2 + 1);
                  } else if (!strcmp(p, "Thumb::MTime")) {
                     sscanf((p2 + 1), "%llu", &(im->in.thumb_info.mtime));
                  } else if (!strcmp(p, "Thumb::Image::Width")) {
                     im->in.thumb_info.w = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Image::Height")) {
                     im->in.thumb_info.h = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Mimetype")) {
                     im->in.thumb_info.mime = strdup(p2 + 1);
                  }
               } /* end "if (p2)" */
               free(p);
            } /* end "if (p)" */
         }
      }
   } /* end for-loop */
   return im;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode(Epeg_Image *im)
{
   int scale, scalew, scaleh, y;

   if (im->pixels) {
      return 1;
   }

   scalew = (im->in.w / im->out.w);
   scaleh = (im->in.h / im->out.h);

   scale = scalew;
   if (scaleh < scalew) {
      scale = scaleh;
   }

   if (scale > 8) {
      scale = 8;
   } else if (scale < 1) {
      scale = 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = (unsigned int)scale;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_IFAST;

   switch (im->color_space) {
      case EPEG_GRAY8:
			 im->in.jinfo.out_color_space = JCS_GRAYSCALE;
			 im->in.jinfo.output_components = 1;
			 break;

      case EPEG_YUV8:
			 im->in.jinfo.out_color_space = JCS_YCbCr;
			 break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
			 im->in.jinfo.out_color_space = JCS_RGB;
			 break;

      case EPEG_CMYK:
			 im->in.jinfo.out_color_space = JCS_CMYK;
			 im->in.jinfo.output_components = 4;
			 break;

      default:
			 break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      epeg_close(im);
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
	   jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_scale(Epeg_Image *im)
{
   unsigned char *dst, *row, *src;
   int            x, y, w, h, i;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   for ((y = 0); (y < h); y++) {
      row = (im->pixels +
             ((((unsigned int)y * im->in.jinfo.output_height) / (unsigned int)h) * (unsigned int)im->in.jinfo.output_components * im->in.jinfo.output_width));
      dst = (im->pixels +
             ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));

      for ((x = 0); (x < im->out.w); x++) {
         src = (row +
                ((((unsigned int)x * im->in.jinfo.output_width) / (unsigned int)w) * (unsigned int)im->in.jinfo.output_components));

         for ((i = 0); (i < im->in.jinfo.output_components); i++) {
            dst[i] = src[i];
         } /* end inmost for-loop */

         dst += im->in.jinfo.output_components;
      } /* end inner for-loop */
   } /* end outer for-loop */
   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode_for_trim(Epeg_Image *im)
{
   int y;

   if (im->pixels) {
      return 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = 1;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_ISLOW;

   switch (im->color_space) {
      case EPEG_GRAY8:
		   im->in.jinfo.out_color_space = JCS_GRAYSCALE;
		   im->in.jinfo.output_components = 1;
		   break;

      case EPEG_YUV8:
		   im->in.jinfo.out_color_space = JCS_YCbCr;
		   break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
		   im->in.jinfo.out_color_space = JCS_RGB;
		   break;

      case EPEG_CMYK:
		   im->in.jinfo.out_color_space = JCS_CMYK;
		   im->in.jinfo.output_components = 4;
		   break;

      default:
		   break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
      im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
      jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_trim(Epeg_Image *im)
{
   int y, a, b, w, h;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   a = im->out.x;
   b = im->out.y;

   /* dummy condition to use value stored to 'w': */
   if (w == 0) {
	   ;
   }

   for ((y = 0); (y < h); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)((y + b) * im->in.jinfo.output_components) * im->in.jinfo.output_width)
                      + (a * im->in.jinfo.output_components));
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_encode(Epeg_Image *im)
{
   void  *data = NULL;
   size_t size = 0;

   if (im->out.f) {
      return 1;
   }

   if (im->out.file) {
      im->out.f = fopen(im->out.file, "wb");
   } else {
      im->out.f = _epeg_memfile_write_open(&data, &size);
   }
   if (!im->out.f) {
      im->error = 1;
      return 1;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_create_compress(&(im->out.jinfo));
   jpeg_stdio_dest(&(im->out.jinfo), im->out.f);
   im->out.jinfo.image_width = (JDIMENSION)im->out.w;
   im->out.jinfo.image_height = (JDIMENSION)im->out.h;
   im->out.jinfo.input_components = im->in.jinfo.output_components;
   im->out.jinfo.in_color_space = im->in.jinfo.out_color_space;
   im->out.jinfo.dct_method = JDCT_IFAST;
   im->out.jinfo.dct_method = im->in.jinfo.dct_method;
   jpeg_set_defaults(&(im->out.jinfo));
   jpeg_set_quality(&(im->out.jinfo), im->out.quality, TRUE);

   if (im->out.quality >= 90) {
      im->out.jinfo.comp_info[0].h_samp_factor = 1;
      im->out.jinfo.comp_info[0].v_samp_factor = 1;
      im->out.jinfo.comp_info[1].h_samp_factor = 1;
      im->out.jinfo.comp_info[1].v_samp_factor = 1;
      im->out.jinfo.comp_info[2].h_samp_factor = 1;
      im->out.jinfo.comp_info[2].v_samp_factor = 1;
   }
   jpeg_start_compress(&(im->out.jinfo), TRUE);

   if (im->out.comment) {
      jpeg_write_marker(&(im->out.jinfo), JPEG_COM,
                        (const JOCTET *)im->out.comment,
                        (unsigned int)strlen(im->out.comment));
   }

   if (im->out.thumbnail_info) {
      char buf[8192];

      if (im->in.file) {
         snprintf(buf, sizeof(buf), "Thumb::URI\nfile://%s", im->in.file);
         jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7),
                           (const JOCTET *)buf, (unsigned int)strlen(buf));
         snprintf(buf, sizeof(buf), "Thumb::MTime\n%llu",
#if defined(HAVE_UINTMAX_T) && !defined(__LP64__)
                  (uintmax_t)im->stat_info.st_mtime);
#else
               (unsigned long long int)im->stat_info.st_mtime);
#endif /* HAVE_UINTMAX_T && !__LP64__ */
      }
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Width\n%i", im->in.w);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Height\n%i", im->in.h);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Mimetype\nimage/jpeg");
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
   }

   while (im->out.jinfo.next_scanline < im->out.h) {
      jpeg_write_scanlines(&(im->out.jinfo),
                           &(im->lines[im->out.jinfo.next_scanline]), 1);
   }

   jpeg_finish_compress(&(im->out.jinfo));

   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if ((im->in.f) && (im->in.file)) {
      fclose(im->in.f);
   }
   if ((im->in.f) && (!im->in.file)) {
      _epeg_memfile_read_close(im->in.f);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if ((im->out.f) && (im->out.file)) {
      fclose(im->out.f);
   }
   if ((im->out.f) && (!im->out.file)) {
      _epeg_memfile_write_close(im->out.f);
   }
   im->in.f = NULL;
   im->out.f = NULL;

   if (im->out.mem.data) {
	   *(im->out.mem.data) = (unsigned char *)data;
   }
   if (im->out.mem.size) {
	   *(im->out.mem.size) = (int)size;
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static void _epeg_fatal_error_handler(j_common_ptr cinfo)
{
   emptr errmgr;

   errmgr = (emptr)cinfo->err;
   longjmp(errmgr->setjmp_buffer, 1);
   return;
}

/* silence '-Wunused-macros' warnings: */
#ifdef MAX
# undef MAX
#endif /* MAX */

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_memfile.c': */
/* epeg_memfile.c */
/* gets built into the libepeg library */

#include <stdio.h>
#include "Epeg.h"
#include "epeg_private.h"

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_read_open(void *data, size_t size)
{
#ifdef HAVE_FMEMOPEN
   return (FILE *)fmemopen(data, size, "r");
#else
   FILE *f;

   f = tmpfile();
   if (!f) {
      return NULL;
   }
   fwrite(data, size, (size_t)1L, f);
   rewind(f);
   return f;
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_read_close(FILE *f)
{
#ifdef HAVE_FMEMOPEN
   fclose(f);
#else
   /* FIXME: make these different halves of the conditional actually different
    * from one another */
   fclose(f);
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only struct and typedef; unnecessary to document: */
typedef struct _Eet_Memfile_Write_Info Eet_Memfile_Write_Info;
struct _Eet_Memfile_Write_Info
{
   FILE *f;
   void **data;
   size_t *size;
};

/* static internal private-only global variables; unnecessary to document: */
static int _epeg_memfile_info_alloc_num = 0;
static int _epeg_memfile_info_num = 0;
static Eet_Memfile_Write_Info *_epeg_memfile_info = NULL;

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_write_open(void **data, size_t *size)
{
#ifdef HAVE_OPEN_MEMSTREAM
   return open_memstream((char **)data, size);
#else
   FILE *f;

   _epeg_memfile_info_num++;
   if (_epeg_memfile_info_num > _epeg_memfile_info_alloc_num) {
      Eet_Memfile_Write_Info *tmp;

      _epeg_memfile_info_alloc_num += 16;
      tmp = (Eet_Memfile_Write_Info *)realloc(_epeg_memfile_info,
                                              ((unsigned long)_epeg_memfile_info_alloc_num *
                                               sizeof(Eet_Memfile_Write_Info)));
      if (!tmp) {
         _epeg_memfile_info_alloc_num -= 16;
         _epeg_memfile_info_num--;
         return NULL;
      }
      _epeg_memfile_info = tmp;
   }
   f = tmpfile();
   if (!f) {
      _epeg_memfile_info_num--;
      return NULL;
   }
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].f = f;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].data = data;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].size = size;
   return f;
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_write_close(FILE *f)
{
#ifdef HAVE_OPEN_MEMSTREAM
   fclose(f);
#else
   int i;

   for ((i = 0); (i < _epeg_memfile_info_num); i++) {
      if (_epeg_memfile_info[i].f == f) {
         int j;

         fseek(f, 0L, SEEK_END);
         (*(_epeg_memfile_info[i].size)) = (size_t)ftell(f);
         rewind(f);
         (*(_epeg_memfile_info[i].data)) = malloc(*(_epeg_memfile_info[i].size));
         if (!(*(_epeg_memfile_info[i].data))) {
            fclose(f);
            (*(_epeg_memfile_info[i].size)) = 0;
            return;
         }
         fread((*(_epeg_memfile_info[i].data)), (*(_epeg_memfile_info[i].size)),
               (size_t)1L, f);
         for ((j = (i + 1)); (j < _epeg_memfile_info_num); j++) {
            _epeg_memfile_info[(j - 1)] = _epeg_memfile_info[j];
         }
         _epeg_memfile_info_num--;
         fclose(f);
         return;
      } /* end "if" */
   } /* end for-loop */
   fclose(f);
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_main.c': */
/* epeg_main.c for src/lib for epeg */
/* gets built into the libepeg library */

#include <stdio.h>
#include <stdlib.h>
#include "Epeg.h"
#include "epeg_private.h"

static Epeg_Image*_epeg_open_header(Epeg_Image *im);
static int _epeg_decode(Epeg_Image *im);
static int _epeg_scale(Epeg_Image *im);
static int _epeg_decode_for_trim(Epeg_Image *im);
static int _epeg_trim(Epeg_Image *im);
static int _epeg_encode(Epeg_Image *im);

static void _epeg_fatal_error_handler(j_common_ptr cinfo);

#ifndef MIN
# define MIN(__x,__y) ((__x) < (__y) ? (__x) : (__y))
#endif /* !MIN */
#ifndef MAX
# define MAX(__x,__y) ((__x) > (__y) ? (__x) : (__y))
#endif /* !MAX */

/**
 * Open a JPEG image by filename.
 * @param file The file path to open.
 * @return A handle to the opened JPEG file, with the header decoded.
 *
 * This function opens the file indicated by the @p file parameter, and
 * attempts to decode it as a jpeg file. If this failes, NULL is returned.
 * Otherwise a valid handle to an open JPEG file is returned that can be used
 * by other Epeg calls.
 *
 * The @p file must be a pointer to a valid C string, NUL (0 byte) terminated
 * thats is a relative or absolute file path. If not results are not
 * determined.
 *
 * See also: epeg_memory_open(), epeg_close()
 */
extern Epeg_Image *epeg_file_open(const char *file)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.file = strdup(file);
   im->in.f = fopen(im->in.file, "rb");
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   fstat(fileno(im->in.f), &(im->stat_info));
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Open a JPEG image stored in memory.
 * @param data A pointer to the memory containing the JPEG data.
 * @param size The size of the memory segment containing the JPEG.
 * @return  A handle to the opened JPEG, with the header decoded.
 *
 * This function opens a JPEG file that is stored in memory pointed to by
 * @p data, and that is @p size bytes in size. If successful a valid handle
 * is returned, or on failure NULL is returned.
 *
 * See also: epeg_file_open(), epeg_close()
 */
extern Epeg_Image *epeg_memory_open(unsigned char *data, int size)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.f = _epeg_memfile_read_open(data, (size_t)size);
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Return the original JPEG pixel size.
 * @param im A handle to an opened Epeg image.
 * @param w A pointer to the width value in pixels to be filled in.
 * @param h A pointer to the height value in pixels to be filled in.
 * @return Nothing.
 *
 * Returns the image size in pixels (well not really).
 *
 * See also: epeg_colorspace_get()
 */
extern void epeg_size_get(Epeg_Image *im, int *w, int *h)
{
	if (w) {
		*w = im->in.w;
	}
	if (h) {
		*h = im->in.h;
	}
}

/**
 * Return the original JPEG pixel color space.
 * @param im A handle to an opened Epeg image.
 * @param space A pointer to the color space value to be filled in.
 * @return Nothing.
 *
 * Returns the image color space (not yet though).
 *
 * See also: epeg_size_get()
 */
extern void epeg_colorspace_get(Epeg_Image *im, int *space)
{
	if (space) {
		*space = im->color_space;
	}
}

/**
 * Set the size of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param w The width of the image to decode at, in pixels.
 * @param h The height of the image to decode at, in pixels.
 * @return Nothing.
 *
 * Sets the size at which to decode the JPEG image, giving an optimized load
 * that only decodes the pixels needed.
 *
 * See also: epeg_decode_bounds_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_size_set(Epeg_Image *im, int w, int h)
{
   if (im->pixels) {
	   return;
   }
   if (w < 1) {
	   w = 1;
   } else if (w > im->in.w) {
	   w = im->in.w;
   }
   if (h < 1) {
	   h = 1;
   } else if (h > im->in.h) {
	   h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   im->out.x = 0;
   im->out.y = 0;
}

/**
 * Set the bounds of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param x Boundary X
 * @param y Boundary Y
 * @param w Boundary W
 * @param h Boundary H
 * @return Nothing.
 *
 * Sets the bounds inside which to decode the JPEG image,
 * giving an optimized load that only decodes the bounded pixels.
 * (???)
 *
 * See also: epeg_decode_size_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_bounds_set(Epeg_Image *im, int x, int y, int w, int h)
{
   if (im->pixels) {
      return;
   }
   if (w < 1) {
      w = 1;
   } else if (w > im->in.w) {
      w = im->in.w;
   }
   if (h < 1) {
      h = 1;
   } else if (h > im->in.h) {
      h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   if (x < 0) {
      x = 0;
   }
   if (y < 0) {
      y = 0;
   }
   im->out.x = x;
   im->out.y = y;
}

/**
 * Set the colorspace in which to decode the image.
 * @param im A handle to an opened Epeg image.
 * @param colorspace The colorspace to decode the image in.
 * @return Nothing.
 *
 * This sets the colorspace to decode the image in. The default is EPEG_YUV8,
 * as this is normally the native colorspace of a JPEG file, avoiding any
 * colorspace conversions for a faster load and/or save.
 *
 * See also: epeg_decode_size_set(), epeg_decode_bounds_set()
 */
extern void epeg_decode_colorspace_set(Epeg_Image *im,
                                       Epeg_Colorspace colorspace)
{
	if (im->pixels) {
		return;
	}
	if ((colorspace < EPEG_GRAY8) || (colorspace > EPEG_ARGB32)) {
		return;
	}
	im->color_space = colorspace;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get_as_RGB8()
 */
extern const void *epeg_pixels_get(Epeg_Image *im, int x, int y,  int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
         return NULL;
      }
   }

   if (!im->pixels) {
      return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* go through all 8 values in type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace): */
   if (im->color_space == EPEG_GRAY8) {
      /* this whole block is pretty much the same thing we do for each
       * of the other enumeration values as well... could probably be simplified
       * with a function-like macro or some other sort of meta-programming: */
      unsigned char *pix, *p;

      /* '1': */
      pix = (unsigned char *)malloc((size_t)(w * h * 1L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = s[0];
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_YUV8) {
      unsigned char *pix, *p;

      /* '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGR8) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* reverse order: */
            p[0] = s[2];
            p[1] = s[1];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGBA8) {
      unsigned char *pix, *p;

      /* '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGRA8) {
      unsigned char *pix, *p;

      /* also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = 0xff;
            /* reverse order: */
            p[1] = s[2];
            p[2] = s[1];
            p[3] = s[0];
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_ARGB32) {
      unsigned int *pix, *p;

      /* still also '4': */
      pix = (unsigned int *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (0xff000000 | (unsigned int)(s[0] << 16) | (unsigned int)(s[1] << 8) | (s[2]));
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* another thing that is also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* back to same order again: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } /* end "if-else" that seems like it should really be a "switch" */
   return NULL;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get()
 */
extern const void *epeg_pixels_get_as_RGB8(Epeg_Image *im, int x, int y,
                                           int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
		   return NULL;
	   }
   }

   if (!im->pixels) {
	   return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* unlike the "non-'_as_RGB8'-version" of this function,
    * there are no 'else'-es separating the cases here. Also, we only go through
    * three of the enumeration values of type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace) here: */
   if (im->color_space == EPEG_GRAY8) {
      unsigned char *pix, *p;

      /* get right into the '3' ones: */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[0];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (unsigned char)(MIN(255, (s[0] * s[3]) / 255));
            p[1] = (unsigned char)(MIN(255, (s[1] * s[3]) / 255));
            p[2] = (unsigned char)(MIN(255, (s[2] * s[3]) / 255));
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   return NULL;
}

/**
 * Free requested pixel block from an image.
 * @param im A handle to an opened Epeg image (unused).
 * @param data The pointer to the image pixels.
 * @return Nothing.
 *
 * This frees the data for a block of pixels requested from image @p im.
 * @p data must be a valid (non NULL) pointer to a pixel block taken from the
 * image @p im by epeg_pixels_get() and must be called before the image is
 * closed by epeg_close().
 */
extern void epeg_pixels_free(Epeg_Image *im, const void *data)
{
#pragma unused (im)
   free((void *)data);
}

/**
 * Get the image comment field as a string.
 * @param im A handle to an opened Epeg image.
 * @return A pointer to the loaded image comments.
 *
 * This function returns the comment field as a string (NUL byte terminated)
 * of the loaded image @p im, if there is a comment, or NULL if no comment is
 * saved with the image. Consider the string returned to be read-only.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern const char *epeg_comment_get(Epeg_Image *im)
{
   return im->in.comment;
}

/**
 * Get thumbnail comments of loaded image.
 * @param im A handle to an opened Epeg image.
 * @param info Pointer to a thumbnail info struct to be filled in.
 * @return Nothing.
 *
 * This function retrieves thumbnail comments written by Epeg to any saved
 * JPEG files. If no thumbnail comments were saved, the fields will be 0 in
 * the @p info struct on return.
 *
 * See also: epeg_comment_get(), epeg_thumbnail_comments_enable()
 */
extern void epeg_thumbnail_comments_get(Epeg_Image *im,
                                        Epeg_Thumbnail_Info *info)
{
   if (!info) {
      return;
   }
   info->uri = im->in.thumb_info.uri;
   info->mtime = im->in.thumb_info.mtime;
   info->w = im->in.thumb_info.w;
   info->h = im->in.thumb_info.h;
   info->mimetype = im->in.thumb_info.mime;
}

/**
 * Set the comment field of the image for saving.
 * @param im A handle to an opened Epeg image.
 * @param comment The comment to set.
 * @return Nothing.
 *
 * Set the comment for the image file for when it gets saved. This is a NUL
 * byte terminated C string. If @p comment is NULL the output file will have
 * no comment field.
 *
 * The default comment will be any comment loaded from the input file.
 *
 * See also: epeg_comment_get()
 */
extern void epeg_comment_set(Epeg_Image *im, const char *comment)
{
   if (im->out.comment) {
      free(im->out.comment);
   }
   if (!comment) {
      im->out.comment = NULL;
   } else {
      im->out.comment = strdup(comment);
   }
}

/**
 * Set the encoding quality of the saved image.
 * @param im A handle to an opened Epeg image.
 * @param quality The quality of encoding from 0 to 100.
 * @return Nothing.
 *
 * Set the quality of the output encoded image. Values from 0 to 100
 * inclusive are valid, with 100 being the maximum quality, and 0 being the
 * minimum. If the quality is set equal to or above 90%, the output U and V
 * color planes are encoded at 1:1 with the Y plane.
 *
 * The default quality is 75.
 *
 * See also: epeg_comment_set()
 */
extern void epeg_quality_set(Epeg_Image *im, int quality)
{
   if (quality < 0) {
      quality = 0;
   } else if (quality > 100) {
      quality = 100;
   }
   im->out.quality = quality;
}

/**
 * Enable thumbnail comments in saved image.
 * @param im A handle to an opened Epeg image.
 * @param onoff A boolean on and off enabling flag.
 * @return Nothing.
 *
 * if @p onoff is 1, the output file will have thumbnail comments added to
 * it, and if it is 0, it will not. The default is 0.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern void epeg_thumbnail_comments_enable(Epeg_Image *im, int onoff)
{
   im->out.thumbnail_info = (char)onoff;
}

/**
 * Set the output file path for the image when saved.
 * @param im A handle to an opened Epeg image.
 * @param file The path to the output file.
 * @return Nothing.
 *
 * This sets the output file path name (either a full or relative path name)
 * to where the file will be written when saved. @p file must be a NUL
 * terminated C string containing the path to the file to be saved to. If it is
 * NULL, then the image will not be saved to a file when calling epeg_encode().
 *
 * See also: epeg_memory_output_set(), epeg_encode()
 */
extern void epeg_file_output_set(Epeg_Image *im, const char *file)
{
   if (im->out.file) {
      free(im->out.file);
   }
   if (!file) {
      im->out.file = NULL;
   } else {
      im->out.file = strdup(file);
   }
}

/**
 * Set the output file to be a block of allocated memory.
 * @param im A handle to an opened Epeg image.
 * @param data A pointer to a pointer to a memory block.
 * @param size A pointer to a counter of the size of the memory block.
 * @return Nothing.
 *
 * This sets the output encoding of the image when saved to be allocated
 * memory. After epeg_close() is called the pointer pointed to by @p data
 * and the integer pointed to by @p size will contain the pointer to the
 * memory block and its size in bytes, respecitvely. The memory block can be
 * freed with the free() function call. If the save fails the pointer to the
 * memory block will be unaffected, as will the size.
 *
 * See also: epeg_file_output_set(), epeg_encode()
 */
extern void epeg_memory_output_set(Epeg_Image *im, unsigned char **data,
                                   int *size)
{
   im->out.mem.data = data;
   im->out.mem.size = size;
}

/**
 * This saves the image to its specified destination.
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * This saves the image @p im to its destination specified by
 * epeg_file_output_set() or epeg_memory_output_set(). The image will be
 * encoded at the decoded pixel size, using the quality, comment,
 * and thumbnail comment settings set on the image.
 *
 * See also: epeg_file_output_set(), epeg_memory_output_set()
 */
extern int epeg_encode(Epeg_Image *im)
{
   if (_epeg_decode(im) != 0) {
      return 1;
   }
   if (_epeg_scale(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * FIXME: Document this with a short, sentence-long description of epeg_trim()
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * FIXME: Document this with a longer, paragraph-long description.
 */
extern int epeg_trim(Epeg_Image *im)
{
   if (_epeg_decode_for_trim(im) != 0) {
      return 1;
   }
   if (_epeg_trim(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * Close an image handle.
 * @param im A handle to an opened Epeg image.
 * @return Nothing.
 *
 * This closes an opened image handle and frees all memory associated with it.
 * It does NOT free encoded data generated by epeg_memory_output_set() followed
 * by epeg_encode(), nor does it guarantee to free any data received by
 * epeg_pixels_get(). Once an image handle is closed, consider it invalid.
 *
 * See also: epeg_file_open(), epeg_memory_open()
 */
extern void epeg_close(Epeg_Image *im)
{
   if (im->pixels) {
      free(im->pixels);
   }
   if (im->lines) {
      free(im->lines);
   }
   if (im->in.file) {
      free(im->in.file);
   }
   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if (im->in.f) {
      fclose(im->in.f);
   }
   if (im->in.comment) {
      free(im->in.comment);
   }
   if (im->in.thumb_info.uri) {
      free(im->in.thumb_info.uri);
   }
   if (im->in.thumb_info.mime) {
      free(im->in.thumb_info.mime);
   }
   if (im->out.file) {
      free(im->out.file);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if (im->out.f) {
      fclose(im->out.f);
   }
   if (im->out.comment) {
      free(im->out.comment);
   }
   free(im);
}

/* static internal private-only function; unnecessary to document: */
static Epeg_Image *_epeg_open_header(Epeg_Image *im)
{
   struct jpeg_marker_struct *m;

   im->in.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      error:
      epeg_close(im);
      im = NULL;
      return NULL;
   }

   jpeg_create_decompress(&(im->in.jinfo));
   jpeg_save_markers(&(im->in.jinfo), (JPEG_APP0 + 7), 1024);
   jpeg_save_markers(&(im->in.jinfo), JPEG_COM, 65535);
   jpeg_stdio_src(&(im->in.jinfo), im->in.f);
   jpeg_read_header(&(im->in.jinfo), TRUE);
   im->in.w = (int)im->in.jinfo.image_width;
   im->in.h = (int)im->in.jinfo.image_height;
   if (im->in.w <= 1) {
      goto error;
   }
   if (im->in.h <= 1) {
      goto error;
   }

   im->out.w = im->in.w;
   im->out.h = im->in.h;

   im->color_space = (((im->in.color_space = im->in.jinfo.out_color_space) == JCS_GRAYSCALE) ? EPEG_GRAY8 : EPEG_RGB8);
   if (im->in.color_space == JCS_CMYK) {
      im->color_space = EPEG_CMYK;
   }

   for ((m = im->in.jinfo.marker_list); m; (m = m->next)) {
      if (m->marker == JPEG_COM) {
         if (im->in.comment) {
            free(im->in.comment);
         }
         im->in.comment = (char *)malloc((size_t)(m->data_length + 1));
         if (im->in.comment) {
            memcpy(im->in.comment, m->data, (size_t)m->data_length);
            im->in.comment[m->data_length] = 0;
         }
      } else if (m->marker == (JPEG_APP0 + 7))  {
         if ((m->data_length > 7) &&
             (!strncmp((char *)m->data, "Thumb::", (size_t)7))) {
            char *p, *p2;

            p = (char *)malloc((size_t)(m->data_length + 1));
            if (p) {
               memcpy(p, m->data, (size_t)m->data_length);
               p[m->data_length] = 0;
               p2 = strchr(p, '\n');
               if (p2) {
                  p2[0] = 0;
                  if (!strcmp(p, "Thumb::URI")) {
                     im->in.thumb_info.uri = strdup(p2 + 1);
                  } else if (!strcmp(p, "Thumb::MTime")) {
                     sscanf((p2 + 1), "%llu", &(im->in.thumb_info.mtime));
                  } else if (!strcmp(p, "Thumb::Image::Width")) {
                     im->in.thumb_info.w = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Image::Height")) {
                     im->in.thumb_info.h = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Mimetype")) {
                     im->in.thumb_info.mime = strdup(p2 + 1);
                  }
               } /* end "if (p2)" */
               free(p);
            } /* end "if (p)" */
         }
      }
   } /* end for-loop */
   return im;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode(Epeg_Image *im)
{
   int scale, scalew, scaleh, y;

   if (im->pixels) {
      return 1;
   }

   scalew = (im->in.w / im->out.w);
   scaleh = (im->in.h / im->out.h);

   scale = scalew;
   if (scaleh < scalew) {
      scale = scaleh;
   }

   if (scale > 8) {
      scale = 8;
   } else if (scale < 1) {
      scale = 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = (unsigned int)scale;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_IFAST;

   switch (im->color_space) {
      case EPEG_GRAY8:
			 im->in.jinfo.out_color_space = JCS_GRAYSCALE;
			 im->in.jinfo.output_components = 1;
			 break;

      case EPEG_YUV8:
			 im->in.jinfo.out_color_space = JCS_YCbCr;
			 break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
			 im->in.jinfo.out_color_space = JCS_RGB;
			 break;

      case EPEG_CMYK:
			 im->in.jinfo.out_color_space = JCS_CMYK;
			 im->in.jinfo.output_components = 4;
			 break;

      default:
			 break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      epeg_close(im);
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
	   jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_scale(Epeg_Image *im)
{
   unsigned char *dst, *row, *src;
   int            x, y, w, h, i;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   for ((y = 0); (y < h); y++) {
      row = (im->pixels +
             ((((unsigned int)y * im->in.jinfo.output_height) / (unsigned int)h) * (unsigned int)im->in.jinfo.output_components * im->in.jinfo.output_width));
      dst = (im->pixels +
             ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));

      for ((x = 0); (x < im->out.w); x++) {
         src = (row +
                ((((unsigned int)x * im->in.jinfo.output_width) / (unsigned int)w) * (unsigned int)im->in.jinfo.output_components));

         for ((i = 0); (i < im->in.jinfo.output_components); i++) {
            dst[i] = src[i];
         } /* end inmost for-loop */

         dst += im->in.jinfo.output_components;
      } /* end inner for-loop */
   } /* end outer for-loop */
   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode_for_trim(Epeg_Image *im)
{
   int y;

   if (im->pixels) {
      return 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = 1;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_ISLOW;

   switch (im->color_space) {
      case EPEG_GRAY8:
		   im->in.jinfo.out_color_space = JCS_GRAYSCALE;
		   im->in.jinfo.output_components = 1;
		   break;

      case EPEG_YUV8:
		   im->in.jinfo.out_color_space = JCS_YCbCr;
		   break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
		   im->in.jinfo.out_color_space = JCS_RGB;
		   break;

      case EPEG_CMYK:
		   im->in.jinfo.out_color_space = JCS_CMYK;
		   im->in.jinfo.output_components = 4;
		   break;

      default:
		   break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
      im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
      jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_trim(Epeg_Image *im)
{
   int y, a, b, w, h;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   a = im->out.x;
   b = im->out.y;

   /* dummy condition to use value stored to 'w': */
   if (w == 0) {
	   ;
   }

   for ((y = 0); (y < h); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)((y + b) * im->in.jinfo.output_components) * im->in.jinfo.output_width)
                      + (a * im->in.jinfo.output_components));
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_encode(Epeg_Image *im)
{
   void  *data = NULL;
   size_t size = 0;

   if (im->out.f) {
      return 1;
   }

   if (im->out.file) {
      im->out.f = fopen(im->out.file, "wb");
   } else {
      im->out.f = _epeg_memfile_write_open(&data, &size);
   }
   if (!im->out.f) {
      im->error = 1;
      return 1;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_create_compress(&(im->out.jinfo));
   jpeg_stdio_dest(&(im->out.jinfo), im->out.f);
   im->out.jinfo.image_width = (JDIMENSION)im->out.w;
   im->out.jinfo.image_height = (JDIMENSION)im->out.h;
   im->out.jinfo.input_components = im->in.jinfo.output_components;
   im->out.jinfo.in_color_space = im->in.jinfo.out_color_space;
   im->out.jinfo.dct_method = JDCT_IFAST;
   im->out.jinfo.dct_method = im->in.jinfo.dct_method;
   jpeg_set_defaults(&(im->out.jinfo));
   jpeg_set_quality(&(im->out.jinfo), im->out.quality, TRUE);

   if (im->out.quality >= 90) {
      im->out.jinfo.comp_info[0].h_samp_factor = 1;
      im->out.jinfo.comp_info[0].v_samp_factor = 1;
      im->out.jinfo.comp_info[1].h_samp_factor = 1;
      im->out.jinfo.comp_info[1].v_samp_factor = 1;
      im->out.jinfo.comp_info[2].h_samp_factor = 1;
      im->out.jinfo.comp_info[2].v_samp_factor = 1;
   }
   jpeg_start_compress(&(im->out.jinfo), TRUE);

   if (im->out.comment) {
      jpeg_write_marker(&(im->out.jinfo), JPEG_COM,
                        (const JOCTET *)im->out.comment,
                        (unsigned int)strlen(im->out.comment));
   }

   if (im->out.thumbnail_info) {
      char buf[8192];

      if (im->in.file) {
         snprintf(buf, sizeof(buf), "Thumb::URI\nfile://%s", im->in.file);
         jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7),
                           (const JOCTET *)buf, (unsigned int)strlen(buf));
         snprintf(buf, sizeof(buf), "Thumb::MTime\n%llu",
#if defined(HAVE_UINTMAX_T) && !defined(__LP64__)
                  (uintmax_t)im->stat_info.st_mtime);
#else
               (unsigned long long int)im->stat_info.st_mtime);
#endif /* HAVE_UINTMAX_T && !__LP64__ */
      }
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Width\n%i", im->in.w);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Height\n%i", im->in.h);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Mimetype\nimage/jpeg");
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
   }

   while (im->out.jinfo.next_scanline < im->out.h) {
      jpeg_write_scanlines(&(im->out.jinfo),
                           &(im->lines[im->out.jinfo.next_scanline]), 1);
   }

   jpeg_finish_compress(&(im->out.jinfo));

   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if ((im->in.f) && (im->in.file)) {
      fclose(im->in.f);
   }
   if ((im->in.f) && (!im->in.file)) {
      _epeg_memfile_read_close(im->in.f);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if ((im->out.f) && (im->out.file)) {
      fclose(im->out.f);
   }
   if ((im->out.f) && (!im->out.file)) {
      _epeg_memfile_write_close(im->out.f);
   }
   im->in.f = NULL;
   im->out.f = NULL;

   if (im->out.mem.data) {
	   *(im->out.mem.data) = (unsigned char *)data;
   }
   if (im->out.mem.size) {
	   *(im->out.mem.size) = (int)size;
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static void _epeg_fatal_error_handler(j_common_ptr cinfo)
{
   emptr errmgr;

   errmgr = (emptr)cinfo->err;
   longjmp(errmgr->setjmp_buffer, 1);
   return;
}

/* silence '-Wunused-macros' warnings: */
#ifdef MAX
# undef MAX
#endif /* MAX */

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_memfile.c': */
/* epeg_memfile.c */
/* gets built into the libepeg library */

#include <stdio.h>
#include "Epeg.h"
#include "epeg_private.h"

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_read_open(void *data, size_t size)
{
#ifdef HAVE_FMEMOPEN
   return (FILE *)fmemopen(data, size, "r");
#else
   FILE *f;

   f = tmpfile();
   if (!f) {
      return NULL;
   }
   fwrite(data, size, (size_t)1L, f);
   rewind(f);
   return f;
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_read_close(FILE *f)
{
#ifdef HAVE_FMEMOPEN
   fclose(f);
#else
   /* FIXME: make these different halves of the conditional actually different
    * from one another */
   fclose(f);
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only struct and typedef; unnecessary to document: */
typedef struct _Eet_Memfile_Write_Info Eet_Memfile_Write_Info;
struct _Eet_Memfile_Write_Info
{
   FILE *f;
   void **data;
   size_t *size;
};

/* static internal private-only global variables; unnecessary to document: */
static int _epeg_memfile_info_alloc_num = 0;
static int _epeg_memfile_info_num = 0;
static Eet_Memfile_Write_Info *_epeg_memfile_info = NULL;

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_write_open(void **data, size_t *size)
{
#ifdef HAVE_OPEN_MEMSTREAM
   return open_memstream((char **)data, size);
#else
   FILE *f;

   _epeg_memfile_info_num++;
   if (_epeg_memfile_info_num > _epeg_memfile_info_alloc_num) {
      Eet_Memfile_Write_Info *tmp;

      _epeg_memfile_info_alloc_num += 16;
      tmp = (Eet_Memfile_Write_Info *)realloc(_epeg_memfile_info,
                                              ((unsigned long)_epeg_memfile_info_alloc_num *
                                               sizeof(Eet_Memfile_Write_Info)));
      if (!tmp) {
         _epeg_memfile_info_alloc_num -= 16;
         _epeg_memfile_info_num--;
         return NULL;
      }
      _epeg_memfile_info = tmp;
   }
   f = tmpfile();
   if (!f) {
      _epeg_memfile_info_num--;
      return NULL;
   }
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].f = f;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].data = data;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].size = size;
   return f;
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_write_close(FILE *f)
{
#ifdef HAVE_OPEN_MEMSTREAM
   fclose(f);
#else
   int i;

   for ((i = 0); (i < _epeg_memfile_info_num); i++) {
      if (_epeg_memfile_info[i].f == f) {
         int j;

         fseek(f, 0L, SEEK_END);
         (*(_epeg_memfile_info[i].size)) = (size_t)ftell(f);
         rewind(f);
         (*(_epeg_memfile_info[i].data)) = malloc(*(_epeg_memfile_info[i].size));
         if (!(*(_epeg_memfile_info[i].data))) {
            fclose(f);
            (*(_epeg_memfile_info[i].size)) = 0;
            return;
         }
         fread((*(_epeg_memfile_info[i].data)), (*(_epeg_memfile_info[i].size)),
               (size_t)1L, f);
         for ((j = (i + 1)); (j < _epeg_memfile_info_num); j++) {
            _epeg_memfile_info[(j - 1)] = _epeg_memfile_info[j];
         }
         _epeg_memfile_info_num--;
         fclose(f);
         return;
      } /* end "if" */
   } /* end for-loop */
   fclose(f);
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_main.c': */
/* epeg_main.c for src/lib for epeg */
/* gets built into the libepeg library */

#include <stdio.h>
#include <stdlib.h>
#include "Epeg.h"
#include "epeg_private.h"

static Epeg_Image*_epeg_open_header(Epeg_Image *im);
static int _epeg_decode(Epeg_Image *im);
static int _epeg_scale(Epeg_Image *im);
static int _epeg_decode_for_trim(Epeg_Image *im);
static int _epeg_trim(Epeg_Image *im);
static int _epeg_encode(Epeg_Image *im);

static void _epeg_fatal_error_handler(j_common_ptr cinfo);

#ifndef MIN
# define MIN(__x,__y) ((__x) < (__y) ? (__x) : (__y))
#endif /* !MIN */
#ifndef MAX
# define MAX(__x,__y) ((__x) > (__y) ? (__x) : (__y))
#endif /* !MAX */

/**
 * Open a JPEG image by filename.
 * @param file The file path to open.
 * @return A handle to the opened JPEG file, with the header decoded.
 *
 * This function opens the file indicated by the @p file parameter, and
 * attempts to decode it as a jpeg file. If this failes, NULL is returned.
 * Otherwise a valid handle to an open JPEG file is returned that can be used
 * by other Epeg calls.
 *
 * The @p file must be a pointer to a valid C string, NUL (0 byte) terminated
 * thats is a relative or absolute file path. If not results are not
 * determined.
 *
 * See also: epeg_memory_open(), epeg_close()
 */
extern Epeg_Image *epeg_file_open(const char *file)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.file = strdup(file);
   im->in.f = fopen(im->in.file, "rb");
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   fstat(fileno(im->in.f), &(im->stat_info));
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Open a JPEG image stored in memory.
 * @param data A pointer to the memory containing the JPEG data.
 * @param size The size of the memory segment containing the JPEG.
 * @return  A handle to the opened JPEG, with the header decoded.
 *
 * This function opens a JPEG file that is stored in memory pointed to by
 * @p data, and that is @p size bytes in size. If successful a valid handle
 * is returned, or on failure NULL is returned.
 *
 * See also: epeg_file_open(), epeg_close()
 */
extern Epeg_Image *epeg_memory_open(unsigned char *data, int size)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.f = _epeg_memfile_read_open(data, (size_t)size);
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Return the original JPEG pixel size.
 * @param im A handle to an opened Epeg image.
 * @param w A pointer to the width value in pixels to be filled in.
 * @param h A pointer to the height value in pixels to be filled in.
 * @return Nothing.
 *
 * Returns the image size in pixels (well not really).
 *
 * See also: epeg_colorspace_get()
 */
extern void epeg_size_get(Epeg_Image *im, int *w, int *h)
{
	if (w) {
		*w = im->in.w;
	}
	if (h) {
		*h = im->in.h;
	}
}

/**
 * Return the original JPEG pixel color space.
 * @param im A handle to an opened Epeg image.
 * @param space A pointer to the color space value to be filled in.
 * @return Nothing.
 *
 * Returns the image color space (not yet though).
 *
 * See also: epeg_size_get()
 */
extern void epeg_colorspace_get(Epeg_Image *im, int *space)
{
	if (space) {
		*space = im->color_space;
	}
}

/**
 * Set the size of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param w The width of the image to decode at, in pixels.
 * @param h The height of the image to decode at, in pixels.
 * @return Nothing.
 *
 * Sets the size at which to decode the JPEG image, giving an optimized load
 * that only decodes the pixels needed.
 *
 * See also: epeg_decode_bounds_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_size_set(Epeg_Image *im, int w, int h)
{
   if (im->pixels) {
	   return;
   }
   if (w < 1) {
	   w = 1;
   } else if (w > im->in.w) {
	   w = im->in.w;
   }
   if (h < 1) {
	   h = 1;
   } else if (h > im->in.h) {
	   h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   im->out.x = 0;
   im->out.y = 0;
}

/**
 * Set the bounds of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param x Boundary X
 * @param y Boundary Y
 * @param w Boundary W
 * @param h Boundary H
 * @return Nothing.
 *
 * Sets the bounds inside which to decode the JPEG image,
 * giving an optimized load that only decodes the bounded pixels.
 * (???)
 *
 * See also: epeg_decode_size_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_bounds_set(Epeg_Image *im, int x, int y, int w, int h)
{
   if (im->pixels) {
      return;
   }
   if (w < 1) {
      w = 1;
   } else if (w > im->in.w) {
      w = im->in.w;
   }
   if (h < 1) {
      h = 1;
   } else if (h > im->in.h) {
      h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   if (x < 0) {
      x = 0;
   }
   if (y < 0) {
      y = 0;
   }
   im->out.x = x;
   im->out.y = y;
}

/**
 * Set the colorspace in which to decode the image.
 * @param im A handle to an opened Epeg image.
 * @param colorspace The colorspace to decode the image in.
 * @return Nothing.
 *
 * This sets the colorspace to decode the image in. The default is EPEG_YUV8,
 * as this is normally the native colorspace of a JPEG file, avoiding any
 * colorspace conversions for a faster load and/or save.
 *
 * See also: epeg_decode_size_set(), epeg_decode_bounds_set()
 */
extern void epeg_decode_colorspace_set(Epeg_Image *im,
                                       Epeg_Colorspace colorspace)
{
	if (im->pixels) {
		return;
	}
	if ((colorspace < EPEG_GRAY8) || (colorspace > EPEG_ARGB32)) {
		return;
	}
	im->color_space = colorspace;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get_as_RGB8()
 */
extern const void *epeg_pixels_get(Epeg_Image *im, int x, int y,  int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
         return NULL;
      }
   }

   if (!im->pixels) {
      return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* go through all 8 values in type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace): */
   if (im->color_space == EPEG_GRAY8) {
      /* this whole block is pretty much the same thing we do for each
       * of the other enumeration values as well... could probably be simplified
       * with a function-like macro or some other sort of meta-programming: */
      unsigned char *pix, *p;

      /* '1': */
      pix = (unsigned char *)malloc((size_t)(w * h * 1L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = s[0];
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_YUV8) {
      unsigned char *pix, *p;

      /* '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGR8) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* reverse order: */
            p[0] = s[2];
            p[1] = s[1];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGBA8) {
      unsigned char *pix, *p;

      /* '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGRA8) {
      unsigned char *pix, *p;

      /* also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = 0xff;
            /* reverse order: */
            p[1] = s[2];
            p[2] = s[1];
            p[3] = s[0];
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_ARGB32) {
      unsigned int *pix, *p;

      /* still also '4': */
      pix = (unsigned int *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (0xff000000 | (unsigned int)(s[0] << 16) | (unsigned int)(s[1] << 8) | (s[2]));
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* another thing that is also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* back to same order again: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } /* end "if-else" that seems like it should really be a "switch" */
   return NULL;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get()
 */
extern const void *epeg_pixels_get_as_RGB8(Epeg_Image *im, int x, int y,
                                           int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
		   return NULL;
	   }
   }

   if (!im->pixels) {
	   return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* unlike the "non-'_as_RGB8'-version" of this function,
    * there are no 'else'-es separating the cases here. Also, we only go through
    * three of the enumeration values of type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace) here: */
   if (im->color_space == EPEG_GRAY8) {
      unsigned char *pix, *p;

      /* get right into the '3' ones: */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[0];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (unsigned char)(MIN(255, (s[0] * s[3]) / 255));
            p[1] = (unsigned char)(MIN(255, (s[1] * s[3]) / 255));
            p[2] = (unsigned char)(MIN(255, (s[2] * s[3]) / 255));
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   return NULL;
}

/**
 * Free requested pixel block from an image.
 * @param im A handle to an opened Epeg image (unused).
 * @param data The pointer to the image pixels.
 * @return Nothing.
 *
 * This frees the data for a block of pixels requested from image @p im.
 * @p data must be a valid (non NULL) pointer to a pixel block taken from the
 * image @p im by epeg_pixels_get() and must be called before the image is
 * closed by epeg_close().
 */
extern void epeg_pixels_free(Epeg_Image *im, const void *data)
{
#pragma unused (im)
   free((void *)data);
}

/**
 * Get the image comment field as a string.
 * @param im A handle to an opened Epeg image.
 * @return A pointer to the loaded image comments.
 *
 * This function returns the comment field as a string (NUL byte terminated)
 * of the loaded image @p im, if there is a comment, or NULL if no comment is
 * saved with the image. Consider the string returned to be read-only.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern const char *epeg_comment_get(Epeg_Image *im)
{
   return im->in.comment;
}

/**
 * Get thumbnail comments of loaded image.
 * @param im A handle to an opened Epeg image.
 * @param info Pointer to a thumbnail info struct to be filled in.
 * @return Nothing.
 *
 * This function retrieves thumbnail comments written by Epeg to any saved
 * JPEG files. If no thumbnail comments were saved, the fields will be 0 in
 * the @p info struct on return.
 *
 * See also: epeg_comment_get(), epeg_thumbnail_comments_enable()
 */
extern void epeg_thumbnail_comments_get(Epeg_Image *im,
                                        Epeg_Thumbnail_Info *info)
{
   if (!info) {
      return;
   }
   info->uri = im->in.thumb_info.uri;
   info->mtime = im->in.thumb_info.mtime;
   info->w = im->in.thumb_info.w;
   info->h = im->in.thumb_info.h;
   info->mimetype = im->in.thumb_info.mime;
}

/**
 * Set the comment field of the image for saving.
 * @param im A handle to an opened Epeg image.
 * @param comment The comment to set.
 * @return Nothing.
 *
 * Set the comment for the image file for when it gets saved. This is a NUL
 * byte terminated C string. If @p comment is NULL the output file will have
 * no comment field.
 *
 * The default comment will be any comment loaded from the input file.
 *
 * See also: epeg_comment_get()
 */
extern void epeg_comment_set(Epeg_Image *im, const char *comment)
{
   if (im->out.comment) {
      free(im->out.comment);
   }
   if (!comment) {
      im->out.comment = NULL;
   } else {
      im->out.comment = strdup(comment);
   }
}

/**
 * Set the encoding quality of the saved image.
 * @param im A handle to an opened Epeg image.
 * @param quality The quality of encoding from 0 to 100.
 * @return Nothing.
 *
 * Set the quality of the output encoded image. Values from 0 to 100
 * inclusive are valid, with 100 being the maximum quality, and 0 being the
 * minimum. If the quality is set equal to or above 90%, the output U and V
 * color planes are encoded at 1:1 with the Y plane.
 *
 * The default quality is 75.
 *
 * See also: epeg_comment_set()
 */
extern void epeg_quality_set(Epeg_Image *im, int quality)
{
   if (quality < 0) {
      quality = 0;
   } else if (quality > 100) {
      quality = 100;
   }
   im->out.quality = quality;
}

/**
 * Enable thumbnail comments in saved image.
 * @param im A handle to an opened Epeg image.
 * @param onoff A boolean on and off enabling flag.
 * @return Nothing.
 *
 * if @p onoff is 1, the output file will have thumbnail comments added to
 * it, and if it is 0, it will not. The default is 0.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern void epeg_thumbnail_comments_enable(Epeg_Image *im, int onoff)
{
   im->out.thumbnail_info = (char)onoff;
}

/**
 * Set the output file path for the image when saved.
 * @param im A handle to an opened Epeg image.
 * @param file The path to the output file.
 * @return Nothing.
 *
 * This sets the output file path name (either a full or relative path name)
 * to where the file will be written when saved. @p file must be a NUL
 * terminated C string containing the path to the file to be saved to. If it is
 * NULL, then the image will not be saved to a file when calling epeg_encode().
 *
 * See also: epeg_memory_output_set(), epeg_encode()
 */
extern void epeg_file_output_set(Epeg_Image *im, const char *file)
{
   if (im->out.file) {
      free(im->out.file);
   }
   if (!file) {
      im->out.file = NULL;
   } else {
      im->out.file = strdup(file);
   }
}

/**
 * Set the output file to be a block of allocated memory.
 * @param im A handle to an opened Epeg image.
 * @param data A pointer to a pointer to a memory block.
 * @param size A pointer to a counter of the size of the memory block.
 * @return Nothing.
 *
 * This sets the output encoding of the image when saved to be allocated
 * memory. After epeg_close() is called the pointer pointed to by @p data
 * and the integer pointed to by @p size will contain the pointer to the
 * memory block and its size in bytes, respecitvely. The memory block can be
 * freed with the free() function call. If the save fails the pointer to the
 * memory block will be unaffected, as will the size.
 *
 * See also: epeg_file_output_set(), epeg_encode()
 */
extern void epeg_memory_output_set(Epeg_Image *im, unsigned char **data,
                                   int *size)
{
   im->out.mem.data = data;
   im->out.mem.size = size;
}

/**
 * This saves the image to its specified destination.
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * This saves the image @p im to its destination specified by
 * epeg_file_output_set() or epeg_memory_output_set(). The image will be
 * encoded at the decoded pixel size, using the quality, comment,
 * and thumbnail comment settings set on the image.
 *
 * See also: epeg_file_output_set(), epeg_memory_output_set()
 */
extern int epeg_encode(Epeg_Image *im)
{
   if (_epeg_decode(im) != 0) {
      return 1;
   }
   if (_epeg_scale(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * FIXME: Document this with a short, sentence-long description of epeg_trim()
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * FIXME: Document this with a longer, paragraph-long description.
 */
extern int epeg_trim(Epeg_Image *im)
{
   if (_epeg_decode_for_trim(im) != 0) {
      return 1;
   }
   if (_epeg_trim(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * Close an image handle.
 * @param im A handle to an opened Epeg image.
 * @return Nothing.
 *
 * This closes an opened image handle and frees all memory associated with it.
 * It does NOT free encoded data generated by epeg_memory_output_set() followed
 * by epeg_encode(), nor does it guarantee to free any data received by
 * epeg_pixels_get(). Once an image handle is closed, consider it invalid.
 *
 * See also: epeg_file_open(), epeg_memory_open()
 */
extern void epeg_close(Epeg_Image *im)
{
   if (im->pixels) {
      free(im->pixels);
   }
   if (im->lines) {
      free(im->lines);
   }
   if (im->in.file) {
      free(im->in.file);
   }
   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if (im->in.f) {
      fclose(im->in.f);
   }
   if (im->in.comment) {
      free(im->in.comment);
   }
   if (im->in.thumb_info.uri) {
      free(im->in.thumb_info.uri);
   }
   if (im->in.thumb_info.mime) {
      free(im->in.thumb_info.mime);
   }
   if (im->out.file) {
      free(im->out.file);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if (im->out.f) {
      fclose(im->out.f);
   }
   if (im->out.comment) {
      free(im->out.comment);
   }
   free(im);
}

/* static internal private-only function; unnecessary to document: */
static Epeg_Image *_epeg_open_header(Epeg_Image *im)
{
   struct jpeg_marker_struct *m;

   im->in.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      error:
      epeg_close(im);
      im = NULL;
      return NULL;
   }

   jpeg_create_decompress(&(im->in.jinfo));
   jpeg_save_markers(&(im->in.jinfo), (JPEG_APP0 + 7), 1024);
   jpeg_save_markers(&(im->in.jinfo), JPEG_COM, 65535);
   jpeg_stdio_src(&(im->in.jinfo), im->in.f);
   jpeg_read_header(&(im->in.jinfo), TRUE);
   im->in.w = (int)im->in.jinfo.image_width;
   im->in.h = (int)im->in.jinfo.image_height;
   if (im->in.w <= 1) {
      goto error;
   }
   if (im->in.h <= 1) {
      goto error;
   }

   im->out.w = im->in.w;
   im->out.h = im->in.h;

   im->color_space = (((im->in.color_space = im->in.jinfo.out_color_space) == JCS_GRAYSCALE) ? EPEG_GRAY8 : EPEG_RGB8);
   if (im->in.color_space == JCS_CMYK) {
      im->color_space = EPEG_CMYK;
   }

   for ((m = im->in.jinfo.marker_list); m; (m = m->next)) {
      if (m->marker == JPEG_COM) {
         if (im->in.comment) {
            free(im->in.comment);
         }
         im->in.comment = (char *)malloc((size_t)(m->data_length + 1));
         if (im->in.comment) {
            memcpy(im->in.comment, m->data, (size_t)m->data_length);
            im->in.comment[m->data_length] = 0;
         }
      } else if (m->marker == (JPEG_APP0 + 7))  {
         if ((m->data_length > 7) &&
             (!strncmp((char *)m->data, "Thumb::", (size_t)7))) {
            char *p, *p2;

            p = (char *)malloc((size_t)(m->data_length + 1));
            if (p) {
               memcpy(p, m->data, (size_t)m->data_length);
               p[m->data_length] = 0;
               p2 = strchr(p, '\n');
               if (p2) {
                  p2[0] = 0;
                  if (!strcmp(p, "Thumb::URI")) {
                     im->in.thumb_info.uri = strdup(p2 + 1);
                  } else if (!strcmp(p, "Thumb::MTime")) {
                     sscanf((p2 + 1), "%llu", &(im->in.thumb_info.mtime));
                  } else if (!strcmp(p, "Thumb::Image::Width")) {
                     im->in.thumb_info.w = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Image::Height")) {
                     im->in.thumb_info.h = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Mimetype")) {
                     im->in.thumb_info.mime = strdup(p2 + 1);
                  }
               } /* end "if (p2)" */
               free(p);
            } /* end "if (p)" */
         }
      }
   } /* end for-loop */
   return im;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode(Epeg_Image *im)
{
   int scale, scalew, scaleh, y;

   if (im->pixels) {
      return 1;
   }

   scalew = (im->in.w / im->out.w);
   scaleh = (im->in.h / im->out.h);

   scale = scalew;
   if (scaleh < scalew) {
      scale = scaleh;
   }

   if (scale > 8) {
      scale = 8;
   } else if (scale < 1) {
      scale = 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = (unsigned int)scale;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_IFAST;

   switch (im->color_space) {
      case EPEG_GRAY8:
			 im->in.jinfo.out_color_space = JCS_GRAYSCALE;
			 im->in.jinfo.output_components = 1;
			 break;

      case EPEG_YUV8:
			 im->in.jinfo.out_color_space = JCS_YCbCr;
			 break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
			 im->in.jinfo.out_color_space = JCS_RGB;
			 break;

      case EPEG_CMYK:
			 im->in.jinfo.out_color_space = JCS_CMYK;
			 im->in.jinfo.output_components = 4;
			 break;

      default:
			 break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      epeg_close(im);
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
	   jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_scale(Epeg_Image *im)
{
   unsigned char *dst, *row, *src;
   int            x, y, w, h, i;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   for ((y = 0); (y < h); y++) {
      row = (im->pixels +
             ((((unsigned int)y * im->in.jinfo.output_height) / (unsigned int)h) * (unsigned int)im->in.jinfo.output_components * im->in.jinfo.output_width));
      dst = (im->pixels +
             ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));

      for ((x = 0); (x < im->out.w); x++) {
         src = (row +
                ((((unsigned int)x * im->in.jinfo.output_width) / (unsigned int)w) * (unsigned int)im->in.jinfo.output_components));

         for ((i = 0); (i < im->in.jinfo.output_components); i++) {
            dst[i] = src[i];
         } /* end inmost for-loop */

         dst += im->in.jinfo.output_components;
      } /* end inner for-loop */
   } /* end outer for-loop */
   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode_for_trim(Epeg_Image *im)
{
   int y;

   if (im->pixels) {
      return 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = 1;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_ISLOW;

   switch (im->color_space) {
      case EPEG_GRAY8:
		   im->in.jinfo.out_color_space = JCS_GRAYSCALE;
		   im->in.jinfo.output_components = 1;
		   break;

      case EPEG_YUV8:
		   im->in.jinfo.out_color_space = JCS_YCbCr;
		   break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
		   im->in.jinfo.out_color_space = JCS_RGB;
		   break;

      case EPEG_CMYK:
		   im->in.jinfo.out_color_space = JCS_CMYK;
		   im->in.jinfo.output_components = 4;
		   break;

      default:
		   break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
      im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
      jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_trim(Epeg_Image *im)
{
   int y, a, b, w, h;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   a = im->out.x;
   b = im->out.y;

   /* dummy condition to use value stored to 'w': */
   if (w == 0) {
	   ;
   }

   for ((y = 0); (y < h); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)((y + b) * im->in.jinfo.output_components) * im->in.jinfo.output_width)
                      + (a * im->in.jinfo.output_components));
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_encode(Epeg_Image *im)
{
   void  *data = NULL;
   size_t size = 0;

   if (im->out.f) {
      return 1;
   }

   if (im->out.file) {
      im->out.f = fopen(im->out.file, "wb");
   } else {
      im->out.f = _epeg_memfile_write_open(&data, &size);
   }
   if (!im->out.f) {
      im->error = 1;
      return 1;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_create_compress(&(im->out.jinfo));
   jpeg_stdio_dest(&(im->out.jinfo), im->out.f);
   im->out.jinfo.image_width = (JDIMENSION)im->out.w;
   im->out.jinfo.image_height = (JDIMENSION)im->out.h;
   im->out.jinfo.input_components = im->in.jinfo.output_components;
   im->out.jinfo.in_color_space = im->in.jinfo.out_color_space;
   im->out.jinfo.dct_method = JDCT_IFAST;
   im->out.jinfo.dct_method = im->in.jinfo.dct_method;
   jpeg_set_defaults(&(im->out.jinfo));
   jpeg_set_quality(&(im->out.jinfo), im->out.quality, TRUE);

   if (im->out.quality >= 90) {
      im->out.jinfo.comp_info[0].h_samp_factor = 1;
      im->out.jinfo.comp_info[0].v_samp_factor = 1;
      im->out.jinfo.comp_info[1].h_samp_factor = 1;
      im->out.jinfo.comp_info[1].v_samp_factor = 1;
      im->out.jinfo.comp_info[2].h_samp_factor = 1;
      im->out.jinfo.comp_info[2].v_samp_factor = 1;
   }
   jpeg_start_compress(&(im->out.jinfo), TRUE);

   if (im->out.comment) {
      jpeg_write_marker(&(im->out.jinfo), JPEG_COM,
                        (const JOCTET *)im->out.comment,
                        (unsigned int)strlen(im->out.comment));
   }

   if (im->out.thumbnail_info) {
      char buf[8192];

      if (im->in.file) {
         snprintf(buf, sizeof(buf), "Thumb::URI\nfile://%s", im->in.file);
         jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7),
                           (const JOCTET *)buf, (unsigned int)strlen(buf));
         snprintf(buf, sizeof(buf), "Thumb::MTime\n%llu",
#if defined(HAVE_UINTMAX_T) && !defined(__LP64__)
                  (uintmax_t)im->stat_info.st_mtime);
#else
               (unsigned long long int)im->stat_info.st_mtime);
#endif /* HAVE_UINTMAX_T && !__LP64__ */
      }
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Width\n%i", im->in.w);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Height\n%i", im->in.h);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Mimetype\nimage/jpeg");
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
   }

   while (im->out.jinfo.next_scanline < im->out.h) {
      jpeg_write_scanlines(&(im->out.jinfo),
                           &(im->lines[im->out.jinfo.next_scanline]), 1);
   }

   jpeg_finish_compress(&(im->out.jinfo));

   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if ((im->in.f) && (im->in.file)) {
      fclose(im->in.f);
   }
   if ((im->in.f) && (!im->in.file)) {
      _epeg_memfile_read_close(im->in.f);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if ((im->out.f) && (im->out.file)) {
      fclose(im->out.f);
   }
   if ((im->out.f) && (!im->out.file)) {
      _epeg_memfile_write_close(im->out.f);
   }
   im->in.f = NULL;
   im->out.f = NULL;

   if (im->out.mem.data) {
	   *(im->out.mem.data) = (unsigned char *)data;
   }
   if (im->out.mem.size) {
	   *(im->out.mem.size) = (int)size;
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static void _epeg_fatal_error_handler(j_common_ptr cinfo)
{
   emptr errmgr;

   errmgr = (emptr)cinfo->err;
   longjmp(errmgr->setjmp_buffer, 1);
   return;
}

/* silence '-Wunused-macros' warnings: */
#ifdef MAX
# undef MAX
#endif /* MAX */

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_memfile.c': */
/* epeg_memfile.c */
/* gets built into the libepeg library */

#include <stdio.h>
#include "Epeg.h"
#include "epeg_private.h"

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_read_open(void *data, size_t size)
{
#ifdef HAVE_FMEMOPEN
   return (FILE *)fmemopen(data, size, "r");
#else
   FILE *f;

   f = tmpfile();
   if (!f) {
      return NULL;
   }
   fwrite(data, size, (size_t)1L, f);
   rewind(f);
   return f;
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_read_close(FILE *f)
{
#ifdef HAVE_FMEMOPEN
   fclose(f);
#else
   /* FIXME: make these different halves of the conditional actually different
    * from one another */
   fclose(f);
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only struct and typedef; unnecessary to document: */
typedef struct _Eet_Memfile_Write_Info Eet_Memfile_Write_Info;
struct _Eet_Memfile_Write_Info
{
   FILE *f;
   void **data;
   size_t *size;
};

/* static internal private-only global variables; unnecessary to document: */
static int _epeg_memfile_info_alloc_num = 0;
static int _epeg_memfile_info_num = 0;
static Eet_Memfile_Write_Info *_epeg_memfile_info = NULL;

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_write_open(void **data, size_t *size)
{
#ifdef HAVE_OPEN_MEMSTREAM
   return open_memstream((char **)data, size);
#else
   FILE *f;

   _epeg_memfile_info_num++;
   if (_epeg_memfile_info_num > _epeg_memfile_info_alloc_num) {
      Eet_Memfile_Write_Info *tmp;

      _epeg_memfile_info_alloc_num += 16;
      tmp = (Eet_Memfile_Write_Info *)realloc(_epeg_memfile_info,
                                              ((unsigned long)_epeg_memfile_info_alloc_num *
                                               sizeof(Eet_Memfile_Write_Info)));
      if (!tmp) {
         _epeg_memfile_info_alloc_num -= 16;
         _epeg_memfile_info_num--;
         return NULL;
      }
      _epeg_memfile_info = tmp;
   }
   f = tmpfile();
   if (!f) {
      _epeg_memfile_info_num--;
      return NULL;
   }
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].f = f;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].data = data;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].size = size;
   return f;
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_write_close(FILE *f)
{
#ifdef HAVE_OPEN_MEMSTREAM
   fclose(f);
#else
   int i;

   for ((i = 0); (i < _epeg_memfile_info_num); i++) {
      if (_epeg_memfile_info[i].f == f) {
         int j;

         fseek(f, 0L, SEEK_END);
         (*(_epeg_memfile_info[i].size)) = (size_t)ftell(f);
         rewind(f);
         (*(_epeg_memfile_info[i].data)) = malloc(*(_epeg_memfile_info[i].size));
         if (!(*(_epeg_memfile_info[i].data))) {
            fclose(f);
            (*(_epeg_memfile_info[i].size)) = 0;
            return;
         }
         fread((*(_epeg_memfile_info[i].data)), (*(_epeg_memfile_info[i].size)),
               (size_t)1L, f);
         for ((j = (i + 1)); (j < _epeg_memfile_info_num); j++) {
            _epeg_memfile_info[(j - 1)] = _epeg_memfile_info[j];
         }
         _epeg_memfile_info_num--;
         fclose(f);
         return;
      } /* end "if" */
   } /* end for-loop */
   fclose(f);
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_main.c': */
/* epeg_main.c for src/lib for epeg */
/* gets built into the libepeg library */

#include <stdio.h>
#include <stdlib.h>
#include "Epeg.h"
#include "epeg_private.h"

static Epeg_Image*_epeg_open_header(Epeg_Image *im);
static int _epeg_decode(Epeg_Image *im);
static int _epeg_scale(Epeg_Image *im);
static int _epeg_decode_for_trim(Epeg_Image *im);
static int _epeg_trim(Epeg_Image *im);
static int _epeg_encode(Epeg_Image *im);

static void _epeg_fatal_error_handler(j_common_ptr cinfo);

#ifndef MIN
# define MIN(__x,__y) ((__x) < (__y) ? (__x) : (__y))
#endif /* !MIN */
#ifndef MAX
# define MAX(__x,__y) ((__x) > (__y) ? (__x) : (__y))
#endif /* !MAX */

/**
 * Open a JPEG image by filename.
 * @param file The file path to open.
 * @return A handle to the opened JPEG file, with the header decoded.
 *
 * This function opens the file indicated by the @p file parameter, and
 * attempts to decode it as a jpeg file. If this failes, NULL is returned.
 * Otherwise a valid handle to an open JPEG file is returned that can be used
 * by other Epeg calls.
 *
 * The @p file must be a pointer to a valid C string, NUL (0 byte) terminated
 * thats is a relative or absolute file path. If not results are not
 * determined.
 *
 * See also: epeg_memory_open(), epeg_close()
 */
extern Epeg_Image *epeg_file_open(const char *file)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.file = strdup(file);
   im->in.f = fopen(im->in.file, "rb");
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   fstat(fileno(im->in.f), &(im->stat_info));
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Open a JPEG image stored in memory.
 * @param data A pointer to the memory containing the JPEG data.
 * @param size The size of the memory segment containing the JPEG.
 * @return  A handle to the opened JPEG, with the header decoded.
 *
 * This function opens a JPEG file that is stored in memory pointed to by
 * @p data, and that is @p size bytes in size. If successful a valid handle
 * is returned, or on failure NULL is returned.
 *
 * See also: epeg_file_open(), epeg_close()
 */
extern Epeg_Image *epeg_memory_open(unsigned char *data, int size)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.f = _epeg_memfile_read_open(data, (size_t)size);
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Return the original JPEG pixel size.
 * @param im A handle to an opened Epeg image.
 * @param w A pointer to the width value in pixels to be filled in.
 * @param h A pointer to the height value in pixels to be filled in.
 * @return Nothing.
 *
 * Returns the image size in pixels (well not really).
 *
 * See also: epeg_colorspace_get()
 */
extern void epeg_size_get(Epeg_Image *im, int *w, int *h)
{
	if (w) {
		*w = im->in.w;
	}
	if (h) {
		*h = im->in.h;
	}
}

/**
 * Return the original JPEG pixel color space.
 * @param im A handle to an opened Epeg image.
 * @param space A pointer to the color space value to be filled in.
 * @return Nothing.
 *
 * Returns the image color space (not yet though).
 *
 * See also: epeg_size_get()
 */
extern void epeg_colorspace_get(Epeg_Image *im, int *space)
{
	if (space) {
		*space = im->color_space;
	}
}

/**
 * Set the size of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param w The width of the image to decode at, in pixels.
 * @param h The height of the image to decode at, in pixels.
 * @return Nothing.
 *
 * Sets the size at which to decode the JPEG image, giving an optimized load
 * that only decodes the pixels needed.
 *
 * See also: epeg_decode_bounds_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_size_set(Epeg_Image *im, int w, int h)
{
   if (im->pixels) {
	   return;
   }
   if (w < 1) {
	   w = 1;
   } else if (w > im->in.w) {
	   w = im->in.w;
   }
   if (h < 1) {
	   h = 1;
   } else if (h > im->in.h) {
	   h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   im->out.x = 0;
   im->out.y = 0;
}

/**
 * Set the bounds of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param x Boundary X
 * @param y Boundary Y
 * @param w Boundary W
 * @param h Boundary H
 * @return Nothing.
 *
 * Sets the bounds inside which to decode the JPEG image,
 * giving an optimized load that only decodes the bounded pixels.
 * (???)
 *
 * See also: epeg_decode_size_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_bounds_set(Epeg_Image *im, int x, int y, int w, int h)
{
   if (im->pixels) {
      return;
   }
   if (w < 1) {
      w = 1;
   } else if (w > im->in.w) {
      w = im->in.w;
   }
   if (h < 1) {
      h = 1;
   } else if (h > im->in.h) {
      h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   if (x < 0) {
      x = 0;
   }
   if (y < 0) {
      y = 0;
   }
   im->out.x = x;
   im->out.y = y;
}

/**
 * Set the colorspace in which to decode the image.
 * @param im A handle to an opened Epeg image.
 * @param colorspace The colorspace to decode the image in.
 * @return Nothing.
 *
 * This sets the colorspace to decode the image in. The default is EPEG_YUV8,
 * as this is normally the native colorspace of a JPEG file, avoiding any
 * colorspace conversions for a faster load and/or save.
 *
 * See also: epeg_decode_size_set(), epeg_decode_bounds_set()
 */
extern void epeg_decode_colorspace_set(Epeg_Image *im,
                                       Epeg_Colorspace colorspace)
{
	if (im->pixels) {
		return;
	}
	if ((colorspace < EPEG_GRAY8) || (colorspace > EPEG_ARGB32)) {
		return;
	}
	im->color_space = colorspace;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get_as_RGB8()
 */
extern const void *epeg_pixels_get(Epeg_Image *im, int x, int y,  int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
         return NULL;
      }
   }

   if (!im->pixels) {
      return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* go through all 8 values in type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace): */
   if (im->color_space == EPEG_GRAY8) {
      /* this whole block is pretty much the same thing we do for each
       * of the other enumeration values as well... could probably be simplified
       * with a function-like macro or some other sort of meta-programming: */
      unsigned char *pix, *p;

      /* '1': */
      pix = (unsigned char *)malloc((size_t)(w * h * 1L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = s[0];
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_YUV8) {
      unsigned char *pix, *p;

      /* '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGR8) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* reverse order: */
            p[0] = s[2];
            p[1] = s[1];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGBA8) {
      unsigned char *pix, *p;

      /* '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGRA8) {
      unsigned char *pix, *p;

      /* also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = 0xff;
            /* reverse order: */
            p[1] = s[2];
            p[2] = s[1];
            p[3] = s[0];
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_ARGB32) {
      unsigned int *pix, *p;

      /* still also '4': */
      pix = (unsigned int *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (0xff000000 | (unsigned int)(s[0] << 16) | (unsigned int)(s[1] << 8) | (s[2]));
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* another thing that is also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* back to same order again: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } /* end "if-else" that seems like it should really be a "switch" */
   return NULL;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get()
 */
extern const void *epeg_pixels_get_as_RGB8(Epeg_Image *im, int x, int y,
                                           int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
		   return NULL;
	   }
   }

   if (!im->pixels) {
	   return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* unlike the "non-'_as_RGB8'-version" of this function,
    * there are no 'else'-es separating the cases here. Also, we only go through
    * three of the enumeration values of type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace) here: */
   if (im->color_space == EPEG_GRAY8) {
      unsigned char *pix, *p;

      /* get right into the '3' ones: */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[0];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (unsigned char)(MIN(255, (s[0] * s[3]) / 255));
            p[1] = (unsigned char)(MIN(255, (s[1] * s[3]) / 255));
            p[2] = (unsigned char)(MIN(255, (s[2] * s[3]) / 255));
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   return NULL;
}

/**
 * Free requested pixel block from an image.
 * @param im A handle to an opened Epeg image (unused).
 * @param data The pointer to the image pixels.
 * @return Nothing.
 *
 * This frees the data for a block of pixels requested from image @p im.
 * @p data must be a valid (non NULL) pointer to a pixel block taken from the
 * image @p im by epeg_pixels_get() and must be called before the image is
 * closed by epeg_close().
 */
extern void epeg_pixels_free(Epeg_Image *im, const void *data)
{
#pragma unused (im)
   free((void *)data);
}

/**
 * Get the image comment field as a string.
 * @param im A handle to an opened Epeg image.
 * @return A pointer to the loaded image comments.
 *
 * This function returns the comment field as a string (NUL byte terminated)
 * of the loaded image @p im, if there is a comment, or NULL if no comment is
 * saved with the image. Consider the string returned to be read-only.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern const char *epeg_comment_get(Epeg_Image *im)
{
   return im->in.comment;
}

/**
 * Get thumbnail comments of loaded image.
 * @param im A handle to an opened Epeg image.
 * @param info Pointer to a thumbnail info struct to be filled in.
 * @return Nothing.
 *
 * This function retrieves thumbnail comments written by Epeg to any saved
 * JPEG files. If no thumbnail comments were saved, the fields will be 0 in
 * the @p info struct on return.
 *
 * See also: epeg_comment_get(), epeg_thumbnail_comments_enable()
 */
extern void epeg_thumbnail_comments_get(Epeg_Image *im,
                                        Epeg_Thumbnail_Info *info)
{
   if (!info) {
      return;
   }
   info->uri = im->in.thumb_info.uri;
   info->mtime = im->in.thumb_info.mtime;
   info->w = im->in.thumb_info.w;
   info->h = im->in.thumb_info.h;
   info->mimetype = im->in.thumb_info.mime;
}

/**
 * Set the comment field of the image for saving.
 * @param im A handle to an opened Epeg image.
 * @param comment The comment to set.
 * @return Nothing.
 *
 * Set the comment for the image file for when it gets saved. This is a NUL
 * byte terminated C string. If @p comment is NULL the output file will have
 * no comment field.
 *
 * The default comment will be any comment loaded from the input file.
 *
 * See also: epeg_comment_get()
 */
extern void epeg_comment_set(Epeg_Image *im, const char *comment)
{
   if (im->out.comment) {
      free(im->out.comment);
   }
   if (!comment) {
      im->out.comment = NULL;
   } else {
      im->out.comment = strdup(comment);
   }
}

/**
 * Set the encoding quality of the saved image.
 * @param im A handle to an opened Epeg image.
 * @param quality The quality of encoding from 0 to 100.
 * @return Nothing.
 *
 * Set the quality of the output encoded image. Values from 0 to 100
 * inclusive are valid, with 100 being the maximum quality, and 0 being the
 * minimum. If the quality is set equal to or above 90%, the output U and V
 * color planes are encoded at 1:1 with the Y plane.
 *
 * The default quality is 75.
 *
 * See also: epeg_comment_set()
 */
extern void epeg_quality_set(Epeg_Image *im, int quality)
{
   if (quality < 0) {
      quality = 0;
   } else if (quality > 100) {
      quality = 100;
   }
   im->out.quality = quality;
}

/**
 * Enable thumbnail comments in saved image.
 * @param im A handle to an opened Epeg image.
 * @param onoff A boolean on and off enabling flag.
 * @return Nothing.
 *
 * if @p onoff is 1, the output file will have thumbnail comments added to
 * it, and if it is 0, it will not. The default is 0.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern void epeg_thumbnail_comments_enable(Epeg_Image *im, int onoff)
{
   im->out.thumbnail_info = (char)onoff;
}

/**
 * Set the output file path for the image when saved.
 * @param im A handle to an opened Epeg image.
 * @param file The path to the output file.
 * @return Nothing.
 *
 * This sets the output file path name (either a full or relative path name)
 * to where the file will be written when saved. @p file must be a NUL
 * terminated C string containing the path to the file to be saved to. If it is
 * NULL, then the image will not be saved to a file when calling epeg_encode().
 *
 * See also: epeg_memory_output_set(), epeg_encode()
 */
extern void epeg_file_output_set(Epeg_Image *im, const char *file)
{
   if (im->out.file) {
      free(im->out.file);
   }
   if (!file) {
      im->out.file = NULL;
   } else {
      im->out.file = strdup(file);
   }
}

/**
 * Set the output file to be a block of allocated memory.
 * @param im A handle to an opened Epeg image.
 * @param data A pointer to a pointer to a memory block.
 * @param size A pointer to a counter of the size of the memory block.
 * @return Nothing.
 *
 * This sets the output encoding of the image when saved to be allocated
 * memory. After epeg_close() is called the pointer pointed to by @p data
 * and the integer pointed to by @p size will contain the pointer to the
 * memory block and its size in bytes, respecitvely. The memory block can be
 * freed with the free() function call. If the save fails the pointer to the
 * memory block will be unaffected, as will the size.
 *
 * See also: epeg_file_output_set(), epeg_encode()
 */
extern void epeg_memory_output_set(Epeg_Image *im, unsigned char **data,
                                   int *size)
{
   im->out.mem.data = data;
   im->out.mem.size = size;
}

/**
 * This saves the image to its specified destination.
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * This saves the image @p im to its destination specified by
 * epeg_file_output_set() or epeg_memory_output_set(). The image will be
 * encoded at the decoded pixel size, using the quality, comment,
 * and thumbnail comment settings set on the image.
 *
 * See also: epeg_file_output_set(), epeg_memory_output_set()
 */
extern int epeg_encode(Epeg_Image *im)
{
   if (_epeg_decode(im) != 0) {
      return 1;
   }
   if (_epeg_scale(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * FIXME: Document this with a short, sentence-long description of epeg_trim()
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * FIXME: Document this with a longer, paragraph-long description.
 */
extern int epeg_trim(Epeg_Image *im)
{
   if (_epeg_decode_for_trim(im) != 0) {
      return 1;
   }
   if (_epeg_trim(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * Close an image handle.
 * @param im A handle to an opened Epeg image.
 * @return Nothing.
 *
 * This closes an opened image handle and frees all memory associated with it.
 * It does NOT free encoded data generated by epeg_memory_output_set() followed
 * by epeg_encode(), nor does it guarantee to free any data received by
 * epeg_pixels_get(). Once an image handle is closed, consider it invalid.
 *
 * See also: epeg_file_open(), epeg_memory_open()
 */
extern void epeg_close(Epeg_Image *im)
{
   if (im->pixels) {
      free(im->pixels);
   }
   if (im->lines) {
      free(im->lines);
   }
   if (im->in.file) {
      free(im->in.file);
   }
   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if (im->in.f) {
      fclose(im->in.f);
   }
   if (im->in.comment) {
      free(im->in.comment);
   }
   if (im->in.thumb_info.uri) {
      free(im->in.thumb_info.uri);
   }
   if (im->in.thumb_info.mime) {
      free(im->in.thumb_info.mime);
   }
   if (im->out.file) {
      free(im->out.file);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if (im->out.f) {
      fclose(im->out.f);
   }
   if (im->out.comment) {
      free(im->out.comment);
   }
   free(im);
}

/* static internal private-only function; unnecessary to document: */
static Epeg_Image *_epeg_open_header(Epeg_Image *im)
{
   struct jpeg_marker_struct *m;

   im->in.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      error:
      epeg_close(im);
      im = NULL;
      return NULL;
   }

   jpeg_create_decompress(&(im->in.jinfo));
   jpeg_save_markers(&(im->in.jinfo), (JPEG_APP0 + 7), 1024);
   jpeg_save_markers(&(im->in.jinfo), JPEG_COM, 65535);
   jpeg_stdio_src(&(im->in.jinfo), im->in.f);
   jpeg_read_header(&(im->in.jinfo), TRUE);
   im->in.w = (int)im->in.jinfo.image_width;
   im->in.h = (int)im->in.jinfo.image_height;
   if (im->in.w <= 1) {
      goto error;
   }
   if (im->in.h <= 1) {
      goto error;
   }

   im->out.w = im->in.w;
   im->out.h = im->in.h;

   im->color_space = (((im->in.color_space = im->in.jinfo.out_color_space) == JCS_GRAYSCALE) ? EPEG_GRAY8 : EPEG_RGB8);
   if (im->in.color_space == JCS_CMYK) {
      im->color_space = EPEG_CMYK;
   }

   for ((m = im->in.jinfo.marker_list); m; (m = m->next)) {
      if (m->marker == JPEG_COM) {
         if (im->in.comment) {
            free(im->in.comment);
         }
         im->in.comment = (char *)malloc((size_t)(m->data_length + 1));
         if (im->in.comment) {
            memcpy(im->in.comment, m->data, (size_t)m->data_length);
            im->in.comment[m->data_length] = 0;
         }
      } else if (m->marker == (JPEG_APP0 + 7))  {
         if ((m->data_length > 7) &&
             (!strncmp((char *)m->data, "Thumb::", (size_t)7))) {
            char *p, *p2;

            p = (char *)malloc((size_t)(m->data_length + 1));
            if (p) {
               memcpy(p, m->data, (size_t)m->data_length);
               p[m->data_length] = 0;
               p2 = strchr(p, '\n');
               if (p2) {
                  p2[0] = 0;
                  if (!strcmp(p, "Thumb::URI")) {
                     im->in.thumb_info.uri = strdup(p2 + 1);
                  } else if (!strcmp(p, "Thumb::MTime")) {
                     sscanf((p2 + 1), "%llu", &(im->in.thumb_info.mtime));
                  } else if (!strcmp(p, "Thumb::Image::Width")) {
                     im->in.thumb_info.w = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Image::Height")) {
                     im->in.thumb_info.h = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Mimetype")) {
                     im->in.thumb_info.mime = strdup(p2 + 1);
                  }
               } /* end "if (p2)" */
               free(p);
            } /* end "if (p)" */
         }
      }
   } /* end for-loop */
   return im;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode(Epeg_Image *im)
{
   int scale, scalew, scaleh, y;

   if (im->pixels) {
      return 1;
   }

   scalew = (im->in.w / im->out.w);
   scaleh = (im->in.h / im->out.h);

   scale = scalew;
   if (scaleh < scalew) {
      scale = scaleh;
   }

   if (scale > 8) {
      scale = 8;
   } else if (scale < 1) {
      scale = 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = (unsigned int)scale;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_IFAST;

   switch (im->color_space) {
      case EPEG_GRAY8:
			 im->in.jinfo.out_color_space = JCS_GRAYSCALE;
			 im->in.jinfo.output_components = 1;
			 break;

      case EPEG_YUV8:
			 im->in.jinfo.out_color_space = JCS_YCbCr;
			 break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
			 im->in.jinfo.out_color_space = JCS_RGB;
			 break;

      case EPEG_CMYK:
			 im->in.jinfo.out_color_space = JCS_CMYK;
			 im->in.jinfo.output_components = 4;
			 break;

      default:
			 break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      epeg_close(im);
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
	   jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_scale(Epeg_Image *im)
{
   unsigned char *dst, *row, *src;
   int            x, y, w, h, i;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   for ((y = 0); (y < h); y++) {
      row = (im->pixels +
             ((((unsigned int)y * im->in.jinfo.output_height) / (unsigned int)h) * (unsigned int)im->in.jinfo.output_components * im->in.jinfo.output_width));
      dst = (im->pixels +
             ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));

      for ((x = 0); (x < im->out.w); x++) {
         src = (row +
                ((((unsigned int)x * im->in.jinfo.output_width) / (unsigned int)w) * (unsigned int)im->in.jinfo.output_components));

         for ((i = 0); (i < im->in.jinfo.output_components); i++) {
            dst[i] = src[i];
         } /* end inmost for-loop */

         dst += im->in.jinfo.output_components;
      } /* end inner for-loop */
   } /* end outer for-loop */
   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode_for_trim(Epeg_Image *im)
{
   int y;

   if (im->pixels) {
      return 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = 1;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_ISLOW;

   switch (im->color_space) {
      case EPEG_GRAY8:
		   im->in.jinfo.out_color_space = JCS_GRAYSCALE;
		   im->in.jinfo.output_components = 1;
		   break;

      case EPEG_YUV8:
		   im->in.jinfo.out_color_space = JCS_YCbCr;
		   break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
		   im->in.jinfo.out_color_space = JCS_RGB;
		   break;

      case EPEG_CMYK:
		   im->in.jinfo.out_color_space = JCS_CMYK;
		   im->in.jinfo.output_components = 4;
		   break;

      default:
		   break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
      im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
      jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_trim(Epeg_Image *im)
{
   int y, a, b, w, h;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   a = im->out.x;
   b = im->out.y;

   /* dummy condition to use value stored to 'w': */
   if (w == 0) {
	   ;
   }

   for ((y = 0); (y < h); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)((y + b) * im->in.jinfo.output_components) * im->in.jinfo.output_width)
                      + (a * im->in.jinfo.output_components));
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_encode(Epeg_Image *im)
{
   void  *data = NULL;
   size_t size = 0;

   if (im->out.f) {
      return 1;
   }

   if (im->out.file) {
      im->out.f = fopen(im->out.file, "wb");
   } else {
      im->out.f = _epeg_memfile_write_open(&data, &size);
   }
   if (!im->out.f) {
      im->error = 1;
      return 1;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_create_compress(&(im->out.jinfo));
   jpeg_stdio_dest(&(im->out.jinfo), im->out.f);
   im->out.jinfo.image_width = (JDIMENSION)im->out.w;
   im->out.jinfo.image_height = (JDIMENSION)im->out.h;
   im->out.jinfo.input_components = im->in.jinfo.output_components;
   im->out.jinfo.in_color_space = im->in.jinfo.out_color_space;
   im->out.jinfo.dct_method = JDCT_IFAST;
   im->out.jinfo.dct_method = im->in.jinfo.dct_method;
   jpeg_set_defaults(&(im->out.jinfo));
   jpeg_set_quality(&(im->out.jinfo), im->out.quality, TRUE);

   if (im->out.quality >= 90) {
      im->out.jinfo.comp_info[0].h_samp_factor = 1;
      im->out.jinfo.comp_info[0].v_samp_factor = 1;
      im->out.jinfo.comp_info[1].h_samp_factor = 1;
      im->out.jinfo.comp_info[1].v_samp_factor = 1;
      im->out.jinfo.comp_info[2].h_samp_factor = 1;
      im->out.jinfo.comp_info[2].v_samp_factor = 1;
   }
   jpeg_start_compress(&(im->out.jinfo), TRUE);

   if (im->out.comment) {
      jpeg_write_marker(&(im->out.jinfo), JPEG_COM,
                        (const JOCTET *)im->out.comment,
                        (unsigned int)strlen(im->out.comment));
   }

   if (im->out.thumbnail_info) {
      char buf[8192];

      if (im->in.file) {
         snprintf(buf, sizeof(buf), "Thumb::URI\nfile://%s", im->in.file);
         jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7),
                           (const JOCTET *)buf, (unsigned int)strlen(buf));
         snprintf(buf, sizeof(buf), "Thumb::MTime\n%llu",
#if defined(HAVE_UINTMAX_T) && !defined(__LP64__)
                  (uintmax_t)im->stat_info.st_mtime);
#else
               (unsigned long long int)im->stat_info.st_mtime);
#endif /* HAVE_UINTMAX_T && !__LP64__ */
      }
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Width\n%i", im->in.w);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Height\n%i", im->in.h);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Mimetype\nimage/jpeg");
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
   }

   while (im->out.jinfo.next_scanline < im->out.h) {
      jpeg_write_scanlines(&(im->out.jinfo),
                           &(im->lines[im->out.jinfo.next_scanline]), 1);
   }

   jpeg_finish_compress(&(im->out.jinfo));

   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if ((im->in.f) && (im->in.file)) {
      fclose(im->in.f);
   }
   if ((im->in.f) && (!im->in.file)) {
      _epeg_memfile_read_close(im->in.f);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if ((im->out.f) && (im->out.file)) {
      fclose(im->out.f);
   }
   if ((im->out.f) && (!im->out.file)) {
      _epeg_memfile_write_close(im->out.f);
   }
   im->in.f = NULL;
   im->out.f = NULL;

   if (im->out.mem.data) {
	   *(im->out.mem.data) = (unsigned char *)data;
   }
   if (im->out.mem.size) {
	   *(im->out.mem.size) = (int)size;
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static void _epeg_fatal_error_handler(j_common_ptr cinfo)
{
   emptr errmgr;

   errmgr = (emptr)cinfo->err;
   longjmp(errmgr->setjmp_buffer, 1);
   return;
}

/* silence '-Wunused-macros' warnings: */
#ifdef MAX
# undef MAX
#endif /* MAX */

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_memfile.c': */
/* epeg_memfile.c */
/* gets built into the libepeg library */

#include <stdio.h>
#include "Epeg.h"
#include "epeg_private.h"

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_read_open(void *data, size_t size)
{
#ifdef HAVE_FMEMOPEN
   return (FILE *)fmemopen(data, size, "r");
#else
   FILE *f;

   f = tmpfile();
   if (!f) {
      return NULL;
   }
   fwrite(data, size, (size_t)1L, f);
   rewind(f);
   return f;
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_read_close(FILE *f)
{
#ifdef HAVE_FMEMOPEN
   fclose(f);
#else
   /* FIXME: make these different halves of the conditional actually different
    * from one another */
   fclose(f);
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only struct and typedef; unnecessary to document: */
typedef struct _Eet_Memfile_Write_Info Eet_Memfile_Write_Info;
struct _Eet_Memfile_Write_Info
{
   FILE *f;
   void **data;
   size_t *size;
};

/* static internal private-only global variables; unnecessary to document: */
static int _epeg_memfile_info_alloc_num = 0;
static int _epeg_memfile_info_num = 0;
static Eet_Memfile_Write_Info *_epeg_memfile_info = NULL;

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_write_open(void **data, size_t *size)
{
#ifdef HAVE_OPEN_MEMSTREAM
   return open_memstream((char **)data, size);
#else
   FILE *f;

   _epeg_memfile_info_num++;
   if (_epeg_memfile_info_num > _epeg_memfile_info_alloc_num) {
      Eet_Memfile_Write_Info *tmp;

      _epeg_memfile_info_alloc_num += 16;
      tmp = (Eet_Memfile_Write_Info *)realloc(_epeg_memfile_info,
                                              ((unsigned long)_epeg_memfile_info_alloc_num *
                                               sizeof(Eet_Memfile_Write_Info)));
      if (!tmp) {
         _epeg_memfile_info_alloc_num -= 16;
         _epeg_memfile_info_num--;
         return NULL;
      }
      _epeg_memfile_info = tmp;
   }
   f = tmpfile();
   if (!f) {
      _epeg_memfile_info_num--;
      return NULL;
   }
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].f = f;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].data = data;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].size = size;
   return f;
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_write_close(FILE *f)
{
#ifdef HAVE_OPEN_MEMSTREAM
   fclose(f);
#else
   int i;

   for ((i = 0); (i < _epeg_memfile_info_num); i++) {
      if (_epeg_memfile_info[i].f == f) {
         int j;

         fseek(f, 0L, SEEK_END);
         (*(_epeg_memfile_info[i].size)) = (size_t)ftell(f);
         rewind(f);
         (*(_epeg_memfile_info[i].data)) = malloc(*(_epeg_memfile_info[i].size));
         if (!(*(_epeg_memfile_info[i].data))) {
            fclose(f);
            (*(_epeg_memfile_info[i].size)) = 0;
            return;
         }
         fread((*(_epeg_memfile_info[i].data)), (*(_epeg_memfile_info[i].size)),
               (size_t)1L, f);
         for ((j = (i + 1)); (j < _epeg_memfile_info_num); j++) {
            _epeg_memfile_info[(j - 1)] = _epeg_memfile_info[j];
         }
         _epeg_memfile_info_num--;
         fclose(f);
         return;
      } /* end "if" */
   } /* end for-loop */
   fclose(f);
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_main.c': */
/* epeg_main.c for src/lib for epeg */
/* gets built into the libepeg library */

#include <stdio.h>
#include <stdlib.h>
#include "Epeg.h"
#include "epeg_private.h"

static Epeg_Image*_epeg_open_header(Epeg_Image *im);
static int _epeg_decode(Epeg_Image *im);
static int _epeg_scale(Epeg_Image *im);
static int _epeg_decode_for_trim(Epeg_Image *im);
static int _epeg_trim(Epeg_Image *im);
static int _epeg_encode(Epeg_Image *im);

static void _epeg_fatal_error_handler(j_common_ptr cinfo);

#ifndef MIN
# define MIN(__x,__y) ((__x) < (__y) ? (__x) : (__y))
#endif /* !MIN */
#ifndef MAX
# define MAX(__x,__y) ((__x) > (__y) ? (__x) : (__y))
#endif /* !MAX */

/**
 * Open a JPEG image by filename.
 * @param file The file path to open.
 * @return A handle to the opened JPEG file, with the header decoded.
 *
 * This function opens the file indicated by the @p file parameter, and
 * attempts to decode it as a jpeg file. If this failes, NULL is returned.
 * Otherwise a valid handle to an open JPEG file is returned that can be used
 * by other Epeg calls.
 *
 * The @p file must be a pointer to a valid C string, NUL (0 byte) terminated
 * thats is a relative or absolute file path. If not results are not
 * determined.
 *
 * See also: epeg_memory_open(), epeg_close()
 */
extern Epeg_Image *epeg_file_open(const char *file)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.file = strdup(file);
   im->in.f = fopen(im->in.file, "rb");
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   fstat(fileno(im->in.f), &(im->stat_info));
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Open a JPEG image stored in memory.
 * @param data A pointer to the memory containing the JPEG data.
 * @param size The size of the memory segment containing the JPEG.
 * @return  A handle to the opened JPEG, with the header decoded.
 *
 * This function opens a JPEG file that is stored in memory pointed to by
 * @p data, and that is @p size bytes in size. If successful a valid handle
 * is returned, or on failure NULL is returned.
 *
 * See also: epeg_file_open(), epeg_close()
 */
extern Epeg_Image *epeg_memory_open(unsigned char *data, int size)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.f = _epeg_memfile_read_open(data, (size_t)size);
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Return the original JPEG pixel size.
 * @param im A handle to an opened Epeg image.
 * @param w A pointer to the width value in pixels to be filled in.
 * @param h A pointer to the height value in pixels to be filled in.
 * @return Nothing.
 *
 * Returns the image size in pixels (well not really).
 *
 * See also: epeg_colorspace_get()
 */
extern void epeg_size_get(Epeg_Image *im, int *w, int *h)
{
	if (w) {
		*w = im->in.w;
	}
	if (h) {
		*h = im->in.h;
	}
}

/**
 * Return the original JPEG pixel color space.
 * @param im A handle to an opened Epeg image.
 * @param space A pointer to the color space value to be filled in.
 * @return Nothing.
 *
 * Returns the image color space (not yet though).
 *
 * See also: epeg_size_get()
 */
extern void epeg_colorspace_get(Epeg_Image *im, int *space)
{
	if (space) {
		*space = im->color_space;
	}
}

/**
 * Set the size of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param w The width of the image to decode at, in pixels.
 * @param h The height of the image to decode at, in pixels.
 * @return Nothing.
 *
 * Sets the size at which to decode the JPEG image, giving an optimized load
 * that only decodes the pixels needed.
 *
 * See also: epeg_decode_bounds_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_size_set(Epeg_Image *im, int w, int h)
{
   if (im->pixels) {
	   return;
   }
   if (w < 1) {
	   w = 1;
   } else if (w > im->in.w) {
	   w = im->in.w;
   }
   if (h < 1) {
	   h = 1;
   } else if (h > im->in.h) {
	   h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   im->out.x = 0;
   im->out.y = 0;
}

/**
 * Set the bounds of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param x Boundary X
 * @param y Boundary Y
 * @param w Boundary W
 * @param h Boundary H
 * @return Nothing.
 *
 * Sets the bounds inside which to decode the JPEG image,
 * giving an optimized load that only decodes the bounded pixels.
 * (???)
 *
 * See also: epeg_decode_size_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_bounds_set(Epeg_Image *im, int x, int y, int w, int h)
{
   if (im->pixels) {
      return;
   }
   if (w < 1) {
      w = 1;
   } else if (w > im->in.w) {
      w = im->in.w;
   }
   if (h < 1) {
      h = 1;
   } else if (h > im->in.h) {
      h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   if (x < 0) {
      x = 0;
   }
   if (y < 0) {
      y = 0;
   }
   im->out.x = x;
   im->out.y = y;
}

/**
 * Set the colorspace in which to decode the image.
 * @param im A handle to an opened Epeg image.
 * @param colorspace The colorspace to decode the image in.
 * @return Nothing.
 *
 * This sets the colorspace to decode the image in. The default is EPEG_YUV8,
 * as this is normally the native colorspace of a JPEG file, avoiding any
 * colorspace conversions for a faster load and/or save.
 *
 * See also: epeg_decode_size_set(), epeg_decode_bounds_set()
 */
extern void epeg_decode_colorspace_set(Epeg_Image *im,
                                       Epeg_Colorspace colorspace)
{
	if (im->pixels) {
		return;
	}
	if ((colorspace < EPEG_GRAY8) || (colorspace > EPEG_ARGB32)) {
		return;
	}
	im->color_space = colorspace;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get_as_RGB8()
 */
extern const void *epeg_pixels_get(Epeg_Image *im, int x, int y,  int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
         return NULL;
      }
   }

   if (!im->pixels) {
      return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* go through all 8 values in type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace): */
   if (im->color_space == EPEG_GRAY8) {
      /* this whole block is pretty much the same thing we do for each
       * of the other enumeration values as well... could probably be simplified
       * with a function-like macro or some other sort of meta-programming: */
      unsigned char *pix, *p;

      /* '1': */
      pix = (unsigned char *)malloc((size_t)(w * h * 1L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = s[0];
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_YUV8) {
      unsigned char *pix, *p;

      /* '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGR8) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* reverse order: */
            p[0] = s[2];
            p[1] = s[1];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGBA8) {
      unsigned char *pix, *p;

      /* '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGRA8) {
      unsigned char *pix, *p;

      /* also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = 0xff;
            /* reverse order: */
            p[1] = s[2];
            p[2] = s[1];
            p[3] = s[0];
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_ARGB32) {
      unsigned int *pix, *p;

      /* still also '4': */
      pix = (unsigned int *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (0xff000000 | (unsigned int)(s[0] << 16) | (unsigned int)(s[1] << 8) | (s[2]));
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* another thing that is also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* back to same order again: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } /* end "if-else" that seems like it should really be a "switch" */
   return NULL;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get()
 */
extern const void *epeg_pixels_get_as_RGB8(Epeg_Image *im, int x, int y,
                                           int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
		   return NULL;
	   }
   }

   if (!im->pixels) {
	   return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* unlike the "non-'_as_RGB8'-version" of this function,
    * there are no 'else'-es separating the cases here. Also, we only go through
    * three of the enumeration values of type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace) here: */
   if (im->color_space == EPEG_GRAY8) {
      unsigned char *pix, *p;

      /* get right into the '3' ones: */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[0];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (unsigned char)(MIN(255, (s[0] * s[3]) / 255));
            p[1] = (unsigned char)(MIN(255, (s[1] * s[3]) / 255));
            p[2] = (unsigned char)(MIN(255, (s[2] * s[3]) / 255));
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   return NULL;
}

/**
 * Free requested pixel block from an image.
 * @param im A handle to an opened Epeg image (unused).
 * @param data The pointer to the image pixels.
 * @return Nothing.
 *
 * This frees the data for a block of pixels requested from image @p im.
 * @p data must be a valid (non NULL) pointer to a pixel block taken from the
 * image @p im by epeg_pixels_get() and must be called before the image is
 * closed by epeg_close().
 */
extern void epeg_pixels_free(Epeg_Image *im, const void *data)
{
#pragma unused (im)
   free((void *)data);
}

/**
 * Get the image comment field as a string.
 * @param im A handle to an opened Epeg image.
 * @return A pointer to the loaded image comments.
 *
 * This function returns the comment field as a string (NUL byte terminated)
 * of the loaded image @p im, if there is a comment, or NULL if no comment is
 * saved with the image. Consider the string returned to be read-only.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern const char *epeg_comment_get(Epeg_Image *im)
{
   return im->in.comment;
}

/**
 * Get thumbnail comments of loaded image.
 * @param im A handle to an opened Epeg image.
 * @param info Pointer to a thumbnail info struct to be filled in.
 * @return Nothing.
 *
 * This function retrieves thumbnail comments written by Epeg to any saved
 * JPEG files. If no thumbnail comments were saved, the fields will be 0 in
 * the @p info struct on return.
 *
 * See also: epeg_comment_get(), epeg_thumbnail_comments_enable()
 */
extern void epeg_thumbnail_comments_get(Epeg_Image *im,
                                        Epeg_Thumbnail_Info *info)
{
   if (!info) {
      return;
   }
   info->uri = im->in.thumb_info.uri;
   info->mtime = im->in.thumb_info.mtime;
   info->w = im->in.thumb_info.w;
   info->h = im->in.thumb_info.h;
   info->mimetype = im->in.thumb_info.mime;
}

/**
 * Set the comment field of the image for saving.
 * @param im A handle to an opened Epeg image.
 * @param comment The comment to set.
 * @return Nothing.
 *
 * Set the comment for the image file for when it gets saved. This is a NUL
 * byte terminated C string. If @p comment is NULL the output file will have
 * no comment field.
 *
 * The default comment will be any comment loaded from the input file.
 *
 * See also: epeg_comment_get()
 */
extern void epeg_comment_set(Epeg_Image *im, const char *comment)
{
   if (im->out.comment) {
      free(im->out.comment);
   }
   if (!comment) {
      im->out.comment = NULL;
   } else {
      im->out.comment = strdup(comment);
   }
}

/**
 * Set the encoding quality of the saved image.
 * @param im A handle to an opened Epeg image.
 * @param quality The quality of encoding from 0 to 100.
 * @return Nothing.
 *
 * Set the quality of the output encoded image. Values from 0 to 100
 * inclusive are valid, with 100 being the maximum quality, and 0 being the
 * minimum. If the quality is set equal to or above 90%, the output U and V
 * color planes are encoded at 1:1 with the Y plane.
 *
 * The default quality is 75.
 *
 * See also: epeg_comment_set()
 */
extern void epeg_quality_set(Epeg_Image *im, int quality)
{
   if (quality < 0) {
      quality = 0;
   } else if (quality > 100) {
      quality = 100;
   }
   im->out.quality = quality;
}

/**
 * Enable thumbnail comments in saved image.
 * @param im A handle to an opened Epeg image.
 * @param onoff A boolean on and off enabling flag.
 * @return Nothing.
 *
 * if @p onoff is 1, the output file will have thumbnail comments added to
 * it, and if it is 0, it will not. The default is 0.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern void epeg_thumbnail_comments_enable(Epeg_Image *im, int onoff)
{
   im->out.thumbnail_info = (char)onoff;
}

/**
 * Set the output file path for the image when saved.
 * @param im A handle to an opened Epeg image.
 * @param file The path to the output file.
 * @return Nothing.
 *
 * This sets the output file path name (either a full or relative path name)
 * to where the file will be written when saved. @p file must be a NUL
 * terminated C string containing the path to the file to be saved to. If it is
 * NULL, then the image will not be saved to a file when calling epeg_encode().
 *
 * See also: epeg_memory_output_set(), epeg_encode()
 */
extern void epeg_file_output_set(Epeg_Image *im, const char *file)
{
   if (im->out.file) {
      free(im->out.file);
   }
   if (!file) {
      im->out.file = NULL;
   } else {
      im->out.file = strdup(file);
   }
}

/**
 * Set the output file to be a block of allocated memory.
 * @param im A handle to an opened Epeg image.
 * @param data A pointer to a pointer to a memory block.
 * @param size A pointer to a counter of the size of the memory block.
 * @return Nothing.
 *
 * This sets the output encoding of the image when saved to be allocated
 * memory. After epeg_close() is called the pointer pointed to by @p data
 * and the integer pointed to by @p size will contain the pointer to the
 * memory block and its size in bytes, respecitvely. The memory block can be
 * freed with the free() function call. If the save fails the pointer to the
 * memory block will be unaffected, as will the size.
 *
 * See also: epeg_file_output_set(), epeg_encode()
 */
extern void epeg_memory_output_set(Epeg_Image *im, unsigned char **data,
                                   int *size)
{
   im->out.mem.data = data;
   im->out.mem.size = size;
}

/**
 * This saves the image to its specified destination.
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * This saves the image @p im to its destination specified by
 * epeg_file_output_set() or epeg_memory_output_set(). The image will be
 * encoded at the decoded pixel size, using the quality, comment,
 * and thumbnail comment settings set on the image.
 *
 * See also: epeg_file_output_set(), epeg_memory_output_set()
 */
extern int epeg_encode(Epeg_Image *im)
{
   if (_epeg_decode(im) != 0) {
      return 1;
   }
   if (_epeg_scale(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * FIXME: Document this with a short, sentence-long description of epeg_trim()
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * FIXME: Document this with a longer, paragraph-long description.
 */
extern int epeg_trim(Epeg_Image *im)
{
   if (_epeg_decode_for_trim(im) != 0) {
      return 1;
   }
   if (_epeg_trim(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * Close an image handle.
 * @param im A handle to an opened Epeg image.
 * @return Nothing.
 *
 * This closes an opened image handle and frees all memory associated with it.
 * It does NOT free encoded data generated by epeg_memory_output_set() followed
 * by epeg_encode(), nor does it guarantee to free any data received by
 * epeg_pixels_get(). Once an image handle is closed, consider it invalid.
 *
 * See also: epeg_file_open(), epeg_memory_open()
 */
extern void epeg_close(Epeg_Image *im)
{
   if (im->pixels) {
      free(im->pixels);
   }
   if (im->lines) {
      free(im->lines);
   }
   if (im->in.file) {
      free(im->in.file);
   }
   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if (im->in.f) {
      fclose(im->in.f);
   }
   if (im->in.comment) {
      free(im->in.comment);
   }
   if (im->in.thumb_info.uri) {
      free(im->in.thumb_info.uri);
   }
   if (im->in.thumb_info.mime) {
      free(im->in.thumb_info.mime);
   }
   if (im->out.file) {
      free(im->out.file);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if (im->out.f) {
      fclose(im->out.f);
   }
   if (im->out.comment) {
      free(im->out.comment);
   }
   free(im);
}

/* static internal private-only function; unnecessary to document: */
static Epeg_Image *_epeg_open_header(Epeg_Image *im)
{
   struct jpeg_marker_struct *m;

   im->in.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      error:
      epeg_close(im);
      im = NULL;
      return NULL;
   }

   jpeg_create_decompress(&(im->in.jinfo));
   jpeg_save_markers(&(im->in.jinfo), (JPEG_APP0 + 7), 1024);
   jpeg_save_markers(&(im->in.jinfo), JPEG_COM, 65535);
   jpeg_stdio_src(&(im->in.jinfo), im->in.f);
   jpeg_read_header(&(im->in.jinfo), TRUE);
   im->in.w = (int)im->in.jinfo.image_width;
   im->in.h = (int)im->in.jinfo.image_height;
   if (im->in.w <= 1) {
      goto error;
   }
   if (im->in.h <= 1) {
      goto error;
   }

   im->out.w = im->in.w;
   im->out.h = im->in.h;

   im->color_space = (((im->in.color_space = im->in.jinfo.out_color_space) == JCS_GRAYSCALE) ? EPEG_GRAY8 : EPEG_RGB8);
   if (im->in.color_space == JCS_CMYK) {
      im->color_space = EPEG_CMYK;
   }

   for ((m = im->in.jinfo.marker_list); m; (m = m->next)) {
      if (m->marker == JPEG_COM) {
         if (im->in.comment) {
            free(im->in.comment);
         }
         im->in.comment = (char *)malloc((size_t)(m->data_length + 1));
         if (im->in.comment) {
            memcpy(im->in.comment, m->data, (size_t)m->data_length);
            im->in.comment[m->data_length] = 0;
         }
      } else if (m->marker == (JPEG_APP0 + 7))  {
         if ((m->data_length > 7) &&
             (!strncmp((char *)m->data, "Thumb::", (size_t)7))) {
            char *p, *p2;

            p = (char *)malloc((size_t)(m->data_length + 1));
            if (p) {
               memcpy(p, m->data, (size_t)m->data_length);
               p[m->data_length] = 0;
               p2 = strchr(p, '\n');
               if (p2) {
                  p2[0] = 0;
                  if (!strcmp(p, "Thumb::URI")) {
                     im->in.thumb_info.uri = strdup(p2 + 1);
                  } else if (!strcmp(p, "Thumb::MTime")) {
                     sscanf((p2 + 1), "%llu", &(im->in.thumb_info.mtime));
                  } else if (!strcmp(p, "Thumb::Image::Width")) {
                     im->in.thumb_info.w = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Image::Height")) {
                     im->in.thumb_info.h = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Mimetype")) {
                     im->in.thumb_info.mime = strdup(p2 + 1);
                  }
               } /* end "if (p2)" */
               free(p);
            } /* end "if (p)" */
         }
      }
   } /* end for-loop */
   return im;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode(Epeg_Image *im)
{
   int scale, scalew, scaleh, y;

   if (im->pixels) {
      return 1;
   }

   scalew = (im->in.w / im->out.w);
   scaleh = (im->in.h / im->out.h);

   scale = scalew;
   if (scaleh < scalew) {
      scale = scaleh;
   }

   if (scale > 8) {
      scale = 8;
   } else if (scale < 1) {
      scale = 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = (unsigned int)scale;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_IFAST;

   switch (im->color_space) {
      case EPEG_GRAY8:
			 im->in.jinfo.out_color_space = JCS_GRAYSCALE;
			 im->in.jinfo.output_components = 1;
			 break;

      case EPEG_YUV8:
			 im->in.jinfo.out_color_space = JCS_YCbCr;
			 break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
			 im->in.jinfo.out_color_space = JCS_RGB;
			 break;

      case EPEG_CMYK:
			 im->in.jinfo.out_color_space = JCS_CMYK;
			 im->in.jinfo.output_components = 4;
			 break;

      default:
			 break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      epeg_close(im);
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
	   jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_scale(Epeg_Image *im)
{
   unsigned char *dst, *row, *src;
   int            x, y, w, h, i;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   for ((y = 0); (y < h); y++) {
      row = (im->pixels +
             ((((unsigned int)y * im->in.jinfo.output_height) / (unsigned int)h) * (unsigned int)im->in.jinfo.output_components * im->in.jinfo.output_width));
      dst = (im->pixels +
             ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));

      for ((x = 0); (x < im->out.w); x++) {
         src = (row +
                ((((unsigned int)x * im->in.jinfo.output_width) / (unsigned int)w) * (unsigned int)im->in.jinfo.output_components));

         for ((i = 0); (i < im->in.jinfo.output_components); i++) {
            dst[i] = src[i];
         } /* end inmost for-loop */

         dst += im->in.jinfo.output_components;
      } /* end inner for-loop */
   } /* end outer for-loop */
   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode_for_trim(Epeg_Image *im)
{
   int y;

   if (im->pixels) {
      return 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = 1;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_ISLOW;

   switch (im->color_space) {
      case EPEG_GRAY8:
		   im->in.jinfo.out_color_space = JCS_GRAYSCALE;
		   im->in.jinfo.output_components = 1;
		   break;

      case EPEG_YUV8:
		   im->in.jinfo.out_color_space = JCS_YCbCr;
		   break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
		   im->in.jinfo.out_color_space = JCS_RGB;
		   break;

      case EPEG_CMYK:
		   im->in.jinfo.out_color_space = JCS_CMYK;
		   im->in.jinfo.output_components = 4;
		   break;

      default:
		   break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
      im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
      jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_trim(Epeg_Image *im)
{
   int y, a, b, w, h;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   a = im->out.x;
   b = im->out.y;

   /* dummy condition to use value stored to 'w': */
   if (w == 0) {
	   ;
   }

   for ((y = 0); (y < h); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)((y + b) * im->in.jinfo.output_components) * im->in.jinfo.output_width)
                      + (a * im->in.jinfo.output_components));
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_encode(Epeg_Image *im)
{
   void  *data = NULL;
   size_t size = 0;

   if (im->out.f) {
      return 1;
   }

   if (im->out.file) {
      im->out.f = fopen(im->out.file, "wb");
   } else {
      im->out.f = _epeg_memfile_write_open(&data, &size);
   }
   if (!im->out.f) {
      im->error = 1;
      return 1;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_create_compress(&(im->out.jinfo));
   jpeg_stdio_dest(&(im->out.jinfo), im->out.f);
   im->out.jinfo.image_width = (JDIMENSION)im->out.w;
   im->out.jinfo.image_height = (JDIMENSION)im->out.h;
   im->out.jinfo.input_components = im->in.jinfo.output_components;
   im->out.jinfo.in_color_space = im->in.jinfo.out_color_space;
   im->out.jinfo.dct_method = JDCT_IFAST;
   im->out.jinfo.dct_method = im->in.jinfo.dct_method;
   jpeg_set_defaults(&(im->out.jinfo));
   jpeg_set_quality(&(im->out.jinfo), im->out.quality, TRUE);

   if (im->out.quality >= 90) {
      im->out.jinfo.comp_info[0].h_samp_factor = 1;
      im->out.jinfo.comp_info[0].v_samp_factor = 1;
      im->out.jinfo.comp_info[1].h_samp_factor = 1;
      im->out.jinfo.comp_info[1].v_samp_factor = 1;
      im->out.jinfo.comp_info[2].h_samp_factor = 1;
      im->out.jinfo.comp_info[2].v_samp_factor = 1;
   }
   jpeg_start_compress(&(im->out.jinfo), TRUE);

   if (im->out.comment) {
      jpeg_write_marker(&(im->out.jinfo), JPEG_COM,
                        (const JOCTET *)im->out.comment,
                        (unsigned int)strlen(im->out.comment));
   }

   if (im->out.thumbnail_info) {
      char buf[8192];

      if (im->in.file) {
         snprintf(buf, sizeof(buf), "Thumb::URI\nfile://%s", im->in.file);
         jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7),
                           (const JOCTET *)buf, (unsigned int)strlen(buf));
         snprintf(buf, sizeof(buf), "Thumb::MTime\n%llu",
#if defined(HAVE_UINTMAX_T) && !defined(__LP64__)
                  (uintmax_t)im->stat_info.st_mtime);
#else
               (unsigned long long int)im->stat_info.st_mtime);
#endif /* HAVE_UINTMAX_T && !__LP64__ */
      }
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Width\n%i", im->in.w);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Height\n%i", im->in.h);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Mimetype\nimage/jpeg");
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
   }

   while (im->out.jinfo.next_scanline < im->out.h) {
      jpeg_write_scanlines(&(im->out.jinfo),
                           &(im->lines[im->out.jinfo.next_scanline]), 1);
   }

   jpeg_finish_compress(&(im->out.jinfo));

   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if ((im->in.f) && (im->in.file)) {
      fclose(im->in.f);
   }
   if ((im->in.f) && (!im->in.file)) {
      _epeg_memfile_read_close(im->in.f);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if ((im->out.f) && (im->out.file)) {
      fclose(im->out.f);
   }
   if ((im->out.f) && (!im->out.file)) {
      _epeg_memfile_write_close(im->out.f);
   }
   im->in.f = NULL;
   im->out.f = NULL;

   if (im->out.mem.data) {
	   *(im->out.mem.data) = (unsigned char *)data;
   }
   if (im->out.mem.size) {
	   *(im->out.mem.size) = (int)size;
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static void _epeg_fatal_error_handler(j_common_ptr cinfo)
{
   emptr errmgr;

   errmgr = (emptr)cinfo->err;
   longjmp(errmgr->setjmp_buffer, 1);
   return;
}

/* silence '-Wunused-macros' warnings: */
#ifdef MAX
# undef MAX
#endif /* MAX */

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_memfile.c': */
/* epeg_memfile.c */
/* gets built into the libepeg library */

#include <stdio.h>
#include "Epeg.h"
#include "epeg_private.h"

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_read_open(void *data, size_t size)
{
#ifdef HAVE_FMEMOPEN
   return (FILE *)fmemopen(data, size, "r");
#else
   FILE *f;

   f = tmpfile();
   if (!f) {
      return NULL;
   }
   fwrite(data, size, (size_t)1L, f);
   rewind(f);
   return f;
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_read_close(FILE *f)
{
#ifdef HAVE_FMEMOPEN
   fclose(f);
#else
   /* FIXME: make these different halves of the conditional actually different
    * from one another */
   fclose(f);
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only struct and typedef; unnecessary to document: */
typedef struct _Eet_Memfile_Write_Info Eet_Memfile_Write_Info;
struct _Eet_Memfile_Write_Info
{
   FILE *f;
   void **data;
   size_t *size;
};

/* static internal private-only global variables; unnecessary to document: */
static int _epeg_memfile_info_alloc_num = 0;
static int _epeg_memfile_info_num = 0;
static Eet_Memfile_Write_Info *_epeg_memfile_info = NULL;

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_write_open(void **data, size_t *size)
{
#ifdef HAVE_OPEN_MEMSTREAM
   return open_memstream((char **)data, size);
#else
   FILE *f;

   _epeg_memfile_info_num++;
   if (_epeg_memfile_info_num > _epeg_memfile_info_alloc_num) {
      Eet_Memfile_Write_Info *tmp;

      _epeg_memfile_info_alloc_num += 16;
      tmp = (Eet_Memfile_Write_Info *)realloc(_epeg_memfile_info,
                                              ((unsigned long)_epeg_memfile_info_alloc_num *
                                               sizeof(Eet_Memfile_Write_Info)));
      if (!tmp) {
         _epeg_memfile_info_alloc_num -= 16;
         _epeg_memfile_info_num--;
         return NULL;
      }
      _epeg_memfile_info = tmp;
   }
   f = tmpfile();
   if (!f) {
      _epeg_memfile_info_num--;
      return NULL;
   }
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].f = f;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].data = data;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].size = size;
   return f;
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_write_close(FILE *f)
{
#ifdef HAVE_OPEN_MEMSTREAM
   fclose(f);
#else
   int i;

   for ((i = 0); (i < _epeg_memfile_info_num); i++) {
      if (_epeg_memfile_info[i].f == f) {
         int j;

         fseek(f, 0L, SEEK_END);
         (*(_epeg_memfile_info[i].size)) = (size_t)ftell(f);
         rewind(f);
         (*(_epeg_memfile_info[i].data)) = malloc(*(_epeg_memfile_info[i].size));
         if (!(*(_epeg_memfile_info[i].data))) {
            fclose(f);
            (*(_epeg_memfile_info[i].size)) = 0;
            return;
         }
         fread((*(_epeg_memfile_info[i].data)), (*(_epeg_memfile_info[i].size)),
               (size_t)1L, f);
         for ((j = (i + 1)); (j < _epeg_memfile_info_num); j++) {
            _epeg_memfile_info[(j - 1)] = _epeg_memfile_info[j];
         }
         _epeg_memfile_info_num--;
         fclose(f);
         return;
      } /* end "if" */
   } /* end for-loop */
   fclose(f);
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_main.c': */
/* epeg_main.c for src/lib for epeg */
/* gets built into the libepeg library */

#include <stdio.h>
#include <stdlib.h>
#include "Epeg.h"
#include "epeg_private.h"

static Epeg_Image*_epeg_open_header(Epeg_Image *im);
static int _epeg_decode(Epeg_Image *im);
static int _epeg_scale(Epeg_Image *im);
static int _epeg_decode_for_trim(Epeg_Image *im);
static int _epeg_trim(Epeg_Image *im);
static int _epeg_encode(Epeg_Image *im);

static void _epeg_fatal_error_handler(j_common_ptr cinfo);

#ifndef MIN
# define MIN(__x,__y) ((__x) < (__y) ? (__x) : (__y))
#endif /* !MIN */
#ifndef MAX
# define MAX(__x,__y) ((__x) > (__y) ? (__x) : (__y))
#endif /* !MAX */

/**
 * Open a JPEG image by filename.
 * @param file The file path to open.
 * @return A handle to the opened JPEG file, with the header decoded.
 *
 * This function opens the file indicated by the @p file parameter, and
 * attempts to decode it as a jpeg file. If this failes, NULL is returned.
 * Otherwise a valid handle to an open JPEG file is returned that can be used
 * by other Epeg calls.
 *
 * The @p file must be a pointer to a valid C string, NUL (0 byte) terminated
 * thats is a relative or absolute file path. If not results are not
 * determined.
 *
 * See also: epeg_memory_open(), epeg_close()
 */
extern Epeg_Image *epeg_file_open(const char *file)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.file = strdup(file);
   im->in.f = fopen(im->in.file, "rb");
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   fstat(fileno(im->in.f), &(im->stat_info));
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Open a JPEG image stored in memory.
 * @param data A pointer to the memory containing the JPEG data.
 * @param size The size of the memory segment containing the JPEG.
 * @return  A handle to the opened JPEG, with the header decoded.
 *
 * This function opens a JPEG file that is stored in memory pointed to by
 * @p data, and that is @p size bytes in size. If successful a valid handle
 * is returned, or on failure NULL is returned.
 *
 * See also: epeg_file_open(), epeg_close()
 */
extern Epeg_Image *epeg_memory_open(unsigned char *data, int size)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.f = _epeg_memfile_read_open(data, (size_t)size);
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Return the original JPEG pixel size.
 * @param im A handle to an opened Epeg image.
 * @param w A pointer to the width value in pixels to be filled in.
 * @param h A pointer to the height value in pixels to be filled in.
 * @return Nothing.
 *
 * Returns the image size in pixels (well not really).
 *
 * See also: epeg_colorspace_get()
 */
extern void epeg_size_get(Epeg_Image *im, int *w, int *h)
{
	if (w) {
		*w = im->in.w;
	}
	if (h) {
		*h = im->in.h;
	}
}

/**
 * Return the original JPEG pixel color space.
 * @param im A handle to an opened Epeg image.
 * @param space A pointer to the color space value to be filled in.
 * @return Nothing.
 *
 * Returns the image color space (not yet though).
 *
 * See also: epeg_size_get()
 */
extern void epeg_colorspace_get(Epeg_Image *im, int *space)
{
	if (space) {
		*space = im->color_space;
	}
}

/**
 * Set the size of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param w The width of the image to decode at, in pixels.
 * @param h The height of the image to decode at, in pixels.
 * @return Nothing.
 *
 * Sets the size at which to decode the JPEG image, giving an optimized load
 * that only decodes the pixels needed.
 *
 * See also: epeg_decode_bounds_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_size_set(Epeg_Image *im, int w, int h)
{
   if (im->pixels) {
	   return;
   }
   if (w < 1) {
	   w = 1;
   } else if (w > im->in.w) {
	   w = im->in.w;
   }
   if (h < 1) {
	   h = 1;
   } else if (h > im->in.h) {
	   h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   im->out.x = 0;
   im->out.y = 0;
}

/**
 * Set the bounds of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param x Boundary X
 * @param y Boundary Y
 * @param w Boundary W
 * @param h Boundary H
 * @return Nothing.
 *
 * Sets the bounds inside which to decode the JPEG image,
 * giving an optimized load that only decodes the bounded pixels.
 * (???)
 *
 * See also: epeg_decode_size_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_bounds_set(Epeg_Image *im, int x, int y, int w, int h)
{
   if (im->pixels) {
      return;
   }
   if (w < 1) {
      w = 1;
   } else if (w > im->in.w) {
      w = im->in.w;
   }
   if (h < 1) {
      h = 1;
   } else if (h > im->in.h) {
      h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   if (x < 0) {
      x = 0;
   }
   if (y < 0) {
      y = 0;
   }
   im->out.x = x;
   im->out.y = y;
}

/**
 * Set the colorspace in which to decode the image.
 * @param im A handle to an opened Epeg image.
 * @param colorspace The colorspace to decode the image in.
 * @return Nothing.
 *
 * This sets the colorspace to decode the image in. The default is EPEG_YUV8,
 * as this is normally the native colorspace of a JPEG file, avoiding any
 * colorspace conversions for a faster load and/or save.
 *
 * See also: epeg_decode_size_set(), epeg_decode_bounds_set()
 */
extern void epeg_decode_colorspace_set(Epeg_Image *im,
                                       Epeg_Colorspace colorspace)
{
	if (im->pixels) {
		return;
	}
	if ((colorspace < EPEG_GRAY8) || (colorspace > EPEG_ARGB32)) {
		return;
	}
	im->color_space = colorspace;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get_as_RGB8()
 */
extern const void *epeg_pixels_get(Epeg_Image *im, int x, int y,  int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
         return NULL;
      }
   }

   if (!im->pixels) {
      return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* go through all 8 values in type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace): */
   if (im->color_space == EPEG_GRAY8) {
      /* this whole block is pretty much the same thing we do for each
       * of the other enumeration values as well... could probably be simplified
       * with a function-like macro or some other sort of meta-programming: */
      unsigned char *pix, *p;

      /* '1': */
      pix = (unsigned char *)malloc((size_t)(w * h * 1L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = s[0];
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_YUV8) {
      unsigned char *pix, *p;

      /* '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGR8) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* reverse order: */
            p[0] = s[2];
            p[1] = s[1];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGBA8) {
      unsigned char *pix, *p;

      /* '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGRA8) {
      unsigned char *pix, *p;

      /* also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = 0xff;
            /* reverse order: */
            p[1] = s[2];
            p[2] = s[1];
            p[3] = s[0];
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_ARGB32) {
      unsigned int *pix, *p;

      /* still also '4': */
      pix = (unsigned int *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (0xff000000 | (unsigned int)(s[0] << 16) | (unsigned int)(s[1] << 8) | (s[2]));
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* another thing that is also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* back to same order again: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } /* end "if-else" that seems like it should really be a "switch" */
   return NULL;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get()
 */
extern const void *epeg_pixels_get_as_RGB8(Epeg_Image *im, int x, int y,
                                           int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
		   return NULL;
	   }
   }

   if (!im->pixels) {
	   return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* unlike the "non-'_as_RGB8'-version" of this function,
    * there are no 'else'-es separating the cases here. Also, we only go through
    * three of the enumeration values of type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace) here: */
   if (im->color_space == EPEG_GRAY8) {
      unsigned char *pix, *p;

      /* get right into the '3' ones: */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[0];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (unsigned char)(MIN(255, (s[0] * s[3]) / 255));
            p[1] = (unsigned char)(MIN(255, (s[1] * s[3]) / 255));
            p[2] = (unsigned char)(MIN(255, (s[2] * s[3]) / 255));
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   return NULL;
}

/**
 * Free requested pixel block from an image.
 * @param im A handle to an opened Epeg image (unused).
 * @param data The pointer to the image pixels.
 * @return Nothing.
 *
 * This frees the data for a block of pixels requested from image @p im.
 * @p data must be a valid (non NULL) pointer to a pixel block taken from the
 * image @p im by epeg_pixels_get() and must be called before the image is
 * closed by epeg_close().
 */
extern void epeg_pixels_free(Epeg_Image *im, const void *data)
{
#pragma unused (im)
   free((void *)data);
}

/**
 * Get the image comment field as a string.
 * @param im A handle to an opened Epeg image.
 * @return A pointer to the loaded image comments.
 *
 * This function returns the comment field as a string (NUL byte terminated)
 * of the loaded image @p im, if there is a comment, or NULL if no comment is
 * saved with the image. Consider the string returned to be read-only.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern const char *epeg_comment_get(Epeg_Image *im)
{
   return im->in.comment;
}

/**
 * Get thumbnail comments of loaded image.
 * @param im A handle to an opened Epeg image.
 * @param info Pointer to a thumbnail info struct to be filled in.
 * @return Nothing.
 *
 * This function retrieves thumbnail comments written by Epeg to any saved
 * JPEG files. If no thumbnail comments were saved, the fields will be 0 in
 * the @p info struct on return.
 *
 * See also: epeg_comment_get(), epeg_thumbnail_comments_enable()
 */
extern void epeg_thumbnail_comments_get(Epeg_Image *im,
                                        Epeg_Thumbnail_Info *info)
{
   if (!info) {
      return;
   }
   info->uri = im->in.thumb_info.uri;
   info->mtime = im->in.thumb_info.mtime;
   info->w = im->in.thumb_info.w;
   info->h = im->in.thumb_info.h;
   info->mimetype = im->in.thumb_info.mime;
}

/**
 * Set the comment field of the image for saving.
 * @param im A handle to an opened Epeg image.
 * @param comment The comment to set.
 * @return Nothing.
 *
 * Set the comment for the image file for when it gets saved. This is a NUL
 * byte terminated C string. If @p comment is NULL the output file will have
 * no comment field.
 *
 * The default comment will be any comment loaded from the input file.
 *
 * See also: epeg_comment_get()
 */
extern void epeg_comment_set(Epeg_Image *im, const char *comment)
{
   if (im->out.comment) {
      free(im->out.comment);
   }
   if (!comment) {
      im->out.comment = NULL;
   } else {
      im->out.comment = strdup(comment);
   }
}

/**
 * Set the encoding quality of the saved image.
 * @param im A handle to an opened Epeg image.
 * @param quality The quality of encoding from 0 to 100.
 * @return Nothing.
 *
 * Set the quality of the output encoded image. Values from 0 to 100
 * inclusive are valid, with 100 being the maximum quality, and 0 being the
 * minimum. If the quality is set equal to or above 90%, the output U and V
 * color planes are encoded at 1:1 with the Y plane.
 *
 * The default quality is 75.
 *
 * See also: epeg_comment_set()
 */
extern void epeg_quality_set(Epeg_Image *im, int quality)
{
   if (quality < 0) {
      quality = 0;
   } else if (quality > 100) {
      quality = 100;
   }
   im->out.quality = quality;
}

/**
 * Enable thumbnail comments in saved image.
 * @param im A handle to an opened Epeg image.
 * @param onoff A boolean on and off enabling flag.
 * @return Nothing.
 *
 * if @p onoff is 1, the output file will have thumbnail comments added to
 * it, and if it is 0, it will not. The default is 0.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern void epeg_thumbnail_comments_enable(Epeg_Image *im, int onoff)
{
   im->out.thumbnail_info = (char)onoff;
}

/**
 * Set the output file path for the image when saved.
 * @param im A handle to an opened Epeg image.
 * @param file The path to the output file.
 * @return Nothing.
 *
 * This sets the output file path name (either a full or relative path name)
 * to where the file will be written when saved. @p file must be a NUL
 * terminated C string containing the path to the file to be saved to. If it is
 * NULL, then the image will not be saved to a file when calling epeg_encode().
 *
 * See also: epeg_memory_output_set(), epeg_encode()
 */
extern void epeg_file_output_set(Epeg_Image *im, const char *file)
{
   if (im->out.file) {
      free(im->out.file);
   }
   if (!file) {
      im->out.file = NULL;
   } else {
      im->out.file = strdup(file);
   }
}

/**
 * Set the output file to be a block of allocated memory.
 * @param im A handle to an opened Epeg image.
 * @param data A pointer to a pointer to a memory block.
 * @param size A pointer to a counter of the size of the memory block.
 * @return Nothing.
 *
 * This sets the output encoding of the image when saved to be allocated
 * memory. After epeg_close() is called the pointer pointed to by @p data
 * and the integer pointed to by @p size will contain the pointer to the
 * memory block and its size in bytes, respecitvely. The memory block can be
 * freed with the free() function call. If the save fails the pointer to the
 * memory block will be unaffected, as will the size.
 *
 * See also: epeg_file_output_set(), epeg_encode()
 */
extern void epeg_memory_output_set(Epeg_Image *im, unsigned char **data,
                                   int *size)
{
   im->out.mem.data = data;
   im->out.mem.size = size;
}

/**
 * This saves the image to its specified destination.
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * This saves the image @p im to its destination specified by
 * epeg_file_output_set() or epeg_memory_output_set(). The image will be
 * encoded at the decoded pixel size, using the quality, comment,
 * and thumbnail comment settings set on the image.
 *
 * See also: epeg_file_output_set(), epeg_memory_output_set()
 */
extern int epeg_encode(Epeg_Image *im)
{
   if (_epeg_decode(im) != 0) {
      return 1;
   }
   if (_epeg_scale(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * FIXME: Document this with a short, sentence-long description of epeg_trim()
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * FIXME: Document this with a longer, paragraph-long description.
 */
extern int epeg_trim(Epeg_Image *im)
{
   if (_epeg_decode_for_trim(im) != 0) {
      return 1;
   }
   if (_epeg_trim(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * Close an image handle.
 * @param im A handle to an opened Epeg image.
 * @return Nothing.
 *
 * This closes an opened image handle and frees all memory associated with it.
 * It does NOT free encoded data generated by epeg_memory_output_set() followed
 * by epeg_encode(), nor does it guarantee to free any data received by
 * epeg_pixels_get(). Once an image handle is closed, consider it invalid.
 *
 * See also: epeg_file_open(), epeg_memory_open()
 */
extern void epeg_close(Epeg_Image *im)
{
   if (im->pixels) {
      free(im->pixels);
   }
   if (im->lines) {
      free(im->lines);
   }
   if (im->in.file) {
      free(im->in.file);
   }
   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if (im->in.f) {
      fclose(im->in.f);
   }
   if (im->in.comment) {
      free(im->in.comment);
   }
   if (im->in.thumb_info.uri) {
      free(im->in.thumb_info.uri);
   }
   if (im->in.thumb_info.mime) {
      free(im->in.thumb_info.mime);
   }
   if (im->out.file) {
      free(im->out.file);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if (im->out.f) {
      fclose(im->out.f);
   }
   if (im->out.comment) {
      free(im->out.comment);
   }
   free(im);
}

/* static internal private-only function; unnecessary to document: */
static Epeg_Image *_epeg_open_header(Epeg_Image *im)
{
   struct jpeg_marker_struct *m;

   im->in.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      error:
      epeg_close(im);
      im = NULL;
      return NULL;
   }

   jpeg_create_decompress(&(im->in.jinfo));
   jpeg_save_markers(&(im->in.jinfo), (JPEG_APP0 + 7), 1024);
   jpeg_save_markers(&(im->in.jinfo), JPEG_COM, 65535);
   jpeg_stdio_src(&(im->in.jinfo), im->in.f);
   jpeg_read_header(&(im->in.jinfo), TRUE);
   im->in.w = (int)im->in.jinfo.image_width;
   im->in.h = (int)im->in.jinfo.image_height;
   if (im->in.w <= 1) {
      goto error;
   }
   if (im->in.h <= 1) {
      goto error;
   }

   im->out.w = im->in.w;
   im->out.h = im->in.h;

   im->color_space = (((im->in.color_space = im->in.jinfo.out_color_space) == JCS_GRAYSCALE) ? EPEG_GRAY8 : EPEG_RGB8);
   if (im->in.color_space == JCS_CMYK) {
      im->color_space = EPEG_CMYK;
   }

   for ((m = im->in.jinfo.marker_list); m; (m = m->next)) {
      if (m->marker == JPEG_COM) {
         if (im->in.comment) {
            free(im->in.comment);
         }
         im->in.comment = (char *)malloc((size_t)(m->data_length + 1));
         if (im->in.comment) {
            memcpy(im->in.comment, m->data, (size_t)m->data_length);
            im->in.comment[m->data_length] = 0;
         }
      } else if (m->marker == (JPEG_APP0 + 7))  {
         if ((m->data_length > 7) &&
             (!strncmp((char *)m->data, "Thumb::", (size_t)7))) {
            char *p, *p2;

            p = (char *)malloc((size_t)(m->data_length + 1));
            if (p) {
               memcpy(p, m->data, (size_t)m->data_length);
               p[m->data_length] = 0;
               p2 = strchr(p, '\n');
               if (p2) {
                  p2[0] = 0;
                  if (!strcmp(p, "Thumb::URI")) {
                     im->in.thumb_info.uri = strdup(p2 + 1);
                  } else if (!strcmp(p, "Thumb::MTime")) {
                     sscanf((p2 + 1), "%llu", &(im->in.thumb_info.mtime));
                  } else if (!strcmp(p, "Thumb::Image::Width")) {
                     im->in.thumb_info.w = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Image::Height")) {
                     im->in.thumb_info.h = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Mimetype")) {
                     im->in.thumb_info.mime = strdup(p2 + 1);
                  }
               } /* end "if (p2)" */
               free(p);
            } /* end "if (p)" */
         }
      }
   } /* end for-loop */
   return im;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode(Epeg_Image *im)
{
   int scale, scalew, scaleh, y;

   if (im->pixels) {
      return 1;
   }

   scalew = (im->in.w / im->out.w);
   scaleh = (im->in.h / im->out.h);

   scale = scalew;
   if (scaleh < scalew) {
      scale = scaleh;
   }

   if (scale > 8) {
      scale = 8;
   } else if (scale < 1) {
      scale = 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = (unsigned int)scale;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_IFAST;

   switch (im->color_space) {
      case EPEG_GRAY8:
			 im->in.jinfo.out_color_space = JCS_GRAYSCALE;
			 im->in.jinfo.output_components = 1;
			 break;

      case EPEG_YUV8:
			 im->in.jinfo.out_color_space = JCS_YCbCr;
			 break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
			 im->in.jinfo.out_color_space = JCS_RGB;
			 break;

      case EPEG_CMYK:
			 im->in.jinfo.out_color_space = JCS_CMYK;
			 im->in.jinfo.output_components = 4;
			 break;

      default:
			 break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      epeg_close(im);
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
	   jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_scale(Epeg_Image *im)
{
   unsigned char *dst, *row, *src;
   int            x, y, w, h, i;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   for ((y = 0); (y < h); y++) {
      row = (im->pixels +
             ((((unsigned int)y * im->in.jinfo.output_height) / (unsigned int)h) * (unsigned int)im->in.jinfo.output_components * im->in.jinfo.output_width));
      dst = (im->pixels +
             ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));

      for ((x = 0); (x < im->out.w); x++) {
         src = (row +
                ((((unsigned int)x * im->in.jinfo.output_width) / (unsigned int)w) * (unsigned int)im->in.jinfo.output_components));

         for ((i = 0); (i < im->in.jinfo.output_components); i++) {
            dst[i] = src[i];
         } /* end inmost for-loop */

         dst += im->in.jinfo.output_components;
      } /* end inner for-loop */
   } /* end outer for-loop */
   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode_for_trim(Epeg_Image *im)
{
   int y;

   if (im->pixels) {
      return 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = 1;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_ISLOW;

   switch (im->color_space) {
      case EPEG_GRAY8:
		   im->in.jinfo.out_color_space = JCS_GRAYSCALE;
		   im->in.jinfo.output_components = 1;
		   break;

      case EPEG_YUV8:
		   im->in.jinfo.out_color_space = JCS_YCbCr;
		   break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
		   im->in.jinfo.out_color_space = JCS_RGB;
		   break;

      case EPEG_CMYK:
		   im->in.jinfo.out_color_space = JCS_CMYK;
		   im->in.jinfo.output_components = 4;
		   break;

      default:
		   break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
      im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
      jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_trim(Epeg_Image *im)
{
   int y, a, b, w, h;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   a = im->out.x;
   b = im->out.y;

   /* dummy condition to use value stored to 'w': */
   if (w == 0) {
	   ;
   }

   for ((y = 0); (y < h); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)((y + b) * im->in.jinfo.output_components) * im->in.jinfo.output_width)
                      + (a * im->in.jinfo.output_components));
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_encode(Epeg_Image *im)
{
   void  *data = NULL;
   size_t size = 0;

   if (im->out.f) {
      return 1;
   }

   if (im->out.file) {
      im->out.f = fopen(im->out.file, "wb");
   } else {
      im->out.f = _epeg_memfile_write_open(&data, &size);
   }
   if (!im->out.f) {
      im->error = 1;
      return 1;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_create_compress(&(im->out.jinfo));
   jpeg_stdio_dest(&(im->out.jinfo), im->out.f);
   im->out.jinfo.image_width = (JDIMENSION)im->out.w;
   im->out.jinfo.image_height = (JDIMENSION)im->out.h;
   im->out.jinfo.input_components = im->in.jinfo.output_components;
   im->out.jinfo.in_color_space = im->in.jinfo.out_color_space;
   im->out.jinfo.dct_method = JDCT_IFAST;
   im->out.jinfo.dct_method = im->in.jinfo.dct_method;
   jpeg_set_defaults(&(im->out.jinfo));
   jpeg_set_quality(&(im->out.jinfo), im->out.quality, TRUE);

   if (im->out.quality >= 90) {
      im->out.jinfo.comp_info[0].h_samp_factor = 1;
      im->out.jinfo.comp_info[0].v_samp_factor = 1;
      im->out.jinfo.comp_info[1].h_samp_factor = 1;
      im->out.jinfo.comp_info[1].v_samp_factor = 1;
      im->out.jinfo.comp_info[2].h_samp_factor = 1;
      im->out.jinfo.comp_info[2].v_samp_factor = 1;
   }
   jpeg_start_compress(&(im->out.jinfo), TRUE);

   if (im->out.comment) {
      jpeg_write_marker(&(im->out.jinfo), JPEG_COM,
                        (const JOCTET *)im->out.comment,
                        (unsigned int)strlen(im->out.comment));
   }

   if (im->out.thumbnail_info) {
      char buf[8192];

      if (im->in.file) {
         snprintf(buf, sizeof(buf), "Thumb::URI\nfile://%s", im->in.file);
         jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7),
                           (const JOCTET *)buf, (unsigned int)strlen(buf));
         snprintf(buf, sizeof(buf), "Thumb::MTime\n%llu",
#if defined(HAVE_UINTMAX_T) && !defined(__LP64__)
                  (uintmax_t)im->stat_info.st_mtime);
#else
               (unsigned long long int)im->stat_info.st_mtime);
#endif /* HAVE_UINTMAX_T && !__LP64__ */
      }
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Width\n%i", im->in.w);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Height\n%i", im->in.h);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Mimetype\nimage/jpeg");
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
   }

   while (im->out.jinfo.next_scanline < im->out.h) {
      jpeg_write_scanlines(&(im->out.jinfo),
                           &(im->lines[im->out.jinfo.next_scanline]), 1);
   }

   jpeg_finish_compress(&(im->out.jinfo));

   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if ((im->in.f) && (im->in.file)) {
      fclose(im->in.f);
   }
   if ((im->in.f) && (!im->in.file)) {
      _epeg_memfile_read_close(im->in.f);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if ((im->out.f) && (im->out.file)) {
      fclose(im->out.f);
   }
   if ((im->out.f) && (!im->out.file)) {
      _epeg_memfile_write_close(im->out.f);
   }
   im->in.f = NULL;
   im->out.f = NULL;

   if (im->out.mem.data) {
	   *(im->out.mem.data) = (unsigned char *)data;
   }
   if (im->out.mem.size) {
	   *(im->out.mem.size) = (int)size;
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static void _epeg_fatal_error_handler(j_common_ptr cinfo)
{
   emptr errmgr;

   errmgr = (emptr)cinfo->err;
   longjmp(errmgr->setjmp_buffer, 1);
   return;
}

/* silence '-Wunused-macros' warnings: */
#ifdef MAX
# undef MAX
#endif /* MAX */

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_memfile.c': */
/* epeg_memfile.c */
/* gets built into the libepeg library */

#include <stdio.h>
#include "Epeg.h"
#include "epeg_private.h"

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_read_open(void *data, size_t size)
{
#ifdef HAVE_FMEMOPEN
   return (FILE *)fmemopen(data, size, "r");
#else
   FILE *f;

   f = tmpfile();
   if (!f) {
      return NULL;
   }
   fwrite(data, size, (size_t)1L, f);
   rewind(f);
   return f;
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_read_close(FILE *f)
{
#ifdef HAVE_FMEMOPEN
   fclose(f);
#else
   /* FIXME: make these different halves of the conditional actually different
    * from one another */
   fclose(f);
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only struct and typedef; unnecessary to document: */
typedef struct _Eet_Memfile_Write_Info Eet_Memfile_Write_Info;
struct _Eet_Memfile_Write_Info
{
   FILE *f;
   void **data;
   size_t *size;
};

/* static internal private-only global variables; unnecessary to document: */
static int _epeg_memfile_info_alloc_num = 0;
static int _epeg_memfile_info_num = 0;
static Eet_Memfile_Write_Info *_epeg_memfile_info = NULL;

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_write_open(void **data, size_t *size)
{
#ifdef HAVE_OPEN_MEMSTREAM
   return open_memstream((char **)data, size);
#else
   FILE *f;

   _epeg_memfile_info_num++;
   if (_epeg_memfile_info_num > _epeg_memfile_info_alloc_num) {
      Eet_Memfile_Write_Info *tmp;

      _epeg_memfile_info_alloc_num += 16;
      tmp = (Eet_Memfile_Write_Info *)realloc(_epeg_memfile_info,
                                              ((unsigned long)_epeg_memfile_info_alloc_num *
                                               sizeof(Eet_Memfile_Write_Info)));
      if (!tmp) {
         _epeg_memfile_info_alloc_num -= 16;
         _epeg_memfile_info_num--;
         return NULL;
      }
      _epeg_memfile_info = tmp;
   }
   f = tmpfile();
   if (!f) {
      _epeg_memfile_info_num--;
      return NULL;
   }
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].f = f;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].data = data;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].size = size;
   return f;
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_write_close(FILE *f)
{
#ifdef HAVE_OPEN_MEMSTREAM
   fclose(f);
#else
   int i;

   for ((i = 0); (i < _epeg_memfile_info_num); i++) {
      if (_epeg_memfile_info[i].f == f) {
         int j;

         fseek(f, 0L, SEEK_END);
         (*(_epeg_memfile_info[i].size)) = (size_t)ftell(f);
         rewind(f);
         (*(_epeg_memfile_info[i].data)) = malloc(*(_epeg_memfile_info[i].size));
         if (!(*(_epeg_memfile_info[i].data))) {
            fclose(f);
            (*(_epeg_memfile_info[i].size)) = 0;
            return;
         }
         fread((*(_epeg_memfile_info[i].data)), (*(_epeg_memfile_info[i].size)),
               (size_t)1L, f);
         for ((j = (i + 1)); (j < _epeg_memfile_info_num); j++) {
            _epeg_memfile_info[(j - 1)] = _epeg_memfile_info[j];
         }
         _epeg_memfile_info_num--;
         fclose(f);
         return;
      } /* end "if" */
   } /* end for-loop */
   fclose(f);
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_main.c': */
/* epeg_main.c for src/lib for epeg */
/* gets built into the libepeg library */

#include <stdio.h>
#include <stdlib.h>
#include "Epeg.h"
#include "epeg_private.h"

static Epeg_Image*_epeg_open_header(Epeg_Image *im);
static int _epeg_decode(Epeg_Image *im);
static int _epeg_scale(Epeg_Image *im);
static int _epeg_decode_for_trim(Epeg_Image *im);
static int _epeg_trim(Epeg_Image *im);
static int _epeg_encode(Epeg_Image *im);

static void _epeg_fatal_error_handler(j_common_ptr cinfo);

#ifndef MIN
# define MIN(__x,__y) ((__x) < (__y) ? (__x) : (__y))
#endif /* !MIN */
#ifndef MAX
# define MAX(__x,__y) ((__x) > (__y) ? (__x) : (__y))
#endif /* !MAX */

/**
 * Open a JPEG image by filename.
 * @param file The file path to open.
 * @return A handle to the opened JPEG file, with the header decoded.
 *
 * This function opens the file indicated by the @p file parameter, and
 * attempts to decode it as a jpeg file. If this failes, NULL is returned.
 * Otherwise a valid handle to an open JPEG file is returned that can be used
 * by other Epeg calls.
 *
 * The @p file must be a pointer to a valid C string, NUL (0 byte) terminated
 * thats is a relative or absolute file path. If not results are not
 * determined.
 *
 * See also: epeg_memory_open(), epeg_close()
 */
extern Epeg_Image *epeg_file_open(const char *file)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.file = strdup(file);
   im->in.f = fopen(im->in.file, "rb");
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   fstat(fileno(im->in.f), &(im->stat_info));
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Open a JPEG image stored in memory.
 * @param data A pointer to the memory containing the JPEG data.
 * @param size The size of the memory segment containing the JPEG.
 * @return  A handle to the opened JPEG, with the header decoded.
 *
 * This function opens a JPEG file that is stored in memory pointed to by
 * @p data, and that is @p size bytes in size. If successful a valid handle
 * is returned, or on failure NULL is returned.
 *
 * See also: epeg_file_open(), epeg_close()
 */
extern Epeg_Image *epeg_memory_open(unsigned char *data, int size)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.f = _epeg_memfile_read_open(data, (size_t)size);
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Return the original JPEG pixel size.
 * @param im A handle to an opened Epeg image.
 * @param w A pointer to the width value in pixels to be filled in.
 * @param h A pointer to the height value in pixels to be filled in.
 * @return Nothing.
 *
 * Returns the image size in pixels (well not really).
 *
 * See also: epeg_colorspace_get()
 */
extern void epeg_size_get(Epeg_Image *im, int *w, int *h)
{
	if (w) {
		*w = im->in.w;
	}
	if (h) {
		*h = im->in.h;
	}
}

/**
 * Return the original JPEG pixel color space.
 * @param im A handle to an opened Epeg image.
 * @param space A pointer to the color space value to be filled in.
 * @return Nothing.
 *
 * Returns the image color space (not yet though).
 *
 * See also: epeg_size_get()
 */
extern void epeg_colorspace_get(Epeg_Image *im, int *space)
{
	if (space) {
		*space = im->color_space;
	}
}

/**
 * Set the size of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param w The width of the image to decode at, in pixels.
 * @param h The height of the image to decode at, in pixels.
 * @return Nothing.
 *
 * Sets the size at which to decode the JPEG image, giving an optimized load
 * that only decodes the pixels needed.
 *
 * See also: epeg_decode_bounds_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_size_set(Epeg_Image *im, int w, int h)
{
   if (im->pixels) {
	   return;
   }
   if (w < 1) {
	   w = 1;
   } else if (w > im->in.w) {
	   w = im->in.w;
   }
   if (h < 1) {
	   h = 1;
   } else if (h > im->in.h) {
	   h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   im->out.x = 0;
   im->out.y = 0;
}

/**
 * Set the bounds of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param x Boundary X
 * @param y Boundary Y
 * @param w Boundary W
 * @param h Boundary H
 * @return Nothing.
 *
 * Sets the bounds inside which to decode the JPEG image,
 * giving an optimized load that only decodes the bounded pixels.
 * (???)
 *
 * See also: epeg_decode_size_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_bounds_set(Epeg_Image *im, int x, int y, int w, int h)
{
   if (im->pixels) {
      return;
   }
   if (w < 1) {
      w = 1;
   } else if (w > im->in.w) {
      w = im->in.w;
   }
   if (h < 1) {
      h = 1;
   } else if (h > im->in.h) {
      h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   if (x < 0) {
      x = 0;
   }
   if (y < 0) {
      y = 0;
   }
   im->out.x = x;
   im->out.y = y;
}

/**
 * Set the colorspace in which to decode the image.
 * @param im A handle to an opened Epeg image.
 * @param colorspace The colorspace to decode the image in.
 * @return Nothing.
 *
 * This sets the colorspace to decode the image in. The default is EPEG_YUV8,
 * as this is normally the native colorspace of a JPEG file, avoiding any
 * colorspace conversions for a faster load and/or save.
 *
 * See also: epeg_decode_size_set(), epeg_decode_bounds_set()
 */
extern void epeg_decode_colorspace_set(Epeg_Image *im,
                                       Epeg_Colorspace colorspace)
{
	if (im->pixels) {
		return;
	}
	if ((colorspace < EPEG_GRAY8) || (colorspace > EPEG_ARGB32)) {
		return;
	}
	im->color_space = colorspace;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get_as_RGB8()
 */
extern const void *epeg_pixels_get(Epeg_Image *im, int x, int y,  int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
         return NULL;
      }
   }

   if (!im->pixels) {
      return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* go through all 8 values in type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace): */
   if (im->color_space == EPEG_GRAY8) {
      /* this whole block is pretty much the same thing we do for each
       * of the other enumeration values as well... could probably be simplified
       * with a function-like macro or some other sort of meta-programming: */
      unsigned char *pix, *p;

      /* '1': */
      pix = (unsigned char *)malloc((size_t)(w * h * 1L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = s[0];
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_YUV8) {
      unsigned char *pix, *p;

      /* '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGR8) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* reverse order: */
            p[0] = s[2];
            p[1] = s[1];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGBA8) {
      unsigned char *pix, *p;

      /* '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGRA8) {
      unsigned char *pix, *p;

      /* also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = 0xff;
            /* reverse order: */
            p[1] = s[2];
            p[2] = s[1];
            p[3] = s[0];
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_ARGB32) {
      unsigned int *pix, *p;

      /* still also '4': */
      pix = (unsigned int *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (0xff000000 | (unsigned int)(s[0] << 16) | (unsigned int)(s[1] << 8) | (s[2]));
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* another thing that is also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* back to same order again: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } /* end "if-else" that seems like it should really be a "switch" */
   return NULL;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get()
 */
extern const void *epeg_pixels_get_as_RGB8(Epeg_Image *im, int x, int y,
                                           int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
		   return NULL;
	   }
   }

   if (!im->pixels) {
	   return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* unlike the "non-'_as_RGB8'-version" of this function,
    * there are no 'else'-es separating the cases here. Also, we only go through
    * three of the enumeration values of type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace) here: */
   if (im->color_space == EPEG_GRAY8) {
      unsigned char *pix, *p;

      /* get right into the '3' ones: */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[0];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (unsigned char)(MIN(255, (s[0] * s[3]) / 255));
            p[1] = (unsigned char)(MIN(255, (s[1] * s[3]) / 255));
            p[2] = (unsigned char)(MIN(255, (s[2] * s[3]) / 255));
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   return NULL;
}

/**
 * Free requested pixel block from an image.
 * @param im A handle to an opened Epeg image (unused).
 * @param data The pointer to the image pixels.
 * @return Nothing.
 *
 * This frees the data for a block of pixels requested from image @p im.
 * @p data must be a valid (non NULL) pointer to a pixel block taken from the
 * image @p im by epeg_pixels_get() and must be called before the image is
 * closed by epeg_close().
 */
extern void epeg_pixels_free(Epeg_Image *im, const void *data)
{
#pragma unused (im)
   free((void *)data);
}

/**
 * Get the image comment field as a string.
 * @param im A handle to an opened Epeg image.
 * @return A pointer to the loaded image comments.
 *
 * This function returns the comment field as a string (NUL byte terminated)
 * of the loaded image @p im, if there is a comment, or NULL if no comment is
 * saved with the image. Consider the string returned to be read-only.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern const char *epeg_comment_get(Epeg_Image *im)
{
   return im->in.comment;
}

/**
 * Get thumbnail comments of loaded image.
 * @param im A handle to an opened Epeg image.
 * @param info Pointer to a thumbnail info struct to be filled in.
 * @return Nothing.
 *
 * This function retrieves thumbnail comments written by Epeg to any saved
 * JPEG files. If no thumbnail comments were saved, the fields will be 0 in
 * the @p info struct on return.
 *
 * See also: epeg_comment_get(), epeg_thumbnail_comments_enable()
 */
extern void epeg_thumbnail_comments_get(Epeg_Image *im,
                                        Epeg_Thumbnail_Info *info)
{
   if (!info) {
      return;
   }
   info->uri = im->in.thumb_info.uri;
   info->mtime = im->in.thumb_info.mtime;
   info->w = im->in.thumb_info.w;
   info->h = im->in.thumb_info.h;
   info->mimetype = im->in.thumb_info.mime;
}

/**
 * Set the comment field of the image for saving.
 * @param im A handle to an opened Epeg image.
 * @param comment The comment to set.
 * @return Nothing.
 *
 * Set the comment for the image file for when it gets saved. This is a NUL
 * byte terminated C string. If @p comment is NULL the output file will have
 * no comment field.
 *
 * The default comment will be any comment loaded from the input file.
 *
 * See also: epeg_comment_get()
 */
extern void epeg_comment_set(Epeg_Image *im, const char *comment)
{
   if (im->out.comment) {
      free(im->out.comment);
   }
   if (!comment) {
      im->out.comment = NULL;
   } else {
      im->out.comment = strdup(comment);
   }
}

/**
 * Set the encoding quality of the saved image.
 * @param im A handle to an opened Epeg image.
 * @param quality The quality of encoding from 0 to 100.
 * @return Nothing.
 *
 * Set the quality of the output encoded image. Values from 0 to 100
 * inclusive are valid, with 100 being the maximum quality, and 0 being the
 * minimum. If the quality is set equal to or above 90%, the output U and V
 * color planes are encoded at 1:1 with the Y plane.
 *
 * The default quality is 75.
 *
 * See also: epeg_comment_set()
 */
extern void epeg_quality_set(Epeg_Image *im, int quality)
{
   if (quality < 0) {
      quality = 0;
   } else if (quality > 100) {
      quality = 100;
   }
   im->out.quality = quality;
}

/**
 * Enable thumbnail comments in saved image.
 * @param im A handle to an opened Epeg image.
 * @param onoff A boolean on and off enabling flag.
 * @return Nothing.
 *
 * if @p onoff is 1, the output file will have thumbnail comments added to
 * it, and if it is 0, it will not. The default is 0.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern void epeg_thumbnail_comments_enable(Epeg_Image *im, int onoff)
{
   im->out.thumbnail_info = (char)onoff;
}

/**
 * Set the output file path for the image when saved.
 * @param im A handle to an opened Epeg image.
 * @param file The path to the output file.
 * @return Nothing.
 *
 * This sets the output file path name (either a full or relative path name)
 * to where the file will be written when saved. @p file must be a NUL
 * terminated C string containing the path to the file to be saved to. If it is
 * NULL, then the image will not be saved to a file when calling epeg_encode().
 *
 * See also: epeg_memory_output_set(), epeg_encode()
 */
extern void epeg_file_output_set(Epeg_Image *im, const char *file)
{
   if (im->out.file) {
      free(im->out.file);
   }
   if (!file) {
      im->out.file = NULL;
   } else {
      im->out.file = strdup(file);
   }
}

/**
 * Set the output file to be a block of allocated memory.
 * @param im A handle to an opened Epeg image.
 * @param data A pointer to a pointer to a memory block.
 * @param size A pointer to a counter of the size of the memory block.
 * @return Nothing.
 *
 * This sets the output encoding of the image when saved to be allocated
 * memory. After epeg_close() is called the pointer pointed to by @p data
 * and the integer pointed to by @p size will contain the pointer to the
 * memory block and its size in bytes, respecitvely. The memory block can be
 * freed with the free() function call. If the save fails the pointer to the
 * memory block will be unaffected, as will the size.
 *
 * See also: epeg_file_output_set(), epeg_encode()
 */
extern void epeg_memory_output_set(Epeg_Image *im, unsigned char **data,
                                   int *size)
{
   im->out.mem.data = data;
   im->out.mem.size = size;
}

/**
 * This saves the image to its specified destination.
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * This saves the image @p im to its destination specified by
 * epeg_file_output_set() or epeg_memory_output_set(). The image will be
 * encoded at the decoded pixel size, using the quality, comment,
 * and thumbnail comment settings set on the image.
 *
 * See also: epeg_file_output_set(), epeg_memory_output_set()
 */
extern int epeg_encode(Epeg_Image *im)
{
   if (_epeg_decode(im) != 0) {
      return 1;
   }
   if (_epeg_scale(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * FIXME: Document this with a short, sentence-long description of epeg_trim()
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * FIXME: Document this with a longer, paragraph-long description.
 */
extern int epeg_trim(Epeg_Image *im)
{
   if (_epeg_decode_for_trim(im) != 0) {
      return 1;
   }
   if (_epeg_trim(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * Close an image handle.
 * @param im A handle to an opened Epeg image.
 * @return Nothing.
 *
 * This closes an opened image handle and frees all memory associated with it.
 * It does NOT free encoded data generated by epeg_memory_output_set() followed
 * by epeg_encode(), nor does it guarantee to free any data received by
 * epeg_pixels_get(). Once an image handle is closed, consider it invalid.
 *
 * See also: epeg_file_open(), epeg_memory_open()
 */
extern void epeg_close(Epeg_Image *im)
{
   if (im->pixels) {
      free(im->pixels);
   }
   if (im->lines) {
      free(im->lines);
   }
   if (im->in.file) {
      free(im->in.file);
   }
   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if (im->in.f) {
      fclose(im->in.f);
   }
   if (im->in.comment) {
      free(im->in.comment);
   }
   if (im->in.thumb_info.uri) {
      free(im->in.thumb_info.uri);
   }
   if (im->in.thumb_info.mime) {
      free(im->in.thumb_info.mime);
   }
   if (im->out.file) {
      free(im->out.file);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if (im->out.f) {
      fclose(im->out.f);
   }
   if (im->out.comment) {
      free(im->out.comment);
   }
   free(im);
}

/* static internal private-only function; unnecessary to document: */
static Epeg_Image *_epeg_open_header(Epeg_Image *im)
{
   struct jpeg_marker_struct *m;

   im->in.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      error:
      epeg_close(im);
      im = NULL;
      return NULL;
   }

   jpeg_create_decompress(&(im->in.jinfo));
   jpeg_save_markers(&(im->in.jinfo), (JPEG_APP0 + 7), 1024);
   jpeg_save_markers(&(im->in.jinfo), JPEG_COM, 65535);
   jpeg_stdio_src(&(im->in.jinfo), im->in.f);
   jpeg_read_header(&(im->in.jinfo), TRUE);
   im->in.w = (int)im->in.jinfo.image_width;
   im->in.h = (int)im->in.jinfo.image_height;
   if (im->in.w <= 1) {
      goto error;
   }
   if (im->in.h <= 1) {
      goto error;
   }

   im->out.w = im->in.w;
   im->out.h = im->in.h;

   im->color_space = (((im->in.color_space = im->in.jinfo.out_color_space) == JCS_GRAYSCALE) ? EPEG_GRAY8 : EPEG_RGB8);
   if (im->in.color_space == JCS_CMYK) {
      im->color_space = EPEG_CMYK;
   }

   for ((m = im->in.jinfo.marker_list); m; (m = m->next)) {
      if (m->marker == JPEG_COM) {
         if (im->in.comment) {
            free(im->in.comment);
         }
         im->in.comment = (char *)malloc((size_t)(m->data_length + 1));
         if (im->in.comment) {
            memcpy(im->in.comment, m->data, (size_t)m->data_length);
            im->in.comment[m->data_length] = 0;
         }
      } else if (m->marker == (JPEG_APP0 + 7))  {
         if ((m->data_length > 7) &&
             (!strncmp((char *)m->data, "Thumb::", (size_t)7))) {
            char *p, *p2;

            p = (char *)malloc((size_t)(m->data_length + 1));
            if (p) {
               memcpy(p, m->data, (size_t)m->data_length);
               p[m->data_length] = 0;
               p2 = strchr(p, '\n');
               if (p2) {
                  p2[0] = 0;
                  if (!strcmp(p, "Thumb::URI")) {
                     im->in.thumb_info.uri = strdup(p2 + 1);
                  } else if (!strcmp(p, "Thumb::MTime")) {
                     sscanf((p2 + 1), "%llu", &(im->in.thumb_info.mtime));
                  } else if (!strcmp(p, "Thumb::Image::Width")) {
                     im->in.thumb_info.w = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Image::Height")) {
                     im->in.thumb_info.h = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Mimetype")) {
                     im->in.thumb_info.mime = strdup(p2 + 1);
                  }
               } /* end "if (p2)" */
               free(p);
            } /* end "if (p)" */
         }
      }
   } /* end for-loop */
   return im;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode(Epeg_Image *im)
{
   int scale, scalew, scaleh, y;

   if (im->pixels) {
      return 1;
   }

   scalew = (im->in.w / im->out.w);
   scaleh = (im->in.h / im->out.h);

   scale = scalew;
   if (scaleh < scalew) {
      scale = scaleh;
   }

   if (scale > 8) {
      scale = 8;
   } else if (scale < 1) {
      scale = 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = (unsigned int)scale;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_IFAST;

   switch (im->color_space) {
      case EPEG_GRAY8:
			 im->in.jinfo.out_color_space = JCS_GRAYSCALE;
			 im->in.jinfo.output_components = 1;
			 break;

      case EPEG_YUV8:
			 im->in.jinfo.out_color_space = JCS_YCbCr;
			 break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
			 im->in.jinfo.out_color_space = JCS_RGB;
			 break;

      case EPEG_CMYK:
			 im->in.jinfo.out_color_space = JCS_CMYK;
			 im->in.jinfo.output_components = 4;
			 break;

      default:
			 break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      epeg_close(im);
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
	   jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_scale(Epeg_Image *im)
{
   unsigned char *dst, *row, *src;
   int            x, y, w, h, i;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   for ((y = 0); (y < h); y++) {
      row = (im->pixels +
             ((((unsigned int)y * im->in.jinfo.output_height) / (unsigned int)h) * (unsigned int)im->in.jinfo.output_components * im->in.jinfo.output_width));
      dst = (im->pixels +
             ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));

      for ((x = 0); (x < im->out.w); x++) {
         src = (row +
                ((((unsigned int)x * im->in.jinfo.output_width) / (unsigned int)w) * (unsigned int)im->in.jinfo.output_components));

         for ((i = 0); (i < im->in.jinfo.output_components); i++) {
            dst[i] = src[i];
         } /* end inmost for-loop */

         dst += im->in.jinfo.output_components;
      } /* end inner for-loop */
   } /* end outer for-loop */
   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode_for_trim(Epeg_Image *im)
{
   int y;

   if (im->pixels) {
      return 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = 1;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_ISLOW;

   switch (im->color_space) {
      case EPEG_GRAY8:
		   im->in.jinfo.out_color_space = JCS_GRAYSCALE;
		   im->in.jinfo.output_components = 1;
		   break;

      case EPEG_YUV8:
		   im->in.jinfo.out_color_space = JCS_YCbCr;
		   break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
		   im->in.jinfo.out_color_space = JCS_RGB;
		   break;

      case EPEG_CMYK:
		   im->in.jinfo.out_color_space = JCS_CMYK;
		   im->in.jinfo.output_components = 4;
		   break;

      default:
		   break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
      im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
      jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_trim(Epeg_Image *im)
{
   int y, a, b, w, h;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   a = im->out.x;
   b = im->out.y;

   /* dummy condition to use value stored to 'w': */
   if (w == 0) {
	   ;
   }

   for ((y = 0); (y < h); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)((y + b) * im->in.jinfo.output_components) * im->in.jinfo.output_width)
                      + (a * im->in.jinfo.output_components));
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_encode(Epeg_Image *im)
{
   void  *data = NULL;
   size_t size = 0;

   if (im->out.f) {
      return 1;
   }

   if (im->out.file) {
      im->out.f = fopen(im->out.file, "wb");
   } else {
      im->out.f = _epeg_memfile_write_open(&data, &size);
   }
   if (!im->out.f) {
      im->error = 1;
      return 1;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_create_compress(&(im->out.jinfo));
   jpeg_stdio_dest(&(im->out.jinfo), im->out.f);
   im->out.jinfo.image_width = (JDIMENSION)im->out.w;
   im->out.jinfo.image_height = (JDIMENSION)im->out.h;
   im->out.jinfo.input_components = im->in.jinfo.output_components;
   im->out.jinfo.in_color_space = im->in.jinfo.out_color_space;
   im->out.jinfo.dct_method = JDCT_IFAST;
   im->out.jinfo.dct_method = im->in.jinfo.dct_method;
   jpeg_set_defaults(&(im->out.jinfo));
   jpeg_set_quality(&(im->out.jinfo), im->out.quality, TRUE);

   if (im->out.quality >= 90) {
      im->out.jinfo.comp_info[0].h_samp_factor = 1;
      im->out.jinfo.comp_info[0].v_samp_factor = 1;
      im->out.jinfo.comp_info[1].h_samp_factor = 1;
      im->out.jinfo.comp_info[1].v_samp_factor = 1;
      im->out.jinfo.comp_info[2].h_samp_factor = 1;
      im->out.jinfo.comp_info[2].v_samp_factor = 1;
   }
   jpeg_start_compress(&(im->out.jinfo), TRUE);

   if (im->out.comment) {
      jpeg_write_marker(&(im->out.jinfo), JPEG_COM,
                        (const JOCTET *)im->out.comment,
                        (unsigned int)strlen(im->out.comment));
   }

   if (im->out.thumbnail_info) {
      char buf[8192];

      if (im->in.file) {
         snprintf(buf, sizeof(buf), "Thumb::URI\nfile://%s", im->in.file);
         jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7),
                           (const JOCTET *)buf, (unsigned int)strlen(buf));
         snprintf(buf, sizeof(buf), "Thumb::MTime\n%llu",
#if defined(HAVE_UINTMAX_T) && !defined(__LP64__)
                  (uintmax_t)im->stat_info.st_mtime);
#else
               (unsigned long long int)im->stat_info.st_mtime);
#endif /* HAVE_UINTMAX_T && !__LP64__ */
      }
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Width\n%i", im->in.w);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Height\n%i", im->in.h);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Mimetype\nimage/jpeg");
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
   }

   while (im->out.jinfo.next_scanline < im->out.h) {
      jpeg_write_scanlines(&(im->out.jinfo),
                           &(im->lines[im->out.jinfo.next_scanline]), 1);
   }

   jpeg_finish_compress(&(im->out.jinfo));

   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if ((im->in.f) && (im->in.file)) {
      fclose(im->in.f);
   }
   if ((im->in.f) && (!im->in.file)) {
      _epeg_memfile_read_close(im->in.f);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if ((im->out.f) && (im->out.file)) {
      fclose(im->out.f);
   }
   if ((im->out.f) && (!im->out.file)) {
      _epeg_memfile_write_close(im->out.f);
   }
   im->in.f = NULL;
   im->out.f = NULL;

   if (im->out.mem.data) {
	   *(im->out.mem.data) = (unsigned char *)data;
   }
   if (im->out.mem.size) {
	   *(im->out.mem.size) = (int)size;
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static void _epeg_fatal_error_handler(j_common_ptr cinfo)
{
   emptr errmgr;

   errmgr = (emptr)cinfo->err;
   longjmp(errmgr->setjmp_buffer, 1);
   return;
}

/* silence '-Wunused-macros' warnings: */
#ifdef MAX
# undef MAX
#endif /* MAX */

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_memfile.c': */
/* epeg_memfile.c */
/* gets built into the libepeg library */

#include <stdio.h>
#include "Epeg.h"
#include "epeg_private.h"

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_read_open(void *data, size_t size)
{
#ifdef HAVE_FMEMOPEN
   return (FILE *)fmemopen(data, size, "r");
#else
   FILE *f;

   f = tmpfile();
   if (!f) {
      return NULL;
   }
   fwrite(data, size, (size_t)1L, f);
   rewind(f);
   return f;
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_read_close(FILE *f)
{
#ifdef HAVE_FMEMOPEN
   fclose(f);
#else
   /* FIXME: make these different halves of the conditional actually different
    * from one another */
   fclose(f);
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only struct and typedef; unnecessary to document: */
typedef struct _Eet_Memfile_Write_Info Eet_Memfile_Write_Info;
struct _Eet_Memfile_Write_Info
{
   FILE *f;
   void **data;
   size_t *size;
};

/* static internal private-only global variables; unnecessary to document: */
static int _epeg_memfile_info_alloc_num = 0;
static int _epeg_memfile_info_num = 0;
static Eet_Memfile_Write_Info *_epeg_memfile_info = NULL;

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_write_open(void **data, size_t *size)
{
#ifdef HAVE_OPEN_MEMSTREAM
   return open_memstream((char **)data, size);
#else
   FILE *f;

   _epeg_memfile_info_num++;
   if (_epeg_memfile_info_num > _epeg_memfile_info_alloc_num) {
      Eet_Memfile_Write_Info *tmp;

      _epeg_memfile_info_alloc_num += 16;
      tmp = (Eet_Memfile_Write_Info *)realloc(_epeg_memfile_info,
                                              ((unsigned long)_epeg_memfile_info_alloc_num *
                                               sizeof(Eet_Memfile_Write_Info)));
      if (!tmp) {
         _epeg_memfile_info_alloc_num -= 16;
         _epeg_memfile_info_num--;
         return NULL;
      }
      _epeg_memfile_info = tmp;
   }
   f = tmpfile();
   if (!f) {
      _epeg_memfile_info_num--;
      return NULL;
   }
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].f = f;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].data = data;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].size = size;
   return f;
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_write_close(FILE *f)
{
#ifdef HAVE_OPEN_MEMSTREAM
   fclose(f);
#else
   int i;

   for ((i = 0); (i < _epeg_memfile_info_num); i++) {
      if (_epeg_memfile_info[i].f == f) {
         int j;

         fseek(f, 0L, SEEK_END);
         (*(_epeg_memfile_info[i].size)) = (size_t)ftell(f);
         rewind(f);
         (*(_epeg_memfile_info[i].data)) = malloc(*(_epeg_memfile_info[i].size));
         if (!(*(_epeg_memfile_info[i].data))) {
            fclose(f);
            (*(_epeg_memfile_info[i].size)) = 0;
            return;
         }
         fread((*(_epeg_memfile_info[i].data)), (*(_epeg_memfile_info[i].size)),
               (size_t)1L, f);
         for ((j = (i + 1)); (j < _epeg_memfile_info_num); j++) {
            _epeg_memfile_info[(j - 1)] = _epeg_memfile_info[j];
         }
         _epeg_memfile_info_num--;
         fclose(f);
         return;
      } /* end "if" */
   } /* end for-loop */
   fclose(f);
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_main.c': */
/* epeg_main.c for src/lib for epeg */
/* gets built into the libepeg library */

#include <stdio.h>
#include <stdlib.h>
#include "Epeg.h"
#include "epeg_private.h"

static Epeg_Image*_epeg_open_header(Epeg_Image *im);
static int _epeg_decode(Epeg_Image *im);
static int _epeg_scale(Epeg_Image *im);
static int _epeg_decode_for_trim(Epeg_Image *im);
static int _epeg_trim(Epeg_Image *im);
static int _epeg_encode(Epeg_Image *im);

static void _epeg_fatal_error_handler(j_common_ptr cinfo);

#ifndef MIN
# define MIN(__x,__y) ((__x) < (__y) ? (__x) : (__y))
#endif /* !MIN */
#ifndef MAX
# define MAX(__x,__y) ((__x) > (__y) ? (__x) : (__y))
#endif /* !MAX */

/**
 * Open a JPEG image by filename.
 * @param file The file path to open.
 * @return A handle to the opened JPEG file, with the header decoded.
 *
 * This function opens the file indicated by the @p file parameter, and
 * attempts to decode it as a jpeg file. If this failes, NULL is returned.
 * Otherwise a valid handle to an open JPEG file is returned that can be used
 * by other Epeg calls.
 *
 * The @p file must be a pointer to a valid C string, NUL (0 byte) terminated
 * thats is a relative or absolute file path. If not results are not
 * determined.
 *
 * See also: epeg_memory_open(), epeg_close()
 */
extern Epeg_Image *epeg_file_open(const char *file)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.file = strdup(file);
   im->in.f = fopen(im->in.file, "rb");
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   fstat(fileno(im->in.f), &(im->stat_info));
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Open a JPEG image stored in memory.
 * @param data A pointer to the memory containing the JPEG data.
 * @param size The size of the memory segment containing the JPEG.
 * @return  A handle to the opened JPEG, with the header decoded.
 *
 * This function opens a JPEG file that is stored in memory pointed to by
 * @p data, and that is @p size bytes in size. If successful a valid handle
 * is returned, or on failure NULL is returned.
 *
 * See also: epeg_file_open(), epeg_close()
 */
extern Epeg_Image *epeg_memory_open(unsigned char *data, int size)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.f = _epeg_memfile_read_open(data, (size_t)size);
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Return the original JPEG pixel size.
 * @param im A handle to an opened Epeg image.
 * @param w A pointer to the width value in pixels to be filled in.
 * @param h A pointer to the height value in pixels to be filled in.
 * @return Nothing.
 *
 * Returns the image size in pixels (well not really).
 *
 * See also: epeg_colorspace_get()
 */
extern void epeg_size_get(Epeg_Image *im, int *w, int *h)
{
	if (w) {
		*w = im->in.w;
	}
	if (h) {
		*h = im->in.h;
	}
}

/**
 * Return the original JPEG pixel color space.
 * @param im A handle to an opened Epeg image.
 * @param space A pointer to the color space value to be filled in.
 * @return Nothing.
 *
 * Returns the image color space (not yet though).
 *
 * See also: epeg_size_get()
 */
extern void epeg_colorspace_get(Epeg_Image *im, int *space)
{
	if (space) {
		*space = im->color_space;
	}
}

/**
 * Set the size of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param w The width of the image to decode at, in pixels.
 * @param h The height of the image to decode at, in pixels.
 * @return Nothing.
 *
 * Sets the size at which to decode the JPEG image, giving an optimized load
 * that only decodes the pixels needed.
 *
 * See also: epeg_decode_bounds_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_size_set(Epeg_Image *im, int w, int h)
{
   if (im->pixels) {
	   return;
   }
   if (w < 1) {
	   w = 1;
   } else if (w > im->in.w) {
	   w = im->in.w;
   }
   if (h < 1) {
	   h = 1;
   } else if (h > im->in.h) {
	   h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   im->out.x = 0;
   im->out.y = 0;
}

/**
 * Set the bounds of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param x Boundary X
 * @param y Boundary Y
 * @param w Boundary W
 * @param h Boundary H
 * @return Nothing.
 *
 * Sets the bounds inside which to decode the JPEG image,
 * giving an optimized load that only decodes the bounded pixels.
 * (???)
 *
 * See also: epeg_decode_size_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_bounds_set(Epeg_Image *im, int x, int y, int w, int h)
{
   if (im->pixels) {
      return;
   }
   if (w < 1) {
      w = 1;
   } else if (w > im->in.w) {
      w = im->in.w;
   }
   if (h < 1) {
      h = 1;
   } else if (h > im->in.h) {
      h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   if (x < 0) {
      x = 0;
   }
   if (y < 0) {
      y = 0;
   }
   im->out.x = x;
   im->out.y = y;
}

/**
 * Set the colorspace in which to decode the image.
 * @param im A handle to an opened Epeg image.
 * @param colorspace The colorspace to decode the image in.
 * @return Nothing.
 *
 * This sets the colorspace to decode the image in. The default is EPEG_YUV8,
 * as this is normally the native colorspace of a JPEG file, avoiding any
 * colorspace conversions for a faster load and/or save.
 *
 * See also: epeg_decode_size_set(), epeg_decode_bounds_set()
 */
extern void epeg_decode_colorspace_set(Epeg_Image *im,
                                       Epeg_Colorspace colorspace)
{
	if (im->pixels) {
		return;
	}
	if ((colorspace < EPEG_GRAY8) || (colorspace > EPEG_ARGB32)) {
		return;
	}
	im->color_space = colorspace;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get_as_RGB8()
 */
extern const void *epeg_pixels_get(Epeg_Image *im, int x, int y,  int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
         return NULL;
      }
   }

   if (!im->pixels) {
      return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* go through all 8 values in type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace): */
   if (im->color_space == EPEG_GRAY8) {
      /* this whole block is pretty much the same thing we do for each
       * of the other enumeration values as well... could probably be simplified
       * with a function-like macro or some other sort of meta-programming: */
      unsigned char *pix, *p;

      /* '1': */
      pix = (unsigned char *)malloc((size_t)(w * h * 1L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = s[0];
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_YUV8) {
      unsigned char *pix, *p;

      /* '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGR8) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* reverse order: */
            p[0] = s[2];
            p[1] = s[1];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGBA8) {
      unsigned char *pix, *p;

      /* '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGRA8) {
      unsigned char *pix, *p;

      /* also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = 0xff;
            /* reverse order: */
            p[1] = s[2];
            p[2] = s[1];
            p[3] = s[0];
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_ARGB32) {
      unsigned int *pix, *p;

      /* still also '4': */
      pix = (unsigned int *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (0xff000000 | (unsigned int)(s[0] << 16) | (unsigned int)(s[1] << 8) | (s[2]));
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* another thing that is also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* back to same order again: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } /* end "if-else" that seems like it should really be a "switch" */
   return NULL;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get()
 */
extern const void *epeg_pixels_get_as_RGB8(Epeg_Image *im, int x, int y,
                                           int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
		   return NULL;
	   }
   }

   if (!im->pixels) {
	   return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* unlike the "non-'_as_RGB8'-version" of this function,
    * there are no 'else'-es separating the cases here. Also, we only go through
    * three of the enumeration values of type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace) here: */
   if (im->color_space == EPEG_GRAY8) {
      unsigned char *pix, *p;

      /* get right into the '3' ones: */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[0];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (unsigned char)(MIN(255, (s[0] * s[3]) / 255));
            p[1] = (unsigned char)(MIN(255, (s[1] * s[3]) / 255));
            p[2] = (unsigned char)(MIN(255, (s[2] * s[3]) / 255));
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   return NULL;
}

/**
 * Free requested pixel block from an image.
 * @param im A handle to an opened Epeg image (unused).
 * @param data The pointer to the image pixels.
 * @return Nothing.
 *
 * This frees the data for a block of pixels requested from image @p im.
 * @p data must be a valid (non NULL) pointer to a pixel block taken from the
 * image @p im by epeg_pixels_get() and must be called before the image is
 * closed by epeg_close().
 */
extern void epeg_pixels_free(Epeg_Image *im, const void *data)
{
#pragma unused (im)
   free((void *)data);
}

/**
 * Get the image comment field as a string.
 * @param im A handle to an opened Epeg image.
 * @return A pointer to the loaded image comments.
 *
 * This function returns the comment field as a string (NUL byte terminated)
 * of the loaded image @p im, if there is a comment, or NULL if no comment is
 * saved with the image. Consider the string returned to be read-only.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern const char *epeg_comment_get(Epeg_Image *im)
{
   return im->in.comment;
}

/**
 * Get thumbnail comments of loaded image.
 * @param im A handle to an opened Epeg image.
 * @param info Pointer to a thumbnail info struct to be filled in.
 * @return Nothing.
 *
 * This function retrieves thumbnail comments written by Epeg to any saved
 * JPEG files. If no thumbnail comments were saved, the fields will be 0 in
 * the @p info struct on return.
 *
 * See also: epeg_comment_get(), epeg_thumbnail_comments_enable()
 */
extern void epeg_thumbnail_comments_get(Epeg_Image *im,
                                        Epeg_Thumbnail_Info *info)
{
   if (!info) {
      return;
   }
   info->uri = im->in.thumb_info.uri;
   info->mtime = im->in.thumb_info.mtime;
   info->w = im->in.thumb_info.w;
   info->h = im->in.thumb_info.h;
   info->mimetype = im->in.thumb_info.mime;
}

/**
 * Set the comment field of the image for saving.
 * @param im A handle to an opened Epeg image.
 * @param comment The comment to set.
 * @return Nothing.
 *
 * Set the comment for the image file for when it gets saved. This is a NUL
 * byte terminated C string. If @p comment is NULL the output file will have
 * no comment field.
 *
 * The default comment will be any comment loaded from the input file.
 *
 * See also: epeg_comment_get()
 */
extern void epeg_comment_set(Epeg_Image *im, const char *comment)
{
   if (im->out.comment) {
      free(im->out.comment);
   }
   if (!comment) {
      im->out.comment = NULL;
   } else {
      im->out.comment = strdup(comment);
   }
}

/**
 * Set the encoding quality of the saved image.
 * @param im A handle to an opened Epeg image.
 * @param quality The quality of encoding from 0 to 100.
 * @return Nothing.
 *
 * Set the quality of the output encoded image. Values from 0 to 100
 * inclusive are valid, with 100 being the maximum quality, and 0 being the
 * minimum. If the quality is set equal to or above 90%, the output U and V
 * color planes are encoded at 1:1 with the Y plane.
 *
 * The default quality is 75.
 *
 * See also: epeg_comment_set()
 */
extern void epeg_quality_set(Epeg_Image *im, int quality)
{
   if (quality < 0) {
      quality = 0;
   } else if (quality > 100) {
      quality = 100;
   }
   im->out.quality = quality;
}

/**
 * Enable thumbnail comments in saved image.
 * @param im A handle to an opened Epeg image.
 * @param onoff A boolean on and off enabling flag.
 * @return Nothing.
 *
 * if @p onoff is 1, the output file will have thumbnail comments added to
 * it, and if it is 0, it will not. The default is 0.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern void epeg_thumbnail_comments_enable(Epeg_Image *im, int onoff)
{
   im->out.thumbnail_info = (char)onoff;
}

/**
 * Set the output file path for the image when saved.
 * @param im A handle to an opened Epeg image.
 * @param file The path to the output file.
 * @return Nothing.
 *
 * This sets the output file path name (either a full or relative path name)
 * to where the file will be written when saved. @p file must be a NUL
 * terminated C string containing the path to the file to be saved to. If it is
 * NULL, then the image will not be saved to a file when calling epeg_encode().
 *
 * See also: epeg_memory_output_set(), epeg_encode()
 */
extern void epeg_file_output_set(Epeg_Image *im, const char *file)
{
   if (im->out.file) {
      free(im->out.file);
   }
   if (!file) {
      im->out.file = NULL;
   } else {
      im->out.file = strdup(file);
   }
}

/**
 * Set the output file to be a block of allocated memory.
 * @param im A handle to an opened Epeg image.
 * @param data A pointer to a pointer to a memory block.
 * @param size A pointer to a counter of the size of the memory block.
 * @return Nothing.
 *
 * This sets the output encoding of the image when saved to be allocated
 * memory. After epeg_close() is called the pointer pointed to by @p data
 * and the integer pointed to by @p size will contain the pointer to the
 * memory block and its size in bytes, respecitvely. The memory block can be
 * freed with the free() function call. If the save fails the pointer to the
 * memory block will be unaffected, as will the size.
 *
 * See also: epeg_file_output_set(), epeg_encode()
 */
extern void epeg_memory_output_set(Epeg_Image *im, unsigned char **data,
                                   int *size)
{
   im->out.mem.data = data;
   im->out.mem.size = size;
}

/**
 * This saves the image to its specified destination.
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * This saves the image @p im to its destination specified by
 * epeg_file_output_set() or epeg_memory_output_set(). The image will be
 * encoded at the decoded pixel size, using the quality, comment,
 * and thumbnail comment settings set on the image.
 *
 * See also: epeg_file_output_set(), epeg_memory_output_set()
 */
extern int epeg_encode(Epeg_Image *im)
{
   if (_epeg_decode(im) != 0) {
      return 1;
   }
   if (_epeg_scale(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * FIXME: Document this with a short, sentence-long description of epeg_trim()
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * FIXME: Document this with a longer, paragraph-long description.
 */
extern int epeg_trim(Epeg_Image *im)
{
   if (_epeg_decode_for_trim(im) != 0) {
      return 1;
   }
   if (_epeg_trim(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * Close an image handle.
 * @param im A handle to an opened Epeg image.
 * @return Nothing.
 *
 * This closes an opened image handle and frees all memory associated with it.
 * It does NOT free encoded data generated by epeg_memory_output_set() followed
 * by epeg_encode(), nor does it guarantee to free any data received by
 * epeg_pixels_get(). Once an image handle is closed, consider it invalid.
 *
 * See also: epeg_file_open(), epeg_memory_open()
 */
extern void epeg_close(Epeg_Image *im)
{
   if (im->pixels) {
      free(im->pixels);
   }
   if (im->lines) {
      free(im->lines);
   }
   if (im->in.file) {
      free(im->in.file);
   }
   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if (im->in.f) {
      fclose(im->in.f);
   }
   if (im->in.comment) {
      free(im->in.comment);
   }
   if (im->in.thumb_info.uri) {
      free(im->in.thumb_info.uri);
   }
   if (im->in.thumb_info.mime) {
      free(im->in.thumb_info.mime);
   }
   if (im->out.file) {
      free(im->out.file);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if (im->out.f) {
      fclose(im->out.f);
   }
   if (im->out.comment) {
      free(im->out.comment);
   }
   free(im);
}

/* static internal private-only function; unnecessary to document: */
static Epeg_Image *_epeg_open_header(Epeg_Image *im)
{
   struct jpeg_marker_struct *m;

   im->in.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      error:
      epeg_close(im);
      im = NULL;
      return NULL;
   }

   jpeg_create_decompress(&(im->in.jinfo));
   jpeg_save_markers(&(im->in.jinfo), (JPEG_APP0 + 7), 1024);
   jpeg_save_markers(&(im->in.jinfo), JPEG_COM, 65535);
   jpeg_stdio_src(&(im->in.jinfo), im->in.f);
   jpeg_read_header(&(im->in.jinfo), TRUE);
   im->in.w = (int)im->in.jinfo.image_width;
   im->in.h = (int)im->in.jinfo.image_height;
   if (im->in.w <= 1) {
      goto error;
   }
   if (im->in.h <= 1) {
      goto error;
   }

   im->out.w = im->in.w;
   im->out.h = im->in.h;

   im->color_space = (((im->in.color_space = im->in.jinfo.out_color_space) == JCS_GRAYSCALE) ? EPEG_GRAY8 : EPEG_RGB8);
   if (im->in.color_space == JCS_CMYK) {
      im->color_space = EPEG_CMYK;
   }

   for ((m = im->in.jinfo.marker_list); m; (m = m->next)) {
      if (m->marker == JPEG_COM) {
         if (im->in.comment) {
            free(im->in.comment);
         }
         im->in.comment = (char *)malloc((size_t)(m->data_length + 1));
         if (im->in.comment) {
            memcpy(im->in.comment, m->data, (size_t)m->data_length);
            im->in.comment[m->data_length] = 0;
         }
      } else if (m->marker == (JPEG_APP0 + 7))  {
         if ((m->data_length > 7) &&
             (!strncmp((char *)m->data, "Thumb::", (size_t)7))) {
            char *p, *p2;

            p = (char *)malloc((size_t)(m->data_length + 1));
            if (p) {
               memcpy(p, m->data, (size_t)m->data_length);
               p[m->data_length] = 0;
               p2 = strchr(p, '\n');
               if (p2) {
                  p2[0] = 0;
                  if (!strcmp(p, "Thumb::URI")) {
                     im->in.thumb_info.uri = strdup(p2 + 1);
                  } else if (!strcmp(p, "Thumb::MTime")) {
                     sscanf((p2 + 1), "%llu", &(im->in.thumb_info.mtime));
                  } else if (!strcmp(p, "Thumb::Image::Width")) {
                     im->in.thumb_info.w = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Image::Height")) {
                     im->in.thumb_info.h = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Mimetype")) {
                     im->in.thumb_info.mime = strdup(p2 + 1);
                  }
               } /* end "if (p2)" */
               free(p);
            } /* end "if (p)" */
         }
      }
   } /* end for-loop */
   return im;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode(Epeg_Image *im)
{
   int scale, scalew, scaleh, y;

   if (im->pixels) {
      return 1;
   }

   scalew = (im->in.w / im->out.w);
   scaleh = (im->in.h / im->out.h);

   scale = scalew;
   if (scaleh < scalew) {
      scale = scaleh;
   }

   if (scale > 8) {
      scale = 8;
   } else if (scale < 1) {
      scale = 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = (unsigned int)scale;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_IFAST;

   switch (im->color_space) {
      case EPEG_GRAY8:
			 im->in.jinfo.out_color_space = JCS_GRAYSCALE;
			 im->in.jinfo.output_components = 1;
			 break;

      case EPEG_YUV8:
			 im->in.jinfo.out_color_space = JCS_YCbCr;
			 break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
			 im->in.jinfo.out_color_space = JCS_RGB;
			 break;

      case EPEG_CMYK:
			 im->in.jinfo.out_color_space = JCS_CMYK;
			 im->in.jinfo.output_components = 4;
			 break;

      default:
			 break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      epeg_close(im);
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
	   jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_scale(Epeg_Image *im)
{
   unsigned char *dst, *row, *src;
   int            x, y, w, h, i;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   for ((y = 0); (y < h); y++) {
      row = (im->pixels +
             ((((unsigned int)y * im->in.jinfo.output_height) / (unsigned int)h) * (unsigned int)im->in.jinfo.output_components * im->in.jinfo.output_width));
      dst = (im->pixels +
             ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));

      for ((x = 0); (x < im->out.w); x++) {
         src = (row +
                ((((unsigned int)x * im->in.jinfo.output_width) / (unsigned int)w) * (unsigned int)im->in.jinfo.output_components));

         for ((i = 0); (i < im->in.jinfo.output_components); i++) {
            dst[i] = src[i];
         } /* end inmost for-loop */

         dst += im->in.jinfo.output_components;
      } /* end inner for-loop */
   } /* end outer for-loop */
   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode_for_trim(Epeg_Image *im)
{
   int y;

   if (im->pixels) {
      return 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = 1;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_ISLOW;

   switch (im->color_space) {
      case EPEG_GRAY8:
		   im->in.jinfo.out_color_space = JCS_GRAYSCALE;
		   im->in.jinfo.output_components = 1;
		   break;

      case EPEG_YUV8:
		   im->in.jinfo.out_color_space = JCS_YCbCr;
		   break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
		   im->in.jinfo.out_color_space = JCS_RGB;
		   break;

      case EPEG_CMYK:
		   im->in.jinfo.out_color_space = JCS_CMYK;
		   im->in.jinfo.output_components = 4;
		   break;

      default:
		   break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
      im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
      jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_trim(Epeg_Image *im)
{
   int y, a, b, w, h;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   a = im->out.x;
   b = im->out.y;

   /* dummy condition to use value stored to 'w': */
   if (w == 0) {
	   ;
   }

   for ((y = 0); (y < h); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)((y + b) * im->in.jinfo.output_components) * im->in.jinfo.output_width)
                      + (a * im->in.jinfo.output_components));
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_encode(Epeg_Image *im)
{
   void  *data = NULL;
   size_t size = 0;

   if (im->out.f) {
      return 1;
   }

   if (im->out.file) {
      im->out.f = fopen(im->out.file, "wb");
   } else {
      im->out.f = _epeg_memfile_write_open(&data, &size);
   }
   if (!im->out.f) {
      im->error = 1;
      return 1;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_create_compress(&(im->out.jinfo));
   jpeg_stdio_dest(&(im->out.jinfo), im->out.f);
   im->out.jinfo.image_width = (JDIMENSION)im->out.w;
   im->out.jinfo.image_height = (JDIMENSION)im->out.h;
   im->out.jinfo.input_components = im->in.jinfo.output_components;
   im->out.jinfo.in_color_space = im->in.jinfo.out_color_space;
   im->out.jinfo.dct_method = JDCT_IFAST;
   im->out.jinfo.dct_method = im->in.jinfo.dct_method;
   jpeg_set_defaults(&(im->out.jinfo));
   jpeg_set_quality(&(im->out.jinfo), im->out.quality, TRUE);

   if (im->out.quality >= 90) {
      im->out.jinfo.comp_info[0].h_samp_factor = 1;
      im->out.jinfo.comp_info[0].v_samp_factor = 1;
      im->out.jinfo.comp_info[1].h_samp_factor = 1;
      im->out.jinfo.comp_info[1].v_samp_factor = 1;
      im->out.jinfo.comp_info[2].h_samp_factor = 1;
      im->out.jinfo.comp_info[2].v_samp_factor = 1;
   }
   jpeg_start_compress(&(im->out.jinfo), TRUE);

   if (im->out.comment) {
      jpeg_write_marker(&(im->out.jinfo), JPEG_COM,
                        (const JOCTET *)im->out.comment,
                        (unsigned int)strlen(im->out.comment));
   }

   if (im->out.thumbnail_info) {
      char buf[8192];

      if (im->in.file) {
         snprintf(buf, sizeof(buf), "Thumb::URI\nfile://%s", im->in.file);
         jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7),
                           (const JOCTET *)buf, (unsigned int)strlen(buf));
         snprintf(buf, sizeof(buf), "Thumb::MTime\n%llu",
#if defined(HAVE_UINTMAX_T) && !defined(__LP64__)
                  (uintmax_t)im->stat_info.st_mtime);
#else
               (unsigned long long int)im->stat_info.st_mtime);
#endif /* HAVE_UINTMAX_T && !__LP64__ */
      }
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Width\n%i", im->in.w);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Height\n%i", im->in.h);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Mimetype\nimage/jpeg");
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
   }

   while (im->out.jinfo.next_scanline < im->out.h) {
      jpeg_write_scanlines(&(im->out.jinfo),
                           &(im->lines[im->out.jinfo.next_scanline]), 1);
   }

   jpeg_finish_compress(&(im->out.jinfo));

   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if ((im->in.f) && (im->in.file)) {
      fclose(im->in.f);
   }
   if ((im->in.f) && (!im->in.file)) {
      _epeg_memfile_read_close(im->in.f);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if ((im->out.f) && (im->out.file)) {
      fclose(im->out.f);
   }
   if ((im->out.f) && (!im->out.file)) {
      _epeg_memfile_write_close(im->out.f);
   }
   im->in.f = NULL;
   im->out.f = NULL;

   if (im->out.mem.data) {
	   *(im->out.mem.data) = (unsigned char *)data;
   }
   if (im->out.mem.size) {
	   *(im->out.mem.size) = (int)size;
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static void _epeg_fatal_error_handler(j_common_ptr cinfo)
{
   emptr errmgr;

   errmgr = (emptr)cinfo->err;
   longjmp(errmgr->setjmp_buffer, 1);
   return;
}

/* silence '-Wunused-macros' warnings: */
#ifdef MAX
# undef MAX
#endif /* MAX */

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_memfile.c': */
/* epeg_memfile.c */
/* gets built into the libepeg library */

#include <stdio.h>
#include "Epeg.h"
#include "epeg_private.h"

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_read_open(void *data, size_t size)
{
#ifdef HAVE_FMEMOPEN
   return (FILE *)fmemopen(data, size, "r");
#else
   FILE *f;

   f = tmpfile();
   if (!f) {
      return NULL;
   }
   fwrite(data, size, (size_t)1L, f);
   rewind(f);
   return f;
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_read_close(FILE *f)
{
#ifdef HAVE_FMEMOPEN
   fclose(f);
#else
   /* FIXME: make these different halves of the conditional actually different
    * from one another */
   fclose(f);
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only struct and typedef; unnecessary to document: */
typedef struct _Eet_Memfile_Write_Info Eet_Memfile_Write_Info;
struct _Eet_Memfile_Write_Info
{
   FILE *f;
   void **data;
   size_t *size;
};

/* static internal private-only global variables; unnecessary to document: */
static int _epeg_memfile_info_alloc_num = 0;
static int _epeg_memfile_info_num = 0;
static Eet_Memfile_Write_Info *_epeg_memfile_info = NULL;

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_write_open(void **data, size_t *size)
{
#ifdef HAVE_OPEN_MEMSTREAM
   return open_memstream((char **)data, size);
#else
   FILE *f;

   _epeg_memfile_info_num++;
   if (_epeg_memfile_info_num > _epeg_memfile_info_alloc_num) {
      Eet_Memfile_Write_Info *tmp;

      _epeg_memfile_info_alloc_num += 16;
      tmp = (Eet_Memfile_Write_Info *)realloc(_epeg_memfile_info,
                                              ((unsigned long)_epeg_memfile_info_alloc_num *
                                               sizeof(Eet_Memfile_Write_Info)));
      if (!tmp) {
         _epeg_memfile_info_alloc_num -= 16;
         _epeg_memfile_info_num--;
         return NULL;
      }
      _epeg_memfile_info = tmp;
   }
   f = tmpfile();
   if (!f) {
      _epeg_memfile_info_num--;
      return NULL;
   }
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].f = f;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].data = data;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].size = size;
   return f;
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_write_close(FILE *f)
{
#ifdef HAVE_OPEN_MEMSTREAM
   fclose(f);
#else
   int i;

   for ((i = 0); (i < _epeg_memfile_info_num); i++) {
      if (_epeg_memfile_info[i].f == f) {
         int j;

         fseek(f, 0L, SEEK_END);
         (*(_epeg_memfile_info[i].size)) = (size_t)ftell(f);
         rewind(f);
         (*(_epeg_memfile_info[i].data)) = malloc(*(_epeg_memfile_info[i].size));
         if (!(*(_epeg_memfile_info[i].data))) {
            fclose(f);
            (*(_epeg_memfile_info[i].size)) = 0;
            return;
         }
         fread((*(_epeg_memfile_info[i].data)), (*(_epeg_memfile_info[i].size)),
               (size_t)1L, f);
         for ((j = (i + 1)); (j < _epeg_memfile_info_num); j++) {
            _epeg_memfile_info[(j - 1)] = _epeg_memfile_info[j];
         }
         _epeg_memfile_info_num--;
         fclose(f);
         return;
      } /* end "if" */
   } /* end for-loop */
   fclose(f);
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_main.c': */
/* epeg_main.c for src/lib for epeg */
/* gets built into the libepeg library */

#include <stdio.h>
#include <stdlib.h>
#include "Epeg.h"
#include "epeg_private.h"

static Epeg_Image*_epeg_open_header(Epeg_Image *im);
static int _epeg_decode(Epeg_Image *im);
static int _epeg_scale(Epeg_Image *im);
static int _epeg_decode_for_trim(Epeg_Image *im);
static int _epeg_trim(Epeg_Image *im);
static int _epeg_encode(Epeg_Image *im);

static void _epeg_fatal_error_handler(j_common_ptr cinfo);

#ifndef MIN
# define MIN(__x,__y) ((__x) < (__y) ? (__x) : (__y))
#endif /* !MIN */
#ifndef MAX
# define MAX(__x,__y) ((__x) > (__y) ? (__x) : (__y))
#endif /* !MAX */

/**
 * Open a JPEG image by filename.
 * @param file The file path to open.
 * @return A handle to the opened JPEG file, with the header decoded.
 *
 * This function opens the file indicated by the @p file parameter, and
 * attempts to decode it as a jpeg file. If this failes, NULL is returned.
 * Otherwise a valid handle to an open JPEG file is returned that can be used
 * by other Epeg calls.
 *
 * The @p file must be a pointer to a valid C string, NUL (0 byte) terminated
 * thats is a relative or absolute file path. If not results are not
 * determined.
 *
 * See also: epeg_memory_open(), epeg_close()
 */
extern Epeg_Image *epeg_file_open(const char *file)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.file = strdup(file);
   im->in.f = fopen(im->in.file, "rb");
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   fstat(fileno(im->in.f), &(im->stat_info));
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Open a JPEG image stored in memory.
 * @param data A pointer to the memory containing the JPEG data.
 * @param size The size of the memory segment containing the JPEG.
 * @return  A handle to the opened JPEG, with the header decoded.
 *
 * This function opens a JPEG file that is stored in memory pointed to by
 * @p data, and that is @p size bytes in size. If successful a valid handle
 * is returned, or on failure NULL is returned.
 *
 * See also: epeg_file_open(), epeg_close()
 */
extern Epeg_Image *epeg_memory_open(unsigned char *data, int size)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.f = _epeg_memfile_read_open(data, (size_t)size);
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Return the original JPEG pixel size.
 * @param im A handle to an opened Epeg image.
 * @param w A pointer to the width value in pixels to be filled in.
 * @param h A pointer to the height value in pixels to be filled in.
 * @return Nothing.
 *
 * Returns the image size in pixels (well not really).
 *
 * See also: epeg_colorspace_get()
 */
extern void epeg_size_get(Epeg_Image *im, int *w, int *h)
{
	if (w) {
		*w = im->in.w;
	}
	if (h) {
		*h = im->in.h;
	}
}

/**
 * Return the original JPEG pixel color space.
 * @param im A handle to an opened Epeg image.
 * @param space A pointer to the color space value to be filled in.
 * @return Nothing.
 *
 * Returns the image color space (not yet though).
 *
 * See also: epeg_size_get()
 */
extern void epeg_colorspace_get(Epeg_Image *im, int *space)
{
	if (space) {
		*space = im->color_space;
	}
}

/**
 * Set the size of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param w The width of the image to decode at, in pixels.
 * @param h The height of the image to decode at, in pixels.
 * @return Nothing.
 *
 * Sets the size at which to decode the JPEG image, giving an optimized load
 * that only decodes the pixels needed.
 *
 * See also: epeg_decode_bounds_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_size_set(Epeg_Image *im, int w, int h)
{
   if (im->pixels) {
	   return;
   }
   if (w < 1) {
	   w = 1;
   } else if (w > im->in.w) {
	   w = im->in.w;
   }
   if (h < 1) {
	   h = 1;
   } else if (h > im->in.h) {
	   h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   im->out.x = 0;
   im->out.y = 0;
}

/**
 * Set the bounds of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param x Boundary X
 * @param y Boundary Y
 * @param w Boundary W
 * @param h Boundary H
 * @return Nothing.
 *
 * Sets the bounds inside which to decode the JPEG image,
 * giving an optimized load that only decodes the bounded pixels.
 * (???)
 *
 * See also: epeg_decode_size_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_bounds_set(Epeg_Image *im, int x, int y, int w, int h)
{
   if (im->pixels) {
      return;
   }
   if (w < 1) {
      w = 1;
   } else if (w > im->in.w) {
      w = im->in.w;
   }
   if (h < 1) {
      h = 1;
   } else if (h > im->in.h) {
      h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   if (x < 0) {
      x = 0;
   }
   if (y < 0) {
      y = 0;
   }
   im->out.x = x;
   im->out.y = y;
}

/**
 * Set the colorspace in which to decode the image.
 * @param im A handle to an opened Epeg image.
 * @param colorspace The colorspace to decode the image in.
 * @return Nothing.
 *
 * This sets the colorspace to decode the image in. The default is EPEG_YUV8,
 * as this is normally the native colorspace of a JPEG file, avoiding any
 * colorspace conversions for a faster load and/or save.
 *
 * See also: epeg_decode_size_set(), epeg_decode_bounds_set()
 */
extern void epeg_decode_colorspace_set(Epeg_Image *im,
                                       Epeg_Colorspace colorspace)
{
	if (im->pixels) {
		return;
	}
	if ((colorspace < EPEG_GRAY8) || (colorspace > EPEG_ARGB32)) {
		return;
	}
	im->color_space = colorspace;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get_as_RGB8()
 */
extern const void *epeg_pixels_get(Epeg_Image *im, int x, int y,  int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
         return NULL;
      }
   }

   if (!im->pixels) {
      return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* go through all 8 values in type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace): */
   if (im->color_space == EPEG_GRAY8) {
      /* this whole block is pretty much the same thing we do for each
       * of the other enumeration values as well... could probably be simplified
       * with a function-like macro or some other sort of meta-programming: */
      unsigned char *pix, *p;

      /* '1': */
      pix = (unsigned char *)malloc((size_t)(w * h * 1L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = s[0];
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_YUV8) {
      unsigned char *pix, *p;

      /* '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGR8) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* reverse order: */
            p[0] = s[2];
            p[1] = s[1];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGBA8) {
      unsigned char *pix, *p;

      /* '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGRA8) {
      unsigned char *pix, *p;

      /* also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = 0xff;
            /* reverse order: */
            p[1] = s[2];
            p[2] = s[1];
            p[3] = s[0];
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_ARGB32) {
      unsigned int *pix, *p;

      /* still also '4': */
      pix = (unsigned int *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (0xff000000 | (unsigned int)(s[0] << 16) | (unsigned int)(s[1] << 8) | (s[2]));
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* another thing that is also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* back to same order again: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } /* end "if-else" that seems like it should really be a "switch" */
   return NULL;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get()
 */
extern const void *epeg_pixels_get_as_RGB8(Epeg_Image *im, int x, int y,
                                           int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
		   return NULL;
	   }
   }

   if (!im->pixels) {
	   return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* unlike the "non-'_as_RGB8'-version" of this function,
    * there are no 'else'-es separating the cases here. Also, we only go through
    * three of the enumeration values of type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace) here: */
   if (im->color_space == EPEG_GRAY8) {
      unsigned char *pix, *p;

      /* get right into the '3' ones: */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[0];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (unsigned char)(MIN(255, (s[0] * s[3]) / 255));
            p[1] = (unsigned char)(MIN(255, (s[1] * s[3]) / 255));
            p[2] = (unsigned char)(MIN(255, (s[2] * s[3]) / 255));
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   return NULL;
}

/**
 * Free requested pixel block from an image.
 * @param im A handle to an opened Epeg image (unused).
 * @param data The pointer to the image pixels.
 * @return Nothing.
 *
 * This frees the data for a block of pixels requested from image @p im.
 * @p data must be a valid (non NULL) pointer to a pixel block taken from the
 * image @p im by epeg_pixels_get() and must be called before the image is
 * closed by epeg_close().
 */
extern void epeg_pixels_free(Epeg_Image *im, const void *data)
{
#pragma unused (im)
   free((void *)data);
}

/**
 * Get the image comment field as a string.
 * @param im A handle to an opened Epeg image.
 * @return A pointer to the loaded image comments.
 *
 * This function returns the comment field as a string (NUL byte terminated)
 * of the loaded image @p im, if there is a comment, or NULL if no comment is
 * saved with the image. Consider the string returned to be read-only.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern const char *epeg_comment_get(Epeg_Image *im)
{
   return im->in.comment;
}

/**
 * Get thumbnail comments of loaded image.
 * @param im A handle to an opened Epeg image.
 * @param info Pointer to a thumbnail info struct to be filled in.
 * @return Nothing.
 *
 * This function retrieves thumbnail comments written by Epeg to any saved
 * JPEG files. If no thumbnail comments were saved, the fields will be 0 in
 * the @p info struct on return.
 *
 * See also: epeg_comment_get(), epeg_thumbnail_comments_enable()
 */
extern void epeg_thumbnail_comments_get(Epeg_Image *im,
                                        Epeg_Thumbnail_Info *info)
{
   if (!info) {
      return;
   }
   info->uri = im->in.thumb_info.uri;
   info->mtime = im->in.thumb_info.mtime;
   info->w = im->in.thumb_info.w;
   info->h = im->in.thumb_info.h;
   info->mimetype = im->in.thumb_info.mime;
}

/**
 * Set the comment field of the image for saving.
 * @param im A handle to an opened Epeg image.
 * @param comment The comment to set.
 * @return Nothing.
 *
 * Set the comment for the image file for when it gets saved. This is a NUL
 * byte terminated C string. If @p comment is NULL the output file will have
 * no comment field.
 *
 * The default comment will be any comment loaded from the input file.
 *
 * See also: epeg_comment_get()
 */
extern void epeg_comment_set(Epeg_Image *im, const char *comment)
{
   if (im->out.comment) {
      free(im->out.comment);
   }
   if (!comment) {
      im->out.comment = NULL;
   } else {
      im->out.comment = strdup(comment);
   }
}

/**
 * Set the encoding quality of the saved image.
 * @param im A handle to an opened Epeg image.
 * @param quality The quality of encoding from 0 to 100.
 * @return Nothing.
 *
 * Set the quality of the output encoded image. Values from 0 to 100
 * inclusive are valid, with 100 being the maximum quality, and 0 being the
 * minimum. If the quality is set equal to or above 90%, the output U and V
 * color planes are encoded at 1:1 with the Y plane.
 *
 * The default quality is 75.
 *
 * See also: epeg_comment_set()
 */
extern void epeg_quality_set(Epeg_Image *im, int quality)
{
   if (quality < 0) {
      quality = 0;
   } else if (quality > 100) {
      quality = 100;
   }
   im->out.quality = quality;
}

/**
 * Enable thumbnail comments in saved image.
 * @param im A handle to an opened Epeg image.
 * @param onoff A boolean on and off enabling flag.
 * @return Nothing.
 *
 * if @p onoff is 1, the output file will have thumbnail comments added to
 * it, and if it is 0, it will not. The default is 0.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern void epeg_thumbnail_comments_enable(Epeg_Image *im, int onoff)
{
   im->out.thumbnail_info = (char)onoff;
}

/**
 * Set the output file path for the image when saved.
 * @param im A handle to an opened Epeg image.
 * @param file The path to the output file.
 * @return Nothing.
 *
 * This sets the output file path name (either a full or relative path name)
 * to where the file will be written when saved. @p file must be a NUL
 * terminated C string containing the path to the file to be saved to. If it is
 * NULL, then the image will not be saved to a file when calling epeg_encode().
 *
 * See also: epeg_memory_output_set(), epeg_encode()
 */
extern void epeg_file_output_set(Epeg_Image *im, const char *file)
{
   if (im->out.file) {
      free(im->out.file);
   }
   if (!file) {
      im->out.file = NULL;
   } else {
      im->out.file = strdup(file);
   }
}

/**
 * Set the output file to be a block of allocated memory.
 * @param im A handle to an opened Epeg image.
 * @param data A pointer to a pointer to a memory block.
 * @param size A pointer to a counter of the size of the memory block.
 * @return Nothing.
 *
 * This sets the output encoding of the image when saved to be allocated
 * memory. After epeg_close() is called the pointer pointed to by @p data
 * and the integer pointed to by @p size will contain the pointer to the
 * memory block and its size in bytes, respecitvely. The memory block can be
 * freed with the free() function call. If the save fails the pointer to the
 * memory block will be unaffected, as will the size.
 *
 * See also: epeg_file_output_set(), epeg_encode()
 */
extern void epeg_memory_output_set(Epeg_Image *im, unsigned char **data,
                                   int *size)
{
   im->out.mem.data = data;
   im->out.mem.size = size;
}

/**
 * This saves the image to its specified destination.
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * This saves the image @p im to its destination specified by
 * epeg_file_output_set() or epeg_memory_output_set(). The image will be
 * encoded at the decoded pixel size, using the quality, comment,
 * and thumbnail comment settings set on the image.
 *
 * See also: epeg_file_output_set(), epeg_memory_output_set()
 */
extern int epeg_encode(Epeg_Image *im)
{
   if (_epeg_decode(im) != 0) {
      return 1;
   }
   if (_epeg_scale(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * FIXME: Document this with a short, sentence-long description of epeg_trim()
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * FIXME: Document this with a longer, paragraph-long description.
 */
extern int epeg_trim(Epeg_Image *im)
{
   if (_epeg_decode_for_trim(im) != 0) {
      return 1;
   }
   if (_epeg_trim(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * Close an image handle.
 * @param im A handle to an opened Epeg image.
 * @return Nothing.
 *
 * This closes an opened image handle and frees all memory associated with it.
 * It does NOT free encoded data generated by epeg_memory_output_set() followed
 * by epeg_encode(), nor does it guarantee to free any data received by
 * epeg_pixels_get(). Once an image handle is closed, consider it invalid.
 *
 * See also: epeg_file_open(), epeg_memory_open()
 */
extern void epeg_close(Epeg_Image *im)
{
   if (im->pixels) {
      free(im->pixels);
   }
   if (im->lines) {
      free(im->lines);
   }
   if (im->in.file) {
      free(im->in.file);
   }
   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if (im->in.f) {
      fclose(im->in.f);
   }
   if (im->in.comment) {
      free(im->in.comment);
   }
   if (im->in.thumb_info.uri) {
      free(im->in.thumb_info.uri);
   }
   if (im->in.thumb_info.mime) {
      free(im->in.thumb_info.mime);
   }
   if (im->out.file) {
      free(im->out.file);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if (im->out.f) {
      fclose(im->out.f);
   }
   if (im->out.comment) {
      free(im->out.comment);
   }
   free(im);
}

/* static internal private-only function; unnecessary to document: */
static Epeg_Image *_epeg_open_header(Epeg_Image *im)
{
   struct jpeg_marker_struct *m;

   im->in.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      error:
      epeg_close(im);
      im = NULL;
      return NULL;
   }

   jpeg_create_decompress(&(im->in.jinfo));
   jpeg_save_markers(&(im->in.jinfo), (JPEG_APP0 + 7), 1024);
   jpeg_save_markers(&(im->in.jinfo), JPEG_COM, 65535);
   jpeg_stdio_src(&(im->in.jinfo), im->in.f);
   jpeg_read_header(&(im->in.jinfo), TRUE);
   im->in.w = (int)im->in.jinfo.image_width;
   im->in.h = (int)im->in.jinfo.image_height;
   if (im->in.w <= 1) {
      goto error;
   }
   if (im->in.h <= 1) {
      goto error;
   }

   im->out.w = im->in.w;
   im->out.h = im->in.h;

   im->color_space = (((im->in.color_space = im->in.jinfo.out_color_space) == JCS_GRAYSCALE) ? EPEG_GRAY8 : EPEG_RGB8);
   if (im->in.color_space == JCS_CMYK) {
      im->color_space = EPEG_CMYK;
   }

   for ((m = im->in.jinfo.marker_list); m; (m = m->next)) {
      if (m->marker == JPEG_COM) {
         if (im->in.comment) {
            free(im->in.comment);
         }
         im->in.comment = (char *)malloc((size_t)(m->data_length + 1));
         if (im->in.comment) {
            memcpy(im->in.comment, m->data, (size_t)m->data_length);
            im->in.comment[m->data_length] = 0;
         }
      } else if (m->marker == (JPEG_APP0 + 7))  {
         if ((m->data_length > 7) &&
             (!strncmp((char *)m->data, "Thumb::", (size_t)7))) {
            char *p, *p2;

            p = (char *)malloc((size_t)(m->data_length + 1));
            if (p) {
               memcpy(p, m->data, (size_t)m->data_length);
               p[m->data_length] = 0;
               p2 = strchr(p, '\n');
               if (p2) {
                  p2[0] = 0;
                  if (!strcmp(p, "Thumb::URI")) {
                     im->in.thumb_info.uri = strdup(p2 + 1);
                  } else if (!strcmp(p, "Thumb::MTime")) {
                     sscanf((p2 + 1), "%llu", &(im->in.thumb_info.mtime));
                  } else if (!strcmp(p, "Thumb::Image::Width")) {
                     im->in.thumb_info.w = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Image::Height")) {
                     im->in.thumb_info.h = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Mimetype")) {
                     im->in.thumb_info.mime = strdup(p2 + 1);
                  }
               } /* end "if (p2)" */
               free(p);
            } /* end "if (p)" */
         }
      }
   } /* end for-loop */
   return im;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode(Epeg_Image *im)
{
   int scale, scalew, scaleh, y;

   if (im->pixels) {
      return 1;
   }

   scalew = (im->in.w / im->out.w);
   scaleh = (im->in.h / im->out.h);

   scale = scalew;
   if (scaleh < scalew) {
      scale = scaleh;
   }

   if (scale > 8) {
      scale = 8;
   } else if (scale < 1) {
      scale = 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = (unsigned int)scale;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_IFAST;

   switch (im->color_space) {
      case EPEG_GRAY8:
			 im->in.jinfo.out_color_space = JCS_GRAYSCALE;
			 im->in.jinfo.output_components = 1;
			 break;

      case EPEG_YUV8:
			 im->in.jinfo.out_color_space = JCS_YCbCr;
			 break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
			 im->in.jinfo.out_color_space = JCS_RGB;
			 break;

      case EPEG_CMYK:
			 im->in.jinfo.out_color_space = JCS_CMYK;
			 im->in.jinfo.output_components = 4;
			 break;

      default:
			 break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      epeg_close(im);
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
	   jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_scale(Epeg_Image *im)
{
   unsigned char *dst, *row, *src;
   int            x, y, w, h, i;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   for ((y = 0); (y < h); y++) {
      row = (im->pixels +
             ((((unsigned int)y * im->in.jinfo.output_height) / (unsigned int)h) * (unsigned int)im->in.jinfo.output_components * im->in.jinfo.output_width));
      dst = (im->pixels +
             ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));

      for ((x = 0); (x < im->out.w); x++) {
         src = (row +
                ((((unsigned int)x * im->in.jinfo.output_width) / (unsigned int)w) * (unsigned int)im->in.jinfo.output_components));

         for ((i = 0); (i < im->in.jinfo.output_components); i++) {
            dst[i] = src[i];
         } /* end inmost for-loop */

         dst += im->in.jinfo.output_components;
      } /* end inner for-loop */
   } /* end outer for-loop */
   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode_for_trim(Epeg_Image *im)
{
   int y;

   if (im->pixels) {
      return 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = 1;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_ISLOW;

   switch (im->color_space) {
      case EPEG_GRAY8:
		   im->in.jinfo.out_color_space = JCS_GRAYSCALE;
		   im->in.jinfo.output_components = 1;
		   break;

      case EPEG_YUV8:
		   im->in.jinfo.out_color_space = JCS_YCbCr;
		   break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
		   im->in.jinfo.out_color_space = JCS_RGB;
		   break;

      case EPEG_CMYK:
		   im->in.jinfo.out_color_space = JCS_CMYK;
		   im->in.jinfo.output_components = 4;
		   break;

      default:
		   break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
      im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
      jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_trim(Epeg_Image *im)
{
   int y, a, b, w, h;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   a = im->out.x;
   b = im->out.y;

   /* dummy condition to use value stored to 'w': */
   if (w == 0) {
	   ;
   }

   for ((y = 0); (y < h); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)((y + b) * im->in.jinfo.output_components) * im->in.jinfo.output_width)
                      + (a * im->in.jinfo.output_components));
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_encode(Epeg_Image *im)
{
   void  *data = NULL;
   size_t size = 0;

   if (im->out.f) {
      return 1;
   }

   if (im->out.file) {
      im->out.f = fopen(im->out.file, "wb");
   } else {
      im->out.f = _epeg_memfile_write_open(&data, &size);
   }
   if (!im->out.f) {
      im->error = 1;
      return 1;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_create_compress(&(im->out.jinfo));
   jpeg_stdio_dest(&(im->out.jinfo), im->out.f);
   im->out.jinfo.image_width = (JDIMENSION)im->out.w;
   im->out.jinfo.image_height = (JDIMENSION)im->out.h;
   im->out.jinfo.input_components = im->in.jinfo.output_components;
   im->out.jinfo.in_color_space = im->in.jinfo.out_color_space;
   im->out.jinfo.dct_method = JDCT_IFAST;
   im->out.jinfo.dct_method = im->in.jinfo.dct_method;
   jpeg_set_defaults(&(im->out.jinfo));
   jpeg_set_quality(&(im->out.jinfo), im->out.quality, TRUE);

   if (im->out.quality >= 90) {
      im->out.jinfo.comp_info[0].h_samp_factor = 1;
      im->out.jinfo.comp_info[0].v_samp_factor = 1;
      im->out.jinfo.comp_info[1].h_samp_factor = 1;
      im->out.jinfo.comp_info[1].v_samp_factor = 1;
      im->out.jinfo.comp_info[2].h_samp_factor = 1;
      im->out.jinfo.comp_info[2].v_samp_factor = 1;
   }
   jpeg_start_compress(&(im->out.jinfo), TRUE);

   if (im->out.comment) {
      jpeg_write_marker(&(im->out.jinfo), JPEG_COM,
                        (const JOCTET *)im->out.comment,
                        (unsigned int)strlen(im->out.comment));
   }

   if (im->out.thumbnail_info) {
      char buf[8192];

      if (im->in.file) {
         snprintf(buf, sizeof(buf), "Thumb::URI\nfile://%s", im->in.file);
         jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7),
                           (const JOCTET *)buf, (unsigned int)strlen(buf));
         snprintf(buf, sizeof(buf), "Thumb::MTime\n%llu",
#if defined(HAVE_UINTMAX_T) && !defined(__LP64__)
                  (uintmax_t)im->stat_info.st_mtime);
#else
               (unsigned long long int)im->stat_info.st_mtime);
#endif /* HAVE_UINTMAX_T && !__LP64__ */
      }
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Width\n%i", im->in.w);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Height\n%i", im->in.h);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Mimetype\nimage/jpeg");
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
   }

   while (im->out.jinfo.next_scanline < im->out.h) {
      jpeg_write_scanlines(&(im->out.jinfo),
                           &(im->lines[im->out.jinfo.next_scanline]), 1);
   }

   jpeg_finish_compress(&(im->out.jinfo));

   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if ((im->in.f) && (im->in.file)) {
      fclose(im->in.f);
   }
   if ((im->in.f) && (!im->in.file)) {
      _epeg_memfile_read_close(im->in.f);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if ((im->out.f) && (im->out.file)) {
      fclose(im->out.f);
   }
   if ((im->out.f) && (!im->out.file)) {
      _epeg_memfile_write_close(im->out.f);
   }
   im->in.f = NULL;
   im->out.f = NULL;

   if (im->out.mem.data) {
	   *(im->out.mem.data) = (unsigned char *)data;
   }
   if (im->out.mem.size) {
	   *(im->out.mem.size) = (int)size;
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static void _epeg_fatal_error_handler(j_common_ptr cinfo)
{
   emptr errmgr;

   errmgr = (emptr)cinfo->err;
   longjmp(errmgr->setjmp_buffer, 1);
   return;
}

/* silence '-Wunused-macros' warnings: */
#ifdef MAX
# undef MAX
#endif /* MAX */

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_memfile.c': */
/* epeg_memfile.c */
/* gets built into the libepeg library */

#include <stdio.h>
#include "Epeg.h"
#include "epeg_private.h"

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_read_open(void *data, size_t size)
{
#ifdef HAVE_FMEMOPEN
   return (FILE *)fmemopen(data, size, "r");
#else
   FILE *f;

   f = tmpfile();
   if (!f) {
      return NULL;
   }
   fwrite(data, size, (size_t)1L, f);
   rewind(f);
   return f;
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_read_close(FILE *f)
{
#ifdef HAVE_FMEMOPEN
   fclose(f);
#else
   /* FIXME: make these different halves of the conditional actually different
    * from one another */
   fclose(f);
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only struct and typedef; unnecessary to document: */
typedef struct _Eet_Memfile_Write_Info Eet_Memfile_Write_Info;
struct _Eet_Memfile_Write_Info
{
   FILE *f;
   void **data;
   size_t *size;
};

/* static internal private-only global variables; unnecessary to document: */
static int _epeg_memfile_info_alloc_num = 0;
static int _epeg_memfile_info_num = 0;
static Eet_Memfile_Write_Info *_epeg_memfile_info = NULL;

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_write_open(void **data, size_t *size)
{
#ifdef HAVE_OPEN_MEMSTREAM
   return open_memstream((char **)data, size);
#else
   FILE *f;

   _epeg_memfile_info_num++;
   if (_epeg_memfile_info_num > _epeg_memfile_info_alloc_num) {
      Eet_Memfile_Write_Info *tmp;

      _epeg_memfile_info_alloc_num += 16;
      tmp = (Eet_Memfile_Write_Info *)realloc(_epeg_memfile_info,
                                              ((unsigned long)_epeg_memfile_info_alloc_num *
                                               sizeof(Eet_Memfile_Write_Info)));
      if (!tmp) {
         _epeg_memfile_info_alloc_num -= 16;
         _epeg_memfile_info_num--;
         return NULL;
      }
      _epeg_memfile_info = tmp;
   }
   f = tmpfile();
   if (!f) {
      _epeg_memfile_info_num--;
      return NULL;
   }
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].f = f;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].data = data;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].size = size;
   return f;
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_write_close(FILE *f)
{
#ifdef HAVE_OPEN_MEMSTREAM
   fclose(f);
#else
   int i;

   for ((i = 0); (i < _epeg_memfile_info_num); i++) {
      if (_epeg_memfile_info[i].f == f) {
         int j;

         fseek(f, 0L, SEEK_END);
         (*(_epeg_memfile_info[i].size)) = (size_t)ftell(f);
         rewind(f);
         (*(_epeg_memfile_info[i].data)) = malloc(*(_epeg_memfile_info[i].size));
         if (!(*(_epeg_memfile_info[i].data))) {
            fclose(f);
            (*(_epeg_memfile_info[i].size)) = 0;
            return;
         }
         fread((*(_epeg_memfile_info[i].data)), (*(_epeg_memfile_info[i].size)),
               (size_t)1L, f);
         for ((j = (i + 1)); (j < _epeg_memfile_info_num); j++) {
            _epeg_memfile_info[(j - 1)] = _epeg_memfile_info[j];
         }
         _epeg_memfile_info_num--;
         fclose(f);
         return;
      } /* end "if" */
   } /* end for-loop */
   fclose(f);
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_main.c': */
/* epeg_main.c for src/lib for epeg */
/* gets built into the libepeg library */

#include <stdio.h>
#include <stdlib.h>
#include "Epeg.h"
#include "epeg_private.h"

static Epeg_Image*_epeg_open_header(Epeg_Image *im);
static int _epeg_decode(Epeg_Image *im);
static int _epeg_scale(Epeg_Image *im);
static int _epeg_decode_for_trim(Epeg_Image *im);
static int _epeg_trim(Epeg_Image *im);
static int _epeg_encode(Epeg_Image *im);

static void _epeg_fatal_error_handler(j_common_ptr cinfo);

#ifndef MIN
# define MIN(__x,__y) ((__x) < (__y) ? (__x) : (__y))
#endif /* !MIN */
#ifndef MAX
# define MAX(__x,__y) ((__x) > (__y) ? (__x) : (__y))
#endif /* !MAX */

/**
 * Open a JPEG image by filename.
 * @param file The file path to open.
 * @return A handle to the opened JPEG file, with the header decoded.
 *
 * This function opens the file indicated by the @p file parameter, and
 * attempts to decode it as a jpeg file. If this failes, NULL is returned.
 * Otherwise a valid handle to an open JPEG file is returned that can be used
 * by other Epeg calls.
 *
 * The @p file must be a pointer to a valid C string, NUL (0 byte) terminated
 * thats is a relative or absolute file path. If not results are not
 * determined.
 *
 * See also: epeg_memory_open(), epeg_close()
 */
extern Epeg_Image *epeg_file_open(const char *file)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.file = strdup(file);
   im->in.f = fopen(im->in.file, "rb");
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   fstat(fileno(im->in.f), &(im->stat_info));
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Open a JPEG image stored in memory.
 * @param data A pointer to the memory containing the JPEG data.
 * @param size The size of the memory segment containing the JPEG.
 * @return  A handle to the opened JPEG, with the header decoded.
 *
 * This function opens a JPEG file that is stored in memory pointed to by
 * @p data, and that is @p size bytes in size. If successful a valid handle
 * is returned, or on failure NULL is returned.
 *
 * See also: epeg_file_open(), epeg_close()
 */
extern Epeg_Image *epeg_memory_open(unsigned char *data, int size)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.f = _epeg_memfile_read_open(data, (size_t)size);
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Return the original JPEG pixel size.
 * @param im A handle to an opened Epeg image.
 * @param w A pointer to the width value in pixels to be filled in.
 * @param h A pointer to the height value in pixels to be filled in.
 * @return Nothing.
 *
 * Returns the image size in pixels (well not really).
 *
 * See also: epeg_colorspace_get()
 */
extern void epeg_size_get(Epeg_Image *im, int *w, int *h)
{
	if (w) {
		*w = im->in.w;
	}
	if (h) {
		*h = im->in.h;
	}
}

/**
 * Return the original JPEG pixel color space.
 * @param im A handle to an opened Epeg image.
 * @param space A pointer to the color space value to be filled in.
 * @return Nothing.
 *
 * Returns the image color space (not yet though).
 *
 * See also: epeg_size_get()
 */
extern void epeg_colorspace_get(Epeg_Image *im, int *space)
{
	if (space) {
		*space = im->color_space;
	}
}

/**
 * Set the size of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param w The width of the image to decode at, in pixels.
 * @param h The height of the image to decode at, in pixels.
 * @return Nothing.
 *
 * Sets the size at which to decode the JPEG image, giving an optimized load
 * that only decodes the pixels needed.
 *
 * See also: epeg_decode_bounds_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_size_set(Epeg_Image *im, int w, int h)
{
   if (im->pixels) {
	   return;
   }
   if (w < 1) {
	   w = 1;
   } else if (w > im->in.w) {
	   w = im->in.w;
   }
   if (h < 1) {
	   h = 1;
   } else if (h > im->in.h) {
	   h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   im->out.x = 0;
   im->out.y = 0;
}

/**
 * Set the bounds of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param x Boundary X
 * @param y Boundary Y
 * @param w Boundary W
 * @param h Boundary H
 * @return Nothing.
 *
 * Sets the bounds inside which to decode the JPEG image,
 * giving an optimized load that only decodes the bounded pixels.
 * (???)
 *
 * See also: epeg_decode_size_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_bounds_set(Epeg_Image *im, int x, int y, int w, int h)
{
   if (im->pixels) {
      return;
   }
   if (w < 1) {
      w = 1;
   } else if (w > im->in.w) {
      w = im->in.w;
   }
   if (h < 1) {
      h = 1;
   } else if (h > im->in.h) {
      h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   if (x < 0) {
      x = 0;
   }
   if (y < 0) {
      y = 0;
   }
   im->out.x = x;
   im->out.y = y;
}

/**
 * Set the colorspace in which to decode the image.
 * @param im A handle to an opened Epeg image.
 * @param colorspace The colorspace to decode the image in.
 * @return Nothing.
 *
 * This sets the colorspace to decode the image in. The default is EPEG_YUV8,
 * as this is normally the native colorspace of a JPEG file, avoiding any
 * colorspace conversions for a faster load and/or save.
 *
 * See also: epeg_decode_size_set(), epeg_decode_bounds_set()
 */
extern void epeg_decode_colorspace_set(Epeg_Image *im,
                                       Epeg_Colorspace colorspace)
{
	if (im->pixels) {
		return;
	}
	if ((colorspace < EPEG_GRAY8) || (colorspace > EPEG_ARGB32)) {
		return;
	}
	im->color_space = colorspace;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get_as_RGB8()
 */
extern const void *epeg_pixels_get(Epeg_Image *im, int x, int y,  int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
         return NULL;
      }
   }

   if (!im->pixels) {
      return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* go through all 8 values in type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace): */
   if (im->color_space == EPEG_GRAY8) {
      /* this whole block is pretty much the same thing we do for each
       * of the other enumeration values as well... could probably be simplified
       * with a function-like macro or some other sort of meta-programming: */
      unsigned char *pix, *p;

      /* '1': */
      pix = (unsigned char *)malloc((size_t)(w * h * 1L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = s[0];
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_YUV8) {
      unsigned char *pix, *p;

      /* '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGR8) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* reverse order: */
            p[0] = s[2];
            p[1] = s[1];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGBA8) {
      unsigned char *pix, *p;

      /* '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGRA8) {
      unsigned char *pix, *p;

      /* also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = 0xff;
            /* reverse order: */
            p[1] = s[2];
            p[2] = s[1];
            p[3] = s[0];
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_ARGB32) {
      unsigned int *pix, *p;

      /* still also '4': */
      pix = (unsigned int *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (0xff000000 | (unsigned int)(s[0] << 16) | (unsigned int)(s[1] << 8) | (s[2]));
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* another thing that is also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* back to same order again: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } /* end "if-else" that seems like it should really be a "switch" */
   return NULL;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get()
 */
extern const void *epeg_pixels_get_as_RGB8(Epeg_Image *im, int x, int y,
                                           int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
		   return NULL;
	   }
   }

   if (!im->pixels) {
	   return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* unlike the "non-'_as_RGB8'-version" of this function,
    * there are no 'else'-es separating the cases here. Also, we only go through
    * three of the enumeration values of type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace) here: */
   if (im->color_space == EPEG_GRAY8) {
      unsigned char *pix, *p;

      /* get right into the '3' ones: */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[0];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (unsigned char)(MIN(255, (s[0] * s[3]) / 255));
            p[1] = (unsigned char)(MIN(255, (s[1] * s[3]) / 255));
            p[2] = (unsigned char)(MIN(255, (s[2] * s[3]) / 255));
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   return NULL;
}

/**
 * Free requested pixel block from an image.
 * @param im A handle to an opened Epeg image (unused).
 * @param data The pointer to the image pixels.
 * @return Nothing.
 *
 * This frees the data for a block of pixels requested from image @p im.
 * @p data must be a valid (non NULL) pointer to a pixel block taken from the
 * image @p im by epeg_pixels_get() and must be called before the image is
 * closed by epeg_close().
 */
extern void epeg_pixels_free(Epeg_Image *im, const void *data)
{
#pragma unused (im)
   free((void *)data);
}

/**
 * Get the image comment field as a string.
 * @param im A handle to an opened Epeg image.
 * @return A pointer to the loaded image comments.
 *
 * This function returns the comment field as a string (NUL byte terminated)
 * of the loaded image @p im, if there is a comment, or NULL if no comment is
 * saved with the image. Consider the string returned to be read-only.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern const char *epeg_comment_get(Epeg_Image *im)
{
   return im->in.comment;
}

/**
 * Get thumbnail comments of loaded image.
 * @param im A handle to an opened Epeg image.
 * @param info Pointer to a thumbnail info struct to be filled in.
 * @return Nothing.
 *
 * This function retrieves thumbnail comments written by Epeg to any saved
 * JPEG files. If no thumbnail comments were saved, the fields will be 0 in
 * the @p info struct on return.
 *
 * See also: epeg_comment_get(), epeg_thumbnail_comments_enable()
 */
extern void epeg_thumbnail_comments_get(Epeg_Image *im,
                                        Epeg_Thumbnail_Info *info)
{
   if (!info) {
      return;
   }
   info->uri = im->in.thumb_info.uri;
   info->mtime = im->in.thumb_info.mtime;
   info->w = im->in.thumb_info.w;
   info->h = im->in.thumb_info.h;
   info->mimetype = im->in.thumb_info.mime;
}

/**
 * Set the comment field of the image for saving.
 * @param im A handle to an opened Epeg image.
 * @param comment The comment to set.
 * @return Nothing.
 *
 * Set the comment for the image file for when it gets saved. This is a NUL
 * byte terminated C string. If @p comment is NULL the output file will have
 * no comment field.
 *
 * The default comment will be any comment loaded from the input file.
 *
 * See also: epeg_comment_get()
 */
extern void epeg_comment_set(Epeg_Image *im, const char *comment)
{
   if (im->out.comment) {
      free(im->out.comment);
   }
   if (!comment) {
      im->out.comment = NULL;
   } else {
      im->out.comment = strdup(comment);
   }
}

/**
 * Set the encoding quality of the saved image.
 * @param im A handle to an opened Epeg image.
 * @param quality The quality of encoding from 0 to 100.
 * @return Nothing.
 *
 * Set the quality of the output encoded image. Values from 0 to 100
 * inclusive are valid, with 100 being the maximum quality, and 0 being the
 * minimum. If the quality is set equal to or above 90%, the output U and V
 * color planes are encoded at 1:1 with the Y plane.
 *
 * The default quality is 75.
 *
 * See also: epeg_comment_set()
 */
extern void epeg_quality_set(Epeg_Image *im, int quality)
{
   if (quality < 0) {
      quality = 0;
   } else if (quality > 100) {
      quality = 100;
   }
   im->out.quality = quality;
}

/**
 * Enable thumbnail comments in saved image.
 * @param im A handle to an opened Epeg image.
 * @param onoff A boolean on and off enabling flag.
 * @return Nothing.
 *
 * if @p onoff is 1, the output file will have thumbnail comments added to
 * it, and if it is 0, it will not. The default is 0.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern void epeg_thumbnail_comments_enable(Epeg_Image *im, int onoff)
{
   im->out.thumbnail_info = (char)onoff;
}

/**
 * Set the output file path for the image when saved.
 * @param im A handle to an opened Epeg image.
 * @param file The path to the output file.
 * @return Nothing.
 *
 * This sets the output file path name (either a full or relative path name)
 * to where the file will be written when saved. @p file must be a NUL
 * terminated C string containing the path to the file to be saved to. If it is
 * NULL, then the image will not be saved to a file when calling epeg_encode().
 *
 * See also: epeg_memory_output_set(), epeg_encode()
 */
extern void epeg_file_output_set(Epeg_Image *im, const char *file)
{
   if (im->out.file) {
      free(im->out.file);
   }
   if (!file) {
      im->out.file = NULL;
   } else {
      im->out.file = strdup(file);
   }
}

/**
 * Set the output file to be a block of allocated memory.
 * @param im A handle to an opened Epeg image.
 * @param data A pointer to a pointer to a memory block.
 * @param size A pointer to a counter of the size of the memory block.
 * @return Nothing.
 *
 * This sets the output encoding of the image when saved to be allocated
 * memory. After epeg_close() is called the pointer pointed to by @p data
 * and the integer pointed to by @p size will contain the pointer to the
 * memory block and its size in bytes, respecitvely. The memory block can be
 * freed with the free() function call. If the save fails the pointer to the
 * memory block will be unaffected, as will the size.
 *
 * See also: epeg_file_output_set(), epeg_encode()
 */
extern void epeg_memory_output_set(Epeg_Image *im, unsigned char **data,
                                   int *size)
{
   im->out.mem.data = data;
   im->out.mem.size = size;
}

/**
 * This saves the image to its specified destination.
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * This saves the image @p im to its destination specified by
 * epeg_file_output_set() or epeg_memory_output_set(). The image will be
 * encoded at the decoded pixel size, using the quality, comment,
 * and thumbnail comment settings set on the image.
 *
 * See also: epeg_file_output_set(), epeg_memory_output_set()
 */
extern int epeg_encode(Epeg_Image *im)
{
   if (_epeg_decode(im) != 0) {
      return 1;
   }
   if (_epeg_scale(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * FIXME: Document this with a short, sentence-long description of epeg_trim()
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * FIXME: Document this with a longer, paragraph-long description.
 */
extern int epeg_trim(Epeg_Image *im)
{
   if (_epeg_decode_for_trim(im) != 0) {
      return 1;
   }
   if (_epeg_trim(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * Close an image handle.
 * @param im A handle to an opened Epeg image.
 * @return Nothing.
 *
 * This closes an opened image handle and frees all memory associated with it.
 * It does NOT free encoded data generated by epeg_memory_output_set() followed
 * by epeg_encode(), nor does it guarantee to free any data received by
 * epeg_pixels_get(). Once an image handle is closed, consider it invalid.
 *
 * See also: epeg_file_open(), epeg_memory_open()
 */
extern void epeg_close(Epeg_Image *im)
{
   if (im->pixels) {
      free(im->pixels);
   }
   if (im->lines) {
      free(im->lines);
   }
   if (im->in.file) {
      free(im->in.file);
   }
   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if (im->in.f) {
      fclose(im->in.f);
   }
   if (im->in.comment) {
      free(im->in.comment);
   }
   if (im->in.thumb_info.uri) {
      free(im->in.thumb_info.uri);
   }
   if (im->in.thumb_info.mime) {
      free(im->in.thumb_info.mime);
   }
   if (im->out.file) {
      free(im->out.file);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if (im->out.f) {
      fclose(im->out.f);
   }
   if (im->out.comment) {
      free(im->out.comment);
   }
   free(im);
}

/* static internal private-only function; unnecessary to document: */
static Epeg_Image *_epeg_open_header(Epeg_Image *im)
{
   struct jpeg_marker_struct *m;

   im->in.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      error:
      epeg_close(im);
      im = NULL;
      return NULL;
   }

   jpeg_create_decompress(&(im->in.jinfo));
   jpeg_save_markers(&(im->in.jinfo), (JPEG_APP0 + 7), 1024);
   jpeg_save_markers(&(im->in.jinfo), JPEG_COM, 65535);
   jpeg_stdio_src(&(im->in.jinfo), im->in.f);
   jpeg_read_header(&(im->in.jinfo), TRUE);
   im->in.w = (int)im->in.jinfo.image_width;
   im->in.h = (int)im->in.jinfo.image_height;
   if (im->in.w <= 1) {
      goto error;
   }
   if (im->in.h <= 1) {
      goto error;
   }

   im->out.w = im->in.w;
   im->out.h = im->in.h;

   im->color_space = (((im->in.color_space = im->in.jinfo.out_color_space) == JCS_GRAYSCALE) ? EPEG_GRAY8 : EPEG_RGB8);
   if (im->in.color_space == JCS_CMYK) {
      im->color_space = EPEG_CMYK;
   }

   for ((m = im->in.jinfo.marker_list); m; (m = m->next)) {
      if (m->marker == JPEG_COM) {
         if (im->in.comment) {
            free(im->in.comment);
         }
         im->in.comment = (char *)malloc((size_t)(m->data_length + 1));
         if (im->in.comment) {
            memcpy(im->in.comment, m->data, (size_t)m->data_length);
            im->in.comment[m->data_length] = 0;
         }
      } else if (m->marker == (JPEG_APP0 + 7))  {
         if ((m->data_length > 7) &&
             (!strncmp((char *)m->data, "Thumb::", (size_t)7))) {
            char *p, *p2;

            p = (char *)malloc((size_t)(m->data_length + 1));
            if (p) {
               memcpy(p, m->data, (size_t)m->data_length);
               p[m->data_length] = 0;
               p2 = strchr(p, '\n');
               if (p2) {
                  p2[0] = 0;
                  if (!strcmp(p, "Thumb::URI")) {
                     im->in.thumb_info.uri = strdup(p2 + 1);
                  } else if (!strcmp(p, "Thumb::MTime")) {
                     sscanf((p2 + 1), "%llu", &(im->in.thumb_info.mtime));
                  } else if (!strcmp(p, "Thumb::Image::Width")) {
                     im->in.thumb_info.w = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Image::Height")) {
                     im->in.thumb_info.h = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Mimetype")) {
                     im->in.thumb_info.mime = strdup(p2 + 1);
                  }
               } /* end "if (p2)" */
               free(p);
            } /* end "if (p)" */
         }
      }
   } /* end for-loop */
   return im;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode(Epeg_Image *im)
{
   int scale, scalew, scaleh, y;

   if (im->pixels) {
      return 1;
   }

   scalew = (im->in.w / im->out.w);
   scaleh = (im->in.h / im->out.h);

   scale = scalew;
   if (scaleh < scalew) {
      scale = scaleh;
   }

   if (scale > 8) {
      scale = 8;
   } else if (scale < 1) {
      scale = 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = (unsigned int)scale;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_IFAST;

   switch (im->color_space) {
      case EPEG_GRAY8:
			 im->in.jinfo.out_color_space = JCS_GRAYSCALE;
			 im->in.jinfo.output_components = 1;
			 break;

      case EPEG_YUV8:
			 im->in.jinfo.out_color_space = JCS_YCbCr;
			 break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
			 im->in.jinfo.out_color_space = JCS_RGB;
			 break;

      case EPEG_CMYK:
			 im->in.jinfo.out_color_space = JCS_CMYK;
			 im->in.jinfo.output_components = 4;
			 break;

      default:
			 break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      epeg_close(im);
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
	   jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_scale(Epeg_Image *im)
{
   unsigned char *dst, *row, *src;
   int            x, y, w, h, i;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   for ((y = 0); (y < h); y++) {
      row = (im->pixels +
             ((((unsigned int)y * im->in.jinfo.output_height) / (unsigned int)h) * (unsigned int)im->in.jinfo.output_components * im->in.jinfo.output_width));
      dst = (im->pixels +
             ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));

      for ((x = 0); (x < im->out.w); x++) {
         src = (row +
                ((((unsigned int)x * im->in.jinfo.output_width) / (unsigned int)w) * (unsigned int)im->in.jinfo.output_components));

         for ((i = 0); (i < im->in.jinfo.output_components); i++) {
            dst[i] = src[i];
         } /* end inmost for-loop */

         dst += im->in.jinfo.output_components;
      } /* end inner for-loop */
   } /* end outer for-loop */
   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode_for_trim(Epeg_Image *im)
{
   int y;

   if (im->pixels) {
      return 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = 1;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_ISLOW;

   switch (im->color_space) {
      case EPEG_GRAY8:
		   im->in.jinfo.out_color_space = JCS_GRAYSCALE;
		   im->in.jinfo.output_components = 1;
		   break;

      case EPEG_YUV8:
		   im->in.jinfo.out_color_space = JCS_YCbCr;
		   break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
		   im->in.jinfo.out_color_space = JCS_RGB;
		   break;

      case EPEG_CMYK:
		   im->in.jinfo.out_color_space = JCS_CMYK;
		   im->in.jinfo.output_components = 4;
		   break;

      default:
		   break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
      im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
      jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_trim(Epeg_Image *im)
{
   int y, a, b, w, h;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   a = im->out.x;
   b = im->out.y;

   /* dummy condition to use value stored to 'w': */
   if (w == 0) {
	   ;
   }

   for ((y = 0); (y < h); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)((y + b) * im->in.jinfo.output_components) * im->in.jinfo.output_width)
                      + (a * im->in.jinfo.output_components));
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_encode(Epeg_Image *im)
{
   void  *data = NULL;
   size_t size = 0;

   if (im->out.f) {
      return 1;
   }

   if (im->out.file) {
      im->out.f = fopen(im->out.file, "wb");
   } else {
      im->out.f = _epeg_memfile_write_open(&data, &size);
   }
   if (!im->out.f) {
      im->error = 1;
      return 1;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_create_compress(&(im->out.jinfo));
   jpeg_stdio_dest(&(im->out.jinfo), im->out.f);
   im->out.jinfo.image_width = (JDIMENSION)im->out.w;
   im->out.jinfo.image_height = (JDIMENSION)im->out.h;
   im->out.jinfo.input_components = im->in.jinfo.output_components;
   im->out.jinfo.in_color_space = im->in.jinfo.out_color_space;
   im->out.jinfo.dct_method = JDCT_IFAST;
   im->out.jinfo.dct_method = im->in.jinfo.dct_method;
   jpeg_set_defaults(&(im->out.jinfo));
   jpeg_set_quality(&(im->out.jinfo), im->out.quality, TRUE);

   if (im->out.quality >= 90) {
      im->out.jinfo.comp_info[0].h_samp_factor = 1;
      im->out.jinfo.comp_info[0].v_samp_factor = 1;
      im->out.jinfo.comp_info[1].h_samp_factor = 1;
      im->out.jinfo.comp_info[1].v_samp_factor = 1;
      im->out.jinfo.comp_info[2].h_samp_factor = 1;
      im->out.jinfo.comp_info[2].v_samp_factor = 1;
   }
   jpeg_start_compress(&(im->out.jinfo), TRUE);

   if (im->out.comment) {
      jpeg_write_marker(&(im->out.jinfo), JPEG_COM,
                        (const JOCTET *)im->out.comment,
                        (unsigned int)strlen(im->out.comment));
   }

   if (im->out.thumbnail_info) {
      char buf[8192];

      if (im->in.file) {
         snprintf(buf, sizeof(buf), "Thumb::URI\nfile://%s", im->in.file);
         jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7),
                           (const JOCTET *)buf, (unsigned int)strlen(buf));
         snprintf(buf, sizeof(buf), "Thumb::MTime\n%llu",
#if defined(HAVE_UINTMAX_T) && !defined(__LP64__)
                  (uintmax_t)im->stat_info.st_mtime);
#else
               (unsigned long long int)im->stat_info.st_mtime);
#endif /* HAVE_UINTMAX_T && !__LP64__ */
      }
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Width\n%i", im->in.w);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Height\n%i", im->in.h);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Mimetype\nimage/jpeg");
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
   }

   while (im->out.jinfo.next_scanline < im->out.h) {
      jpeg_write_scanlines(&(im->out.jinfo),
                           &(im->lines[im->out.jinfo.next_scanline]), 1);
   }

   jpeg_finish_compress(&(im->out.jinfo));

   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if ((im->in.f) && (im->in.file)) {
      fclose(im->in.f);
   }
   if ((im->in.f) && (!im->in.file)) {
      _epeg_memfile_read_close(im->in.f);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if ((im->out.f) && (im->out.file)) {
      fclose(im->out.f);
   }
   if ((im->out.f) && (!im->out.file)) {
      _epeg_memfile_write_close(im->out.f);
   }
   im->in.f = NULL;
   im->out.f = NULL;

   if (im->out.mem.data) {
	   *(im->out.mem.data) = (unsigned char *)data;
   }
   if (im->out.mem.size) {
	   *(im->out.mem.size) = (int)size;
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static void _epeg_fatal_error_handler(j_common_ptr cinfo)
{
   emptr errmgr;

   errmgr = (emptr)cinfo->err;
   longjmp(errmgr->setjmp_buffer, 1);
   return;
}

/* silence '-Wunused-macros' warnings: */
#ifdef MAX
# undef MAX
#endif /* MAX */

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_memfile.c': */
/* epeg_memfile.c */
/* gets built into the libepeg library */

#include <stdio.h>
#include "Epeg.h"
#include "epeg_private.h"

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_read_open(void *data, size_t size)
{
#ifdef HAVE_FMEMOPEN
   return (FILE *)fmemopen(data, size, "r");
#else
   FILE *f;

   f = tmpfile();
   if (!f) {
      return NULL;
   }
   fwrite(data, size, (size_t)1L, f);
   rewind(f);
   return f;
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_read_close(FILE *f)
{
#ifdef HAVE_FMEMOPEN
   fclose(f);
#else
   /* FIXME: make these different halves of the conditional actually different
    * from one another */
   fclose(f);
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only struct and typedef; unnecessary to document: */
typedef struct _Eet_Memfile_Write_Info Eet_Memfile_Write_Info;
struct _Eet_Memfile_Write_Info
{
   FILE *f;
   void **data;
   size_t *size;
};

/* static internal private-only global variables; unnecessary to document: */
static int _epeg_memfile_info_alloc_num = 0;
static int _epeg_memfile_info_num = 0;
static Eet_Memfile_Write_Info *_epeg_memfile_info = NULL;

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_write_open(void **data, size_t *size)
{
#ifdef HAVE_OPEN_MEMSTREAM
   return open_memstream((char **)data, size);
#else
   FILE *f;

   _epeg_memfile_info_num++;
   if (_epeg_memfile_info_num > _epeg_memfile_info_alloc_num) {
      Eet_Memfile_Write_Info *tmp;

      _epeg_memfile_info_alloc_num += 16;
      tmp = (Eet_Memfile_Write_Info *)realloc(_epeg_memfile_info,
                                              ((unsigned long)_epeg_memfile_info_alloc_num *
                                               sizeof(Eet_Memfile_Write_Info)));
      if (!tmp) {
         _epeg_memfile_info_alloc_num -= 16;
         _epeg_memfile_info_num--;
         return NULL;
      }
      _epeg_memfile_info = tmp;
   }
   f = tmpfile();
   if (!f) {
      _epeg_memfile_info_num--;
      return NULL;
   }
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].f = f;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].data = data;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].size = size;
   return f;
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_write_close(FILE *f)
{
#ifdef HAVE_OPEN_MEMSTREAM
   fclose(f);
#else
   int i;

   for ((i = 0); (i < _epeg_memfile_info_num); i++) {
      if (_epeg_memfile_info[i].f == f) {
         int j;

         fseek(f, 0L, SEEK_END);
         (*(_epeg_memfile_info[i].size)) = (size_t)ftell(f);
         rewind(f);
         (*(_epeg_memfile_info[i].data)) = malloc(*(_epeg_memfile_info[i].size));
         if (!(*(_epeg_memfile_info[i].data))) {
            fclose(f);
            (*(_epeg_memfile_info[i].size)) = 0;
            return;
         }
         fread((*(_epeg_memfile_info[i].data)), (*(_epeg_memfile_info[i].size)),
               (size_t)1L, f);
         for ((j = (i + 1)); (j < _epeg_memfile_info_num); j++) {
            _epeg_memfile_info[(j - 1)] = _epeg_memfile_info[j];
         }
         _epeg_memfile_info_num--;
         fclose(f);
         return;
      } /* end "if" */
   } /* end for-loop */
   fclose(f);
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_main.c': */
/* epeg_main.c for src/lib for epeg */
/* gets built into the libepeg library */

#include <stdio.h>
#include <stdlib.h>
#include "Epeg.h"
#include "epeg_private.h"

static Epeg_Image*_epeg_open_header(Epeg_Image *im);
static int _epeg_decode(Epeg_Image *im);
static int _epeg_scale(Epeg_Image *im);
static int _epeg_decode_for_trim(Epeg_Image *im);
static int _epeg_trim(Epeg_Image *im);
static int _epeg_encode(Epeg_Image *im);

static void _epeg_fatal_error_handler(j_common_ptr cinfo);

#ifndef MIN
# define MIN(__x,__y) ((__x) < (__y) ? (__x) : (__y))
#endif /* !MIN */
#ifndef MAX
# define MAX(__x,__y) ((__x) > (__y) ? (__x) : (__y))
#endif /* !MAX */

/**
 * Open a JPEG image by filename.
 * @param file The file path to open.
 * @return A handle to the opened JPEG file, with the header decoded.
 *
 * This function opens the file indicated by the @p file parameter, and
 * attempts to decode it as a jpeg file. If this failes, NULL is returned.
 * Otherwise a valid handle to an open JPEG file is returned that can be used
 * by other Epeg calls.
 *
 * The @p file must be a pointer to a valid C string, NUL (0 byte) terminated
 * thats is a relative or absolute file path. If not results are not
 * determined.
 *
 * See also: epeg_memory_open(), epeg_close()
 */
extern Epeg_Image *epeg_file_open(const char *file)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.file = strdup(file);
   im->in.f = fopen(im->in.file, "rb");
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   fstat(fileno(im->in.f), &(im->stat_info));
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Open a JPEG image stored in memory.
 * @param data A pointer to the memory containing the JPEG data.
 * @param size The size of the memory segment containing the JPEG.
 * @return  A handle to the opened JPEG, with the header decoded.
 *
 * This function opens a JPEG file that is stored in memory pointed to by
 * @p data, and that is @p size bytes in size. If successful a valid handle
 * is returned, or on failure NULL is returned.
 *
 * See also: epeg_file_open(), epeg_close()
 */
extern Epeg_Image *epeg_memory_open(unsigned char *data, int size)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.f = _epeg_memfile_read_open(data, (size_t)size);
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Return the original JPEG pixel size.
 * @param im A handle to an opened Epeg image.
 * @param w A pointer to the width value in pixels to be filled in.
 * @param h A pointer to the height value in pixels to be filled in.
 * @return Nothing.
 *
 * Returns the image size in pixels (well not really).
 *
 * See also: epeg_colorspace_get()
 */
extern void epeg_size_get(Epeg_Image *im, int *w, int *h)
{
	if (w) {
		*w = im->in.w;
	}
	if (h) {
		*h = im->in.h;
	}
}

/**
 * Return the original JPEG pixel color space.
 * @param im A handle to an opened Epeg image.
 * @param space A pointer to the color space value to be filled in.
 * @return Nothing.
 *
 * Returns the image color space (not yet though).
 *
 * See also: epeg_size_get()
 */
extern void epeg_colorspace_get(Epeg_Image *im, int *space)
{
	if (space) {
		*space = im->color_space;
	}
}

/**
 * Set the size of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param w The width of the image to decode at, in pixels.
 * @param h The height of the image to decode at, in pixels.
 * @return Nothing.
 *
 * Sets the size at which to decode the JPEG image, giving an optimized load
 * that only decodes the pixels needed.
 *
 * See also: epeg_decode_bounds_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_size_set(Epeg_Image *im, int w, int h)
{
   if (im->pixels) {
	   return;
   }
   if (w < 1) {
	   w = 1;
   } else if (w > im->in.w) {
	   w = im->in.w;
   }
   if (h < 1) {
	   h = 1;
   } else if (h > im->in.h) {
	   h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   im->out.x = 0;
   im->out.y = 0;
}

/**
 * Set the bounds of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param x Boundary X
 * @param y Boundary Y
 * @param w Boundary W
 * @param h Boundary H
 * @return Nothing.
 *
 * Sets the bounds inside which to decode the JPEG image,
 * giving an optimized load that only decodes the bounded pixels.
 * (???)
 *
 * See also: epeg_decode_size_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_bounds_set(Epeg_Image *im, int x, int y, int w, int h)
{
   if (im->pixels) {
      return;
   }
   if (w < 1) {
      w = 1;
   } else if (w > im->in.w) {
      w = im->in.w;
   }
   if (h < 1) {
      h = 1;
   } else if (h > im->in.h) {
      h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   if (x < 0) {
      x = 0;
   }
   if (y < 0) {
      y = 0;
   }
   im->out.x = x;
   im->out.y = y;
}

/**
 * Set the colorspace in which to decode the image.
 * @param im A handle to an opened Epeg image.
 * @param colorspace The colorspace to decode the image in.
 * @return Nothing.
 *
 * This sets the colorspace to decode the image in. The default is EPEG_YUV8,
 * as this is normally the native colorspace of a JPEG file, avoiding any
 * colorspace conversions for a faster load and/or save.
 *
 * See also: epeg_decode_size_set(), epeg_decode_bounds_set()
 */
extern void epeg_decode_colorspace_set(Epeg_Image *im,
                                       Epeg_Colorspace colorspace)
{
	if (im->pixels) {
		return;
	}
	if ((colorspace < EPEG_GRAY8) || (colorspace > EPEG_ARGB32)) {
		return;
	}
	im->color_space = colorspace;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get_as_RGB8()
 */
extern const void *epeg_pixels_get(Epeg_Image *im, int x, int y,  int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
         return NULL;
      }
   }

   if (!im->pixels) {
      return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* go through all 8 values in type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace): */
   if (im->color_space == EPEG_GRAY8) {
      /* this whole block is pretty much the same thing we do for each
       * of the other enumeration values as well... could probably be simplified
       * with a function-like macro or some other sort of meta-programming: */
      unsigned char *pix, *p;

      /* '1': */
      pix = (unsigned char *)malloc((size_t)(w * h * 1L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = s[0];
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_YUV8) {
      unsigned char *pix, *p;

      /* '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGR8) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* reverse order: */
            p[0] = s[2];
            p[1] = s[1];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGBA8) {
      unsigned char *pix, *p;

      /* '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGRA8) {
      unsigned char *pix, *p;

      /* also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = 0xff;
            /* reverse order: */
            p[1] = s[2];
            p[2] = s[1];
            p[3] = s[0];
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_ARGB32) {
      unsigned int *pix, *p;

      /* still also '4': */
      pix = (unsigned int *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (0xff000000 | (unsigned int)(s[0] << 16) | (unsigned int)(s[1] << 8) | (s[2]));
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* another thing that is also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* back to same order again: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } /* end "if-else" that seems like it should really be a "switch" */
   return NULL;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get()
 */
extern const void *epeg_pixels_get_as_RGB8(Epeg_Image *im, int x, int y,
                                           int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
		   return NULL;
	   }
   }

   if (!im->pixels) {
	   return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* unlike the "non-'_as_RGB8'-version" of this function,
    * there are no 'else'-es separating the cases here. Also, we only go through
    * three of the enumeration values of type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace) here: */
   if (im->color_space == EPEG_GRAY8) {
      unsigned char *pix, *p;

      /* get right into the '3' ones: */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[0];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (unsigned char)(MIN(255, (s[0] * s[3]) / 255));
            p[1] = (unsigned char)(MIN(255, (s[1] * s[3]) / 255));
            p[2] = (unsigned char)(MIN(255, (s[2] * s[3]) / 255));
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   return NULL;
}

/**
 * Free requested pixel block from an image.
 * @param im A handle to an opened Epeg image (unused).
 * @param data The pointer to the image pixels.
 * @return Nothing.
 *
 * This frees the data for a block of pixels requested from image @p im.
 * @p data must be a valid (non NULL) pointer to a pixel block taken from the
 * image @p im by epeg_pixels_get() and must be called before the image is
 * closed by epeg_close().
 */
extern void epeg_pixels_free(Epeg_Image *im, const void *data)
{
#pragma unused (im)
   free((void *)data);
}

/**
 * Get the image comment field as a string.
 * @param im A handle to an opened Epeg image.
 * @return A pointer to the loaded image comments.
 *
 * This function returns the comment field as a string (NUL byte terminated)
 * of the loaded image @p im, if there is a comment, or NULL if no comment is
 * saved with the image. Consider the string returned to be read-only.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern const char *epeg_comment_get(Epeg_Image *im)
{
   return im->in.comment;
}

/**
 * Get thumbnail comments of loaded image.
 * @param im A handle to an opened Epeg image.
 * @param info Pointer to a thumbnail info struct to be filled in.
 * @return Nothing.
 *
 * This function retrieves thumbnail comments written by Epeg to any saved
 * JPEG files. If no thumbnail comments were saved, the fields will be 0 in
 * the @p info struct on return.
 *
 * See also: epeg_comment_get(), epeg_thumbnail_comments_enable()
 */
extern void epeg_thumbnail_comments_get(Epeg_Image *im,
                                        Epeg_Thumbnail_Info *info)
{
   if (!info) {
      return;
   }
   info->uri = im->in.thumb_info.uri;
   info->mtime = im->in.thumb_info.mtime;
   info->w = im->in.thumb_info.w;
   info->h = im->in.thumb_info.h;
   info->mimetype = im->in.thumb_info.mime;
}

/**
 * Set the comment field of the image for saving.
 * @param im A handle to an opened Epeg image.
 * @param comment The comment to set.
 * @return Nothing.
 *
 * Set the comment for the image file for when it gets saved. This is a NUL
 * byte terminated C string. If @p comment is NULL the output file will have
 * no comment field.
 *
 * The default comment will be any comment loaded from the input file.
 *
 * See also: epeg_comment_get()
 */
extern void epeg_comment_set(Epeg_Image *im, const char *comment)
{
   if (im->out.comment) {
      free(im->out.comment);
   }
   if (!comment) {
      im->out.comment = NULL;
   } else {
      im->out.comment = strdup(comment);
   }
}

/**
 * Set the encoding quality of the saved image.
 * @param im A handle to an opened Epeg image.
 * @param quality The quality of encoding from 0 to 100.
 * @return Nothing.
 *
 * Set the quality of the output encoded image. Values from 0 to 100
 * inclusive are valid, with 100 being the maximum quality, and 0 being the
 * minimum. If the quality is set equal to or above 90%, the output U and V
 * color planes are encoded at 1:1 with the Y plane.
 *
 * The default quality is 75.
 *
 * See also: epeg_comment_set()
 */
extern void epeg_quality_set(Epeg_Image *im, int quality)
{
   if (quality < 0) {
      quality = 0;
   } else if (quality > 100) {
      quality = 100;
   }
   im->out.quality = quality;
}

/**
 * Enable thumbnail comments in saved image.
 * @param im A handle to an opened Epeg image.
 * @param onoff A boolean on and off enabling flag.
 * @return Nothing.
 *
 * if @p onoff is 1, the output file will have thumbnail comments added to
 * it, and if it is 0, it will not. The default is 0.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern void epeg_thumbnail_comments_enable(Epeg_Image *im, int onoff)
{
   im->out.thumbnail_info = (char)onoff;
}

/**
 * Set the output file path for the image when saved.
 * @param im A handle to an opened Epeg image.
 * @param file The path to the output file.
 * @return Nothing.
 *
 * This sets the output file path name (either a full or relative path name)
 * to where the file will be written when saved. @p file must be a NUL
 * terminated C string containing the path to the file to be saved to. If it is
 * NULL, then the image will not be saved to a file when calling epeg_encode().
 *
 * See also: epeg_memory_output_set(), epeg_encode()
 */
extern void epeg_file_output_set(Epeg_Image *im, const char *file)
{
   if (im->out.file) {
      free(im->out.file);
   }
   if (!file) {
      im->out.file = NULL;
   } else {
      im->out.file = strdup(file);
   }
}

/**
 * Set the output file to be a block of allocated memory.
 * @param im A handle to an opened Epeg image.
 * @param data A pointer to a pointer to a memory block.
 * @param size A pointer to a counter of the size of the memory block.
 * @return Nothing.
 *
 * This sets the output encoding of the image when saved to be allocated
 * memory. After epeg_close() is called the pointer pointed to by @p data
 * and the integer pointed to by @p size will contain the pointer to the
 * memory block and its size in bytes, respecitvely. The memory block can be
 * freed with the free() function call. If the save fails the pointer to the
 * memory block will be unaffected, as will the size.
 *
 * See also: epeg_file_output_set(), epeg_encode()
 */
extern void epeg_memory_output_set(Epeg_Image *im, unsigned char **data,
                                   int *size)
{
   im->out.mem.data = data;
   im->out.mem.size = size;
}

/**
 * This saves the image to its specified destination.
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * This saves the image @p im to its destination specified by
 * epeg_file_output_set() or epeg_memory_output_set(). The image will be
 * encoded at the decoded pixel size, using the quality, comment,
 * and thumbnail comment settings set on the image.
 *
 * See also: epeg_file_output_set(), epeg_memory_output_set()
 */
extern int epeg_encode(Epeg_Image *im)
{
   if (_epeg_decode(im) != 0) {
      return 1;
   }
   if (_epeg_scale(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * FIXME: Document this with a short, sentence-long description of epeg_trim()
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * FIXME: Document this with a longer, paragraph-long description.
 */
extern int epeg_trim(Epeg_Image *im)
{
   if (_epeg_decode_for_trim(im) != 0) {
      return 1;
   }
   if (_epeg_trim(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * Close an image handle.
 * @param im A handle to an opened Epeg image.
 * @return Nothing.
 *
 * This closes an opened image handle and frees all memory associated with it.
 * It does NOT free encoded data generated by epeg_memory_output_set() followed
 * by epeg_encode(), nor does it guarantee to free any data received by
 * epeg_pixels_get(). Once an image handle is closed, consider it invalid.
 *
 * See also: epeg_file_open(), epeg_memory_open()
 */
extern void epeg_close(Epeg_Image *im)
{
   if (im->pixels) {
      free(im->pixels);
   }
   if (im->lines) {
      free(im->lines);
   }
   if (im->in.file) {
      free(im->in.file);
   }
   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if (im->in.f) {
      fclose(im->in.f);
   }
   if (im->in.comment) {
      free(im->in.comment);
   }
   if (im->in.thumb_info.uri) {
      free(im->in.thumb_info.uri);
   }
   if (im->in.thumb_info.mime) {
      free(im->in.thumb_info.mime);
   }
   if (im->out.file) {
      free(im->out.file);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if (im->out.f) {
      fclose(im->out.f);
   }
   if (im->out.comment) {
      free(im->out.comment);
   }
   free(im);
}

/* static internal private-only function; unnecessary to document: */
static Epeg_Image *_epeg_open_header(Epeg_Image *im)
{
   struct jpeg_marker_struct *m;

   im->in.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      error:
      epeg_close(im);
      im = NULL;
      return NULL;
   }

   jpeg_create_decompress(&(im->in.jinfo));
   jpeg_save_markers(&(im->in.jinfo), (JPEG_APP0 + 7), 1024);
   jpeg_save_markers(&(im->in.jinfo), JPEG_COM, 65535);
   jpeg_stdio_src(&(im->in.jinfo), im->in.f);
   jpeg_read_header(&(im->in.jinfo), TRUE);
   im->in.w = (int)im->in.jinfo.image_width;
   im->in.h = (int)im->in.jinfo.image_height;
   if (im->in.w <= 1) {
      goto error;
   }
   if (im->in.h <= 1) {
      goto error;
   }

   im->out.w = im->in.w;
   im->out.h = im->in.h;

   im->color_space = (((im->in.color_space = im->in.jinfo.out_color_space) == JCS_GRAYSCALE) ? EPEG_GRAY8 : EPEG_RGB8);
   if (im->in.color_space == JCS_CMYK) {
      im->color_space = EPEG_CMYK;
   }

   for ((m = im->in.jinfo.marker_list); m; (m = m->next)) {
      if (m->marker == JPEG_COM) {
         if (im->in.comment) {
            free(im->in.comment);
         }
         im->in.comment = (char *)malloc((size_t)(m->data_length + 1));
         if (im->in.comment) {
            memcpy(im->in.comment, m->data, (size_t)m->data_length);
            im->in.comment[m->data_length] = 0;
         }
      } else if (m->marker == (JPEG_APP0 + 7))  {
         if ((m->data_length > 7) &&
             (!strncmp((char *)m->data, "Thumb::", (size_t)7))) {
            char *p, *p2;

            p = (char *)malloc((size_t)(m->data_length + 1));
            if (p) {
               memcpy(p, m->data, (size_t)m->data_length);
               p[m->data_length] = 0;
               p2 = strchr(p, '\n');
               if (p2) {
                  p2[0] = 0;
                  if (!strcmp(p, "Thumb::URI")) {
                     im->in.thumb_info.uri = strdup(p2 + 1);
                  } else if (!strcmp(p, "Thumb::MTime")) {
                     sscanf((p2 + 1), "%llu", &(im->in.thumb_info.mtime));
                  } else if (!strcmp(p, "Thumb::Image::Width")) {
                     im->in.thumb_info.w = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Image::Height")) {
                     im->in.thumb_info.h = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Mimetype")) {
                     im->in.thumb_info.mime = strdup(p2 + 1);
                  }
               } /* end "if (p2)" */
               free(p);
            } /* end "if (p)" */
         }
      }
   } /* end for-loop */
   return im;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode(Epeg_Image *im)
{
   int scale, scalew, scaleh, y;

   if (im->pixels) {
      return 1;
   }

   scalew = (im->in.w / im->out.w);
   scaleh = (im->in.h / im->out.h);

   scale = scalew;
   if (scaleh < scalew) {
      scale = scaleh;
   }

   if (scale > 8) {
      scale = 8;
   } else if (scale < 1) {
      scale = 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = (unsigned int)scale;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_IFAST;

   switch (im->color_space) {
      case EPEG_GRAY8:
			 im->in.jinfo.out_color_space = JCS_GRAYSCALE;
			 im->in.jinfo.output_components = 1;
			 break;

      case EPEG_YUV8:
			 im->in.jinfo.out_color_space = JCS_YCbCr;
			 break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
			 im->in.jinfo.out_color_space = JCS_RGB;
			 break;

      case EPEG_CMYK:
			 im->in.jinfo.out_color_space = JCS_CMYK;
			 im->in.jinfo.output_components = 4;
			 break;

      default:
			 break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      epeg_close(im);
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
	   jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_scale(Epeg_Image *im)
{
   unsigned char *dst, *row, *src;
   int            x, y, w, h, i;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   for ((y = 0); (y < h); y++) {
      row = (im->pixels +
             ((((unsigned int)y * im->in.jinfo.output_height) / (unsigned int)h) * (unsigned int)im->in.jinfo.output_components * im->in.jinfo.output_width));
      dst = (im->pixels +
             ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));

      for ((x = 0); (x < im->out.w); x++) {
         src = (row +
                ((((unsigned int)x * im->in.jinfo.output_width) / (unsigned int)w) * (unsigned int)im->in.jinfo.output_components));

         for ((i = 0); (i < im->in.jinfo.output_components); i++) {
            dst[i] = src[i];
         } /* end inmost for-loop */

         dst += im->in.jinfo.output_components;
      } /* end inner for-loop */
   } /* end outer for-loop */
   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode_for_trim(Epeg_Image *im)
{
   int y;

   if (im->pixels) {
      return 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = 1;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_ISLOW;

   switch (im->color_space) {
      case EPEG_GRAY8:
		   im->in.jinfo.out_color_space = JCS_GRAYSCALE;
		   im->in.jinfo.output_components = 1;
		   break;

      case EPEG_YUV8:
		   im->in.jinfo.out_color_space = JCS_YCbCr;
		   break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
		   im->in.jinfo.out_color_space = JCS_RGB;
		   break;

      case EPEG_CMYK:
		   im->in.jinfo.out_color_space = JCS_CMYK;
		   im->in.jinfo.output_components = 4;
		   break;

      default:
		   break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
      im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
      jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_trim(Epeg_Image *im)
{
   int y, a, b, w, h;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   a = im->out.x;
   b = im->out.y;

   /* dummy condition to use value stored to 'w': */
   if (w == 0) {
	   ;
   }

   for ((y = 0); (y < h); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)((y + b) * im->in.jinfo.output_components) * im->in.jinfo.output_width)
                      + (a * im->in.jinfo.output_components));
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_encode(Epeg_Image *im)
{
   void  *data = NULL;
   size_t size = 0;

   if (im->out.f) {
      return 1;
   }

   if (im->out.file) {
      im->out.f = fopen(im->out.file, "wb");
   } else {
      im->out.f = _epeg_memfile_write_open(&data, &size);
   }
   if (!im->out.f) {
      im->error = 1;
      return 1;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_create_compress(&(im->out.jinfo));
   jpeg_stdio_dest(&(im->out.jinfo), im->out.f);
   im->out.jinfo.image_width = (JDIMENSION)im->out.w;
   im->out.jinfo.image_height = (JDIMENSION)im->out.h;
   im->out.jinfo.input_components = im->in.jinfo.output_components;
   im->out.jinfo.in_color_space = im->in.jinfo.out_color_space;
   im->out.jinfo.dct_method = JDCT_IFAST;
   im->out.jinfo.dct_method = im->in.jinfo.dct_method;
   jpeg_set_defaults(&(im->out.jinfo));
   jpeg_set_quality(&(im->out.jinfo), im->out.quality, TRUE);

   if (im->out.quality >= 90) {
      im->out.jinfo.comp_info[0].h_samp_factor = 1;
      im->out.jinfo.comp_info[0].v_samp_factor = 1;
      im->out.jinfo.comp_info[1].h_samp_factor = 1;
      im->out.jinfo.comp_info[1].v_samp_factor = 1;
      im->out.jinfo.comp_info[2].h_samp_factor = 1;
      im->out.jinfo.comp_info[2].v_samp_factor = 1;
   }
   jpeg_start_compress(&(im->out.jinfo), TRUE);

   if (im->out.comment) {
      jpeg_write_marker(&(im->out.jinfo), JPEG_COM,
                        (const JOCTET *)im->out.comment,
                        (unsigned int)strlen(im->out.comment));
   }

   if (im->out.thumbnail_info) {
      char buf[8192];

      if (im->in.file) {
         snprintf(buf, sizeof(buf), "Thumb::URI\nfile://%s", im->in.file);
         jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7),
                           (const JOCTET *)buf, (unsigned int)strlen(buf));
         snprintf(buf, sizeof(buf), "Thumb::MTime\n%llu",
#if defined(HAVE_UINTMAX_T) && !defined(__LP64__)
                  (uintmax_t)im->stat_info.st_mtime);
#else
               (unsigned long long int)im->stat_info.st_mtime);
#endif /* HAVE_UINTMAX_T && !__LP64__ */
      }
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Width\n%i", im->in.w);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Height\n%i", im->in.h);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Mimetype\nimage/jpeg");
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
   }

   while (im->out.jinfo.next_scanline < im->out.h) {
      jpeg_write_scanlines(&(im->out.jinfo),
                           &(im->lines[im->out.jinfo.next_scanline]), 1);
   }

   jpeg_finish_compress(&(im->out.jinfo));

   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if ((im->in.f) && (im->in.file)) {
      fclose(im->in.f);
   }
   if ((im->in.f) && (!im->in.file)) {
      _epeg_memfile_read_close(im->in.f);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if ((im->out.f) && (im->out.file)) {
      fclose(im->out.f);
   }
   if ((im->out.f) && (!im->out.file)) {
      _epeg_memfile_write_close(im->out.f);
   }
   im->in.f = NULL;
   im->out.f = NULL;

   if (im->out.mem.data) {
	   *(im->out.mem.data) = (unsigned char *)data;
   }
   if (im->out.mem.size) {
	   *(im->out.mem.size) = (int)size;
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static void _epeg_fatal_error_handler(j_common_ptr cinfo)
{
   emptr errmgr;

   errmgr = (emptr)cinfo->err;
   longjmp(errmgr->setjmp_buffer, 1);
   return;
}

/* silence '-Wunused-macros' warnings: */
#ifdef MAX
# undef MAX
#endif /* MAX */

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_memfile.c': */
/* epeg_memfile.c */
/* gets built into the libepeg library */

#include <stdio.h>
#include "Epeg.h"
#include "epeg_private.h"

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_read_open(void *data, size_t size)
{
#ifdef HAVE_FMEMOPEN
   return (FILE *)fmemopen(data, size, "r");
#else
   FILE *f;

   f = tmpfile();
   if (!f) {
      return NULL;
   }
   fwrite(data, size, (size_t)1L, f);
   rewind(f);
   return f;
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_read_close(FILE *f)
{
#ifdef HAVE_FMEMOPEN
   fclose(f);
#else
   /* FIXME: make these different halves of the conditional actually different
    * from one another */
   fclose(f);
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only struct and typedef; unnecessary to document: */
typedef struct _Eet_Memfile_Write_Info Eet_Memfile_Write_Info;
struct _Eet_Memfile_Write_Info
{
   FILE *f;
   void **data;
   size_t *size;
};

/* static internal private-only global variables; unnecessary to document: */
static int _epeg_memfile_info_alloc_num = 0;
static int _epeg_memfile_info_num = 0;
static Eet_Memfile_Write_Info *_epeg_memfile_info = NULL;

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_write_open(void **data, size_t *size)
{
#ifdef HAVE_OPEN_MEMSTREAM
   return open_memstream((char **)data, size);
#else
   FILE *f;

   _epeg_memfile_info_num++;
   if (_epeg_memfile_info_num > _epeg_memfile_info_alloc_num) {
      Eet_Memfile_Write_Info *tmp;

      _epeg_memfile_info_alloc_num += 16;
      tmp = (Eet_Memfile_Write_Info *)realloc(_epeg_memfile_info,
                                              ((unsigned long)_epeg_memfile_info_alloc_num *
                                               sizeof(Eet_Memfile_Write_Info)));
      if (!tmp) {
         _epeg_memfile_info_alloc_num -= 16;
         _epeg_memfile_info_num--;
         return NULL;
      }
      _epeg_memfile_info = tmp;
   }
   f = tmpfile();
   if (!f) {
      _epeg_memfile_info_num--;
      return NULL;
   }
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].f = f;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].data = data;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].size = size;
   return f;
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_write_close(FILE *f)
{
#ifdef HAVE_OPEN_MEMSTREAM
   fclose(f);
#else
   int i;

   for ((i = 0); (i < _epeg_memfile_info_num); i++) {
      if (_epeg_memfile_info[i].f == f) {
         int j;

         fseek(f, 0L, SEEK_END);
         (*(_epeg_memfile_info[i].size)) = (size_t)ftell(f);
         rewind(f);
         (*(_epeg_memfile_info[i].data)) = malloc(*(_epeg_memfile_info[i].size));
         if (!(*(_epeg_memfile_info[i].data))) {
            fclose(f);
            (*(_epeg_memfile_info[i].size)) = 0;
            return;
         }
         fread((*(_epeg_memfile_info[i].data)), (*(_epeg_memfile_info[i].size)),
               (size_t)1L, f);
         for ((j = (i + 1)); (j < _epeg_memfile_info_num); j++) {
            _epeg_memfile_info[(j - 1)] = _epeg_memfile_info[j];
         }
         _epeg_memfile_info_num--;
         fclose(f);
         return;
      } /* end "if" */
   } /* end for-loop */
   fclose(f);
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_main.c': */
/* epeg_main.c for src/lib for epeg */
/* gets built into the libepeg library */

#include <stdio.h>
#include <stdlib.h>
#include "Epeg.h"
#include "epeg_private.h"

static Epeg_Image*_epeg_open_header(Epeg_Image *im);
static int _epeg_decode(Epeg_Image *im);
static int _epeg_scale(Epeg_Image *im);
static int _epeg_decode_for_trim(Epeg_Image *im);
static int _epeg_trim(Epeg_Image *im);
static int _epeg_encode(Epeg_Image *im);

static void _epeg_fatal_error_handler(j_common_ptr cinfo);

#ifndef MIN
# define MIN(__x,__y) ((__x) < (__y) ? (__x) : (__y))
#endif /* !MIN */
#ifndef MAX
# define MAX(__x,__y) ((__x) > (__y) ? (__x) : (__y))
#endif /* !MAX */

/**
 * Open a JPEG image by filename.
 * @param file The file path to open.
 * @return A handle to the opened JPEG file, with the header decoded.
 *
 * This function opens the file indicated by the @p file parameter, and
 * attempts to decode it as a jpeg file. If this failes, NULL is returned.
 * Otherwise a valid handle to an open JPEG file is returned that can be used
 * by other Epeg calls.
 *
 * The @p file must be a pointer to a valid C string, NUL (0 byte) terminated
 * thats is a relative or absolute file path. If not results are not
 * determined.
 *
 * See also: epeg_memory_open(), epeg_close()
 */
extern Epeg_Image *epeg_file_open(const char *file)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.file = strdup(file);
   im->in.f = fopen(im->in.file, "rb");
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   fstat(fileno(im->in.f), &(im->stat_info));
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Open a JPEG image stored in memory.
 * @param data A pointer to the memory containing the JPEG data.
 * @param size The size of the memory segment containing the JPEG.
 * @return  A handle to the opened JPEG, with the header decoded.
 *
 * This function opens a JPEG file that is stored in memory pointed to by
 * @p data, and that is @p size bytes in size. If successful a valid handle
 * is returned, or on failure NULL is returned.
 *
 * See also: epeg_file_open(), epeg_close()
 */
extern Epeg_Image *epeg_memory_open(unsigned char *data, int size)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.f = _epeg_memfile_read_open(data, (size_t)size);
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Return the original JPEG pixel size.
 * @param im A handle to an opened Epeg image.
 * @param w A pointer to the width value in pixels to be filled in.
 * @param h A pointer to the height value in pixels to be filled in.
 * @return Nothing.
 *
 * Returns the image size in pixels (well not really).
 *
 * See also: epeg_colorspace_get()
 */
extern void epeg_size_get(Epeg_Image *im, int *w, int *h)
{
	if (w) {
		*w = im->in.w;
	}
	if (h) {
		*h = im->in.h;
	}
}

/**
 * Return the original JPEG pixel color space.
 * @param im A handle to an opened Epeg image.
 * @param space A pointer to the color space value to be filled in.
 * @return Nothing.
 *
 * Returns the image color space (not yet though).
 *
 * See also: epeg_size_get()
 */
extern void epeg_colorspace_get(Epeg_Image *im, int *space)
{
	if (space) {
		*space = im->color_space;
	}
}

/**
 * Set the size of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param w The width of the image to decode at, in pixels.
 * @param h The height of the image to decode at, in pixels.
 * @return Nothing.
 *
 * Sets the size at which to decode the JPEG image, giving an optimized load
 * that only decodes the pixels needed.
 *
 * See also: epeg_decode_bounds_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_size_set(Epeg_Image *im, int w, int h)
{
   if (im->pixels) {
	   return;
   }
   if (w < 1) {
	   w = 1;
   } else if (w > im->in.w) {
	   w = im->in.w;
   }
   if (h < 1) {
	   h = 1;
   } else if (h > im->in.h) {
	   h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   im->out.x = 0;
   im->out.y = 0;
}

/**
 * Set the bounds of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param x Boundary X
 * @param y Boundary Y
 * @param w Boundary W
 * @param h Boundary H
 * @return Nothing.
 *
 * Sets the bounds inside which to decode the JPEG image,
 * giving an optimized load that only decodes the bounded pixels.
 * (???)
 *
 * See also: epeg_decode_size_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_bounds_set(Epeg_Image *im, int x, int y, int w, int h)
{
   if (im->pixels) {
      return;
   }
   if (w < 1) {
      w = 1;
   } else if (w > im->in.w) {
      w = im->in.w;
   }
   if (h < 1) {
      h = 1;
   } else if (h > im->in.h) {
      h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   if (x < 0) {
      x = 0;
   }
   if (y < 0) {
      y = 0;
   }
   im->out.x = x;
   im->out.y = y;
}

/**
 * Set the colorspace in which to decode the image.
 * @param im A handle to an opened Epeg image.
 * @param colorspace The colorspace to decode the image in.
 * @return Nothing.
 *
 * This sets the colorspace to decode the image in. The default is EPEG_YUV8,
 * as this is normally the native colorspace of a JPEG file, avoiding any
 * colorspace conversions for a faster load and/or save.
 *
 * See also: epeg_decode_size_set(), epeg_decode_bounds_set()
 */
extern void epeg_decode_colorspace_set(Epeg_Image *im,
                                       Epeg_Colorspace colorspace)
{
	if (im->pixels) {
		return;
	}
	if ((colorspace < EPEG_GRAY8) || (colorspace > EPEG_ARGB32)) {
		return;
	}
	im->color_space = colorspace;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get_as_RGB8()
 */
extern const void *epeg_pixels_get(Epeg_Image *im, int x, int y,  int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
         return NULL;
      }
   }

   if (!im->pixels) {
      return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* go through all 8 values in type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace): */
   if (im->color_space == EPEG_GRAY8) {
      /* this whole block is pretty much the same thing we do for each
       * of the other enumeration values as well... could probably be simplified
       * with a function-like macro or some other sort of meta-programming: */
      unsigned char *pix, *p;

      /* '1': */
      pix = (unsigned char *)malloc((size_t)(w * h * 1L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = s[0];
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_YUV8) {
      unsigned char *pix, *p;

      /* '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGR8) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* reverse order: */
            p[0] = s[2];
            p[1] = s[1];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGBA8) {
      unsigned char *pix, *p;

      /* '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGRA8) {
      unsigned char *pix, *p;

      /* also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = 0xff;
            /* reverse order: */
            p[1] = s[2];
            p[2] = s[1];
            p[3] = s[0];
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_ARGB32) {
      unsigned int *pix, *p;

      /* still also '4': */
      pix = (unsigned int *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (0xff000000 | (unsigned int)(s[0] << 16) | (unsigned int)(s[1] << 8) | (s[2]));
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* another thing that is also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* back to same order again: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } /* end "if-else" that seems like it should really be a "switch" */
   return NULL;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get()
 */
extern const void *epeg_pixels_get_as_RGB8(Epeg_Image *im, int x, int y,
                                           int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
		   return NULL;
	   }
   }

   if (!im->pixels) {
	   return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* unlike the "non-'_as_RGB8'-version" of this function,
    * there are no 'else'-es separating the cases here. Also, we only go through
    * three of the enumeration values of type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace) here: */
   if (im->color_space == EPEG_GRAY8) {
      unsigned char *pix, *p;

      /* get right into the '3' ones: */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[0];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (unsigned char)(MIN(255, (s[0] * s[3]) / 255));
            p[1] = (unsigned char)(MIN(255, (s[1] * s[3]) / 255));
            p[2] = (unsigned char)(MIN(255, (s[2] * s[3]) / 255));
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   return NULL;
}

/**
 * Free requested pixel block from an image.
 * @param im A handle to an opened Epeg image (unused).
 * @param data The pointer to the image pixels.
 * @return Nothing.
 *
 * This frees the data for a block of pixels requested from image @p im.
 * @p data must be a valid (non NULL) pointer to a pixel block taken from the
 * image @p im by epeg_pixels_get() and must be called before the image is
 * closed by epeg_close().
 */
extern void epeg_pixels_free(Epeg_Image *im, const void *data)
{
#pragma unused (im)
   free((void *)data);
}

/**
 * Get the image comment field as a string.
 * @param im A handle to an opened Epeg image.
 * @return A pointer to the loaded image comments.
 *
 * This function returns the comment field as a string (NUL byte terminated)
 * of the loaded image @p im, if there is a comment, or NULL if no comment is
 * saved with the image. Consider the string returned to be read-only.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern const char *epeg_comment_get(Epeg_Image *im)
{
   return im->in.comment;
}

/**
 * Get thumbnail comments of loaded image.
 * @param im A handle to an opened Epeg image.
 * @param info Pointer to a thumbnail info struct to be filled in.
 * @return Nothing.
 *
 * This function retrieves thumbnail comments written by Epeg to any saved
 * JPEG files. If no thumbnail comments were saved, the fields will be 0 in
 * the @p info struct on return.
 *
 * See also: epeg_comment_get(), epeg_thumbnail_comments_enable()
 */
extern void epeg_thumbnail_comments_get(Epeg_Image *im,
                                        Epeg_Thumbnail_Info *info)
{
   if (!info) {
      return;
   }
   info->uri = im->in.thumb_info.uri;
   info->mtime = im->in.thumb_info.mtime;
   info->w = im->in.thumb_info.w;
   info->h = im->in.thumb_info.h;
   info->mimetype = im->in.thumb_info.mime;
}

/**
 * Set the comment field of the image for saving.
 * @param im A handle to an opened Epeg image.
 * @param comment The comment to set.
 * @return Nothing.
 *
 * Set the comment for the image file for when it gets saved. This is a NUL
 * byte terminated C string. If @p comment is NULL the output file will have
 * no comment field.
 *
 * The default comment will be any comment loaded from the input file.
 *
 * See also: epeg_comment_get()
 */
extern void epeg_comment_set(Epeg_Image *im, const char *comment)
{
   if (im->out.comment) {
      free(im->out.comment);
   }
   if (!comment) {
      im->out.comment = NULL;
   } else {
      im->out.comment = strdup(comment);
   }
}

/**
 * Set the encoding quality of the saved image.
 * @param im A handle to an opened Epeg image.
 * @param quality The quality of encoding from 0 to 100.
 * @return Nothing.
 *
 * Set the quality of the output encoded image. Values from 0 to 100
 * inclusive are valid, with 100 being the maximum quality, and 0 being the
 * minimum. If the quality is set equal to or above 90%, the output U and V
 * color planes are encoded at 1:1 with the Y plane.
 *
 * The default quality is 75.
 *
 * See also: epeg_comment_set()
 */
extern void epeg_quality_set(Epeg_Image *im, int quality)
{
   if (quality < 0) {
      quality = 0;
   } else if (quality > 100) {
      quality = 100;
   }
   im->out.quality = quality;
}

/**
 * Enable thumbnail comments in saved image.
 * @param im A handle to an opened Epeg image.
 * @param onoff A boolean on and off enabling flag.
 * @return Nothing.
 *
 * if @p onoff is 1, the output file will have thumbnail comments added to
 * it, and if it is 0, it will not. The default is 0.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern void epeg_thumbnail_comments_enable(Epeg_Image *im, int onoff)
{
   im->out.thumbnail_info = (char)onoff;
}

/**
 * Set the output file path for the image when saved.
 * @param im A handle to an opened Epeg image.
 * @param file The path to the output file.
 * @return Nothing.
 *
 * This sets the output file path name (either a full or relative path name)
 * to where the file will be written when saved. @p file must be a NUL
 * terminated C string containing the path to the file to be saved to. If it is
 * NULL, then the image will not be saved to a file when calling epeg_encode().
 *
 * See also: epeg_memory_output_set(), epeg_encode()
 */
extern void epeg_file_output_set(Epeg_Image *im, const char *file)
{
   if (im->out.file) {
      free(im->out.file);
   }
   if (!file) {
      im->out.file = NULL;
   } else {
      im->out.file = strdup(file);
   }
}

/**
 * Set the output file to be a block of allocated memory.
 * @param im A handle to an opened Epeg image.
 * @param data A pointer to a pointer to a memory block.
 * @param size A pointer to a counter of the size of the memory block.
 * @return Nothing.
 *
 * This sets the output encoding of the image when saved to be allocated
 * memory. After epeg_close() is called the pointer pointed to by @p data
 * and the integer pointed to by @p size will contain the pointer to the
 * memory block and its size in bytes, respecitvely. The memory block can be
 * freed with the free() function call. If the save fails the pointer to the
 * memory block will be unaffected, as will the size.
 *
 * See also: epeg_file_output_set(), epeg_encode()
 */
extern void epeg_memory_output_set(Epeg_Image *im, unsigned char **data,
                                   int *size)
{
   im->out.mem.data = data;
   im->out.mem.size = size;
}

/**
 * This saves the image to its specified destination.
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * This saves the image @p im to its destination specified by
 * epeg_file_output_set() or epeg_memory_output_set(). The image will be
 * encoded at the decoded pixel size, using the quality, comment,
 * and thumbnail comment settings set on the image.
 *
 * See also: epeg_file_output_set(), epeg_memory_output_set()
 */
extern int epeg_encode(Epeg_Image *im)
{
   if (_epeg_decode(im) != 0) {
      return 1;
   }
   if (_epeg_scale(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * FIXME: Document this with a short, sentence-long description of epeg_trim()
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * FIXME: Document this with a longer, paragraph-long description.
 */
extern int epeg_trim(Epeg_Image *im)
{
   if (_epeg_decode_for_trim(im) != 0) {
      return 1;
   }
   if (_epeg_trim(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * Close an image handle.
 * @param im A handle to an opened Epeg image.
 * @return Nothing.
 *
 * This closes an opened image handle and frees all memory associated with it.
 * It does NOT free encoded data generated by epeg_memory_output_set() followed
 * by epeg_encode(), nor does it guarantee to free any data received by
 * epeg_pixels_get(). Once an image handle is closed, consider it invalid.
 *
 * See also: epeg_file_open(), epeg_memory_open()
 */
extern void epeg_close(Epeg_Image *im)
{
   if (im->pixels) {
      free(im->pixels);
   }
   if (im->lines) {
      free(im->lines);
   }
   if (im->in.file) {
      free(im->in.file);
   }
   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if (im->in.f) {
      fclose(im->in.f);
   }
   if (im->in.comment) {
      free(im->in.comment);
   }
   if (im->in.thumb_info.uri) {
      free(im->in.thumb_info.uri);
   }
   if (im->in.thumb_info.mime) {
      free(im->in.thumb_info.mime);
   }
   if (im->out.file) {
      free(im->out.file);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if (im->out.f) {
      fclose(im->out.f);
   }
   if (im->out.comment) {
      free(im->out.comment);
   }
   free(im);
}

/* static internal private-only function; unnecessary to document: */
static Epeg_Image *_epeg_open_header(Epeg_Image *im)
{
   struct jpeg_marker_struct *m;

   im->in.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      error:
      epeg_close(im);
      im = NULL;
      return NULL;
   }

   jpeg_create_decompress(&(im->in.jinfo));
   jpeg_save_markers(&(im->in.jinfo), (JPEG_APP0 + 7), 1024);
   jpeg_save_markers(&(im->in.jinfo), JPEG_COM, 65535);
   jpeg_stdio_src(&(im->in.jinfo), im->in.f);
   jpeg_read_header(&(im->in.jinfo), TRUE);
   im->in.w = (int)im->in.jinfo.image_width;
   im->in.h = (int)im->in.jinfo.image_height;
   if (im->in.w <= 1) {
      goto error;
   }
   if (im->in.h <= 1) {
      goto error;
   }

   im->out.w = im->in.w;
   im->out.h = im->in.h;

   im->color_space = (((im->in.color_space = im->in.jinfo.out_color_space) == JCS_GRAYSCALE) ? EPEG_GRAY8 : EPEG_RGB8);
   if (im->in.color_space == JCS_CMYK) {
      im->color_space = EPEG_CMYK;
   }

   for ((m = im->in.jinfo.marker_list); m; (m = m->next)) {
      if (m->marker == JPEG_COM) {
         if (im->in.comment) {
            free(im->in.comment);
         }
         im->in.comment = (char *)malloc((size_t)(m->data_length + 1));
         if (im->in.comment) {
            memcpy(im->in.comment, m->data, (size_t)m->data_length);
            im->in.comment[m->data_length] = 0;
         }
      } else if (m->marker == (JPEG_APP0 + 7))  {
         if ((m->data_length > 7) &&
             (!strncmp((char *)m->data, "Thumb::", (size_t)7))) {
            char *p, *p2;

            p = (char *)malloc((size_t)(m->data_length + 1));
            if (p) {
               memcpy(p, m->data, (size_t)m->data_length);
               p[m->data_length] = 0;
               p2 = strchr(p, '\n');
               if (p2) {
                  p2[0] = 0;
                  if (!strcmp(p, "Thumb::URI")) {
                     im->in.thumb_info.uri = strdup(p2 + 1);
                  } else if (!strcmp(p, "Thumb::MTime")) {
                     sscanf((p2 + 1), "%llu", &(im->in.thumb_info.mtime));
                  } else if (!strcmp(p, "Thumb::Image::Width")) {
                     im->in.thumb_info.w = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Image::Height")) {
                     im->in.thumb_info.h = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Mimetype")) {
                     im->in.thumb_info.mime = strdup(p2 + 1);
                  }
               } /* end "if (p2)" */
               free(p);
            } /* end "if (p)" */
         }
      }
   } /* end for-loop */
   return im;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode(Epeg_Image *im)
{
   int scale, scalew, scaleh, y;

   if (im->pixels) {
      return 1;
   }

   scalew = (im->in.w / im->out.w);
   scaleh = (im->in.h / im->out.h);

   scale = scalew;
   if (scaleh < scalew) {
      scale = scaleh;
   }

   if (scale > 8) {
      scale = 8;
   } else if (scale < 1) {
      scale = 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = (unsigned int)scale;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_IFAST;

   switch (im->color_space) {
      case EPEG_GRAY8:
			 im->in.jinfo.out_color_space = JCS_GRAYSCALE;
			 im->in.jinfo.output_components = 1;
			 break;

      case EPEG_YUV8:
			 im->in.jinfo.out_color_space = JCS_YCbCr;
			 break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
			 im->in.jinfo.out_color_space = JCS_RGB;
			 break;

      case EPEG_CMYK:
			 im->in.jinfo.out_color_space = JCS_CMYK;
			 im->in.jinfo.output_components = 4;
			 break;

      default:
			 break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      epeg_close(im);
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
	   jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_scale(Epeg_Image *im)
{
   unsigned char *dst, *row, *src;
   int            x, y, w, h, i;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   for ((y = 0); (y < h); y++) {
      row = (im->pixels +
             ((((unsigned int)y * im->in.jinfo.output_height) / (unsigned int)h) * (unsigned int)im->in.jinfo.output_components * im->in.jinfo.output_width));
      dst = (im->pixels +
             ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));

      for ((x = 0); (x < im->out.w); x++) {
         src = (row +
                ((((unsigned int)x * im->in.jinfo.output_width) / (unsigned int)w) * (unsigned int)im->in.jinfo.output_components));

         for ((i = 0); (i < im->in.jinfo.output_components); i++) {
            dst[i] = src[i];
         } /* end inmost for-loop */

         dst += im->in.jinfo.output_components;
      } /* end inner for-loop */
   } /* end outer for-loop */
   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode_for_trim(Epeg_Image *im)
{
   int y;

   if (im->pixels) {
      return 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = 1;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_ISLOW;

   switch (im->color_space) {
      case EPEG_GRAY8:
		   im->in.jinfo.out_color_space = JCS_GRAYSCALE;
		   im->in.jinfo.output_components = 1;
		   break;

      case EPEG_YUV8:
		   im->in.jinfo.out_color_space = JCS_YCbCr;
		   break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
		   im->in.jinfo.out_color_space = JCS_RGB;
		   break;

      case EPEG_CMYK:
		   im->in.jinfo.out_color_space = JCS_CMYK;
		   im->in.jinfo.output_components = 4;
		   break;

      default:
		   break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
      im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
      jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_trim(Epeg_Image *im)
{
   int y, a, b, w, h;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   a = im->out.x;
   b = im->out.y;

   /* dummy condition to use value stored to 'w': */
   if (w == 0) {
	   ;
   }

   for ((y = 0); (y < h); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)((y + b) * im->in.jinfo.output_components) * im->in.jinfo.output_width)
                      + (a * im->in.jinfo.output_components));
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_encode(Epeg_Image *im)
{
   void  *data = NULL;
   size_t size = 0;

   if (im->out.f) {
      return 1;
   }

   if (im->out.file) {
      im->out.f = fopen(im->out.file, "wb");
   } else {
      im->out.f = _epeg_memfile_write_open(&data, &size);
   }
   if (!im->out.f) {
      im->error = 1;
      return 1;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_create_compress(&(im->out.jinfo));
   jpeg_stdio_dest(&(im->out.jinfo), im->out.f);
   im->out.jinfo.image_width = (JDIMENSION)im->out.w;
   im->out.jinfo.image_height = (JDIMENSION)im->out.h;
   im->out.jinfo.input_components = im->in.jinfo.output_components;
   im->out.jinfo.in_color_space = im->in.jinfo.out_color_space;
   im->out.jinfo.dct_method = JDCT_IFAST;
   im->out.jinfo.dct_method = im->in.jinfo.dct_method;
   jpeg_set_defaults(&(im->out.jinfo));
   jpeg_set_quality(&(im->out.jinfo), im->out.quality, TRUE);

   if (im->out.quality >= 90) {
      im->out.jinfo.comp_info[0].h_samp_factor = 1;
      im->out.jinfo.comp_info[0].v_samp_factor = 1;
      im->out.jinfo.comp_info[1].h_samp_factor = 1;
      im->out.jinfo.comp_info[1].v_samp_factor = 1;
      im->out.jinfo.comp_info[2].h_samp_factor = 1;
      im->out.jinfo.comp_info[2].v_samp_factor = 1;
   }
   jpeg_start_compress(&(im->out.jinfo), TRUE);

   if (im->out.comment) {
      jpeg_write_marker(&(im->out.jinfo), JPEG_COM,
                        (const JOCTET *)im->out.comment,
                        (unsigned int)strlen(im->out.comment));
   }

   if (im->out.thumbnail_info) {
      char buf[8192];

      if (im->in.file) {
         snprintf(buf, sizeof(buf), "Thumb::URI\nfile://%s", im->in.file);
         jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7),
                           (const JOCTET *)buf, (unsigned int)strlen(buf));
         snprintf(buf, sizeof(buf), "Thumb::MTime\n%llu",
#if defined(HAVE_UINTMAX_T) && !defined(__LP64__)
                  (uintmax_t)im->stat_info.st_mtime);
#else
               (unsigned long long int)im->stat_info.st_mtime);
#endif /* HAVE_UINTMAX_T && !__LP64__ */
      }
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Width\n%i", im->in.w);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Height\n%i", im->in.h);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Mimetype\nimage/jpeg");
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
   }

   while (im->out.jinfo.next_scanline < im->out.h) {
      jpeg_write_scanlines(&(im->out.jinfo),
                           &(im->lines[im->out.jinfo.next_scanline]), 1);
   }

   jpeg_finish_compress(&(im->out.jinfo));

   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if ((im->in.f) && (im->in.file)) {
      fclose(im->in.f);
   }
   if ((im->in.f) && (!im->in.file)) {
      _epeg_memfile_read_close(im->in.f);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if ((im->out.f) && (im->out.file)) {
      fclose(im->out.f);
   }
   if ((im->out.f) && (!im->out.file)) {
      _epeg_memfile_write_close(im->out.f);
   }
   im->in.f = NULL;
   im->out.f = NULL;

   if (im->out.mem.data) {
	   *(im->out.mem.data) = (unsigned char *)data;
   }
   if (im->out.mem.size) {
	   *(im->out.mem.size) = (int)size;
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static void _epeg_fatal_error_handler(j_common_ptr cinfo)
{
   emptr errmgr;

   errmgr = (emptr)cinfo->err;
   longjmp(errmgr->setjmp_buffer, 1);
   return;
}

/* silence '-Wunused-macros' warnings: */
#ifdef MAX
# undef MAX
#endif /* MAX */

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_memfile.c': */
/* epeg_memfile.c */
/* gets built into the libepeg library */

#include <stdio.h>
#include "Epeg.h"
#include "epeg_private.h"

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_read_open(void *data, size_t size)
{
#ifdef HAVE_FMEMOPEN
   return (FILE *)fmemopen(data, size, "r");
#else
   FILE *f;

   f = tmpfile();
   if (!f) {
      return NULL;
   }
   fwrite(data, size, (size_t)1L, f);
   rewind(f);
   return f;
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_read_close(FILE *f)
{
#ifdef HAVE_FMEMOPEN
   fclose(f);
#else
   /* FIXME: make these different halves of the conditional actually different
    * from one another */
   fclose(f);
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only struct and typedef; unnecessary to document: */
typedef struct _Eet_Memfile_Write_Info Eet_Memfile_Write_Info;
struct _Eet_Memfile_Write_Info
{
   FILE *f;
   void **data;
   size_t *size;
};

/* static internal private-only global variables; unnecessary to document: */
static int _epeg_memfile_info_alloc_num = 0;
static int _epeg_memfile_info_num = 0;
static Eet_Memfile_Write_Info *_epeg_memfile_info = NULL;

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_write_open(void **data, size_t *size)
{
#ifdef HAVE_OPEN_MEMSTREAM
   return open_memstream((char **)data, size);
#else
   FILE *f;

   _epeg_memfile_info_num++;
   if (_epeg_memfile_info_num > _epeg_memfile_info_alloc_num) {
      Eet_Memfile_Write_Info *tmp;

      _epeg_memfile_info_alloc_num += 16;
      tmp = (Eet_Memfile_Write_Info *)realloc(_epeg_memfile_info,
                                              ((unsigned long)_epeg_memfile_info_alloc_num *
                                               sizeof(Eet_Memfile_Write_Info)));
      if (!tmp) {
         _epeg_memfile_info_alloc_num -= 16;
         _epeg_memfile_info_num--;
         return NULL;
      }
      _epeg_memfile_info = tmp;
   }
   f = tmpfile();
   if (!f) {
      _epeg_memfile_info_num--;
      return NULL;
   }
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].f = f;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].data = data;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].size = size;
   return f;
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_write_close(FILE *f)
{
#ifdef HAVE_OPEN_MEMSTREAM
   fclose(f);
#else
   int i;

   for ((i = 0); (i < _epeg_memfile_info_num); i++) {
      if (_epeg_memfile_info[i].f == f) {
         int j;

         fseek(f, 0L, SEEK_END);
         (*(_epeg_memfile_info[i].size)) = (size_t)ftell(f);
         rewind(f);
         (*(_epeg_memfile_info[i].data)) = malloc(*(_epeg_memfile_info[i].size));
         if (!(*(_epeg_memfile_info[i].data))) {
            fclose(f);
            (*(_epeg_memfile_info[i].size)) = 0;
            return;
         }
         fread((*(_epeg_memfile_info[i].data)), (*(_epeg_memfile_info[i].size)),
               (size_t)1L, f);
         for ((j = (i + 1)); (j < _epeg_memfile_info_num); j++) {
            _epeg_memfile_info[(j - 1)] = _epeg_memfile_info[j];
         }
         _epeg_memfile_info_num--;
         fclose(f);
         return;
      } /* end "if" */
   } /* end for-loop */
   fclose(f);
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_main.c': */
/* epeg_main.c for src/lib for epeg */
/* gets built into the libepeg library */

#include <stdio.h>
#include <stdlib.h>
#include "Epeg.h"
#include "epeg_private.h"

static Epeg_Image*_epeg_open_header(Epeg_Image *im);
static int _epeg_decode(Epeg_Image *im);
static int _epeg_scale(Epeg_Image *im);
static int _epeg_decode_for_trim(Epeg_Image *im);
static int _epeg_trim(Epeg_Image *im);
static int _epeg_encode(Epeg_Image *im);

static void _epeg_fatal_error_handler(j_common_ptr cinfo);

#ifndef MIN
# define MIN(__x,__y) ((__x) < (__y) ? (__x) : (__y))
#endif /* !MIN */
#ifndef MAX
# define MAX(__x,__y) ((__x) > (__y) ? (__x) : (__y))
#endif /* !MAX */

/**
 * Open a JPEG image by filename.
 * @param file The file path to open.
 * @return A handle to the opened JPEG file, with the header decoded.
 *
 * This function opens the file indicated by the @p file parameter, and
 * attempts to decode it as a jpeg file. If this failes, NULL is returned.
 * Otherwise a valid handle to an open JPEG file is returned that can be used
 * by other Epeg calls.
 *
 * The @p file must be a pointer to a valid C string, NUL (0 byte) terminated
 * thats is a relative or absolute file path. If not results are not
 * determined.
 *
 * See also: epeg_memory_open(), epeg_close()
 */
extern Epeg_Image *epeg_file_open(const char *file)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.file = strdup(file);
   im->in.f = fopen(im->in.file, "rb");
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   fstat(fileno(im->in.f), &(im->stat_info));
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Open a JPEG image stored in memory.
 * @param data A pointer to the memory containing the JPEG data.
 * @param size The size of the memory segment containing the JPEG.
 * @return  A handle to the opened JPEG, with the header decoded.
 *
 * This function opens a JPEG file that is stored in memory pointed to by
 * @p data, and that is @p size bytes in size. If successful a valid handle
 * is returned, or on failure NULL is returned.
 *
 * See also: epeg_file_open(), epeg_close()
 */
extern Epeg_Image *epeg_memory_open(unsigned char *data, int size)
{
   Epeg_Image *im;

   im = (Epeg_Image *)calloc((size_t)1, sizeof(Epeg_Image));
   im->in.f = _epeg_memfile_read_open(data, (size_t)size);
   if (!im->in.f) {
	   epeg_close(im);
	   return NULL;
   }
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Return the original JPEG pixel size.
 * @param im A handle to an opened Epeg image.
 * @param w A pointer to the width value in pixels to be filled in.
 * @param h A pointer to the height value in pixels to be filled in.
 * @return Nothing.
 *
 * Returns the image size in pixels (well not really).
 *
 * See also: epeg_colorspace_get()
 */
extern void epeg_size_get(Epeg_Image *im, int *w, int *h)
{
	if (w) {
		*w = im->in.w;
	}
	if (h) {
		*h = im->in.h;
	}
}

/**
 * Return the original JPEG pixel color space.
 * @param im A handle to an opened Epeg image.
 * @param space A pointer to the color space value to be filled in.
 * @return Nothing.
 *
 * Returns the image color space (not yet though).
 *
 * See also: epeg_size_get()
 */
extern void epeg_colorspace_get(Epeg_Image *im, int *space)
{
	if (space) {
		*space = im->color_space;
	}
}

/**
 * Set the size of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param w The width of the image to decode at, in pixels.
 * @param h The height of the image to decode at, in pixels.
 * @return Nothing.
 *
 * Sets the size at which to decode the JPEG image, giving an optimized load
 * that only decodes the pixels needed.
 *
 * See also: epeg_decode_bounds_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_size_set(Epeg_Image *im, int w, int h)
{
   if (im->pixels) {
	   return;
   }
   if (w < 1) {
	   w = 1;
   } else if (w > im->in.w) {
	   w = im->in.w;
   }
   if (h < 1) {
	   h = 1;
   } else if (h > im->in.h) {
	   h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   im->out.x = 0;
   im->out.y = 0;
}

/**
 * Set the bounds of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param x Boundary X
 * @param y Boundary Y
 * @param w Boundary W
 * @param h Boundary H
 * @return Nothing.
 *
 * Sets the bounds inside which to decode the JPEG image,
 * giving an optimized load that only decodes the bounded pixels.
 * (???)
 *
 * See also: epeg_decode_size_set(), epeg_decode_colorspace_set()
 */
extern void epeg_decode_bounds_set(Epeg_Image *im, int x, int y, int w, int h)
{
   if (im->pixels) {
      return;
   }
   if (w < 1) {
      w = 1;
   } else if (w > im->in.w) {
      w = im->in.w;
   }
   if (h < 1) {
      h = 1;
   } else if (h > im->in.h) {
      h = im->in.h;
   }
   im->out.w = w;
   im->out.h = h;
   if (x < 0) {
      x = 0;
   }
   if (y < 0) {
      y = 0;
   }
   im->out.x = x;
   im->out.y = y;
}

/**
 * Set the colorspace in which to decode the image.
 * @param im A handle to an opened Epeg image.
 * @param colorspace The colorspace to decode the image in.
 * @return Nothing.
 *
 * This sets the colorspace to decode the image in. The default is EPEG_YUV8,
 * as this is normally the native colorspace of a JPEG file, avoiding any
 * colorspace conversions for a faster load and/or save.
 *
 * See also: epeg_decode_size_set(), epeg_decode_bounds_set()
 */
extern void epeg_decode_colorspace_set(Epeg_Image *im,
                                       Epeg_Colorspace colorspace)
{
	if (im->pixels) {
		return;
	}
	if ((colorspace < EPEG_GRAY8) || (colorspace > EPEG_ARGB32)) {
		return;
	}
	im->color_space = colorspace;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get_as_RGB8()
 */
extern const void *epeg_pixels_get(Epeg_Image *im, int x, int y,  int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
         return NULL;
      }
   }

   if (!im->pixels) {
      return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* go through all 8 values in type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace): */
   if (im->color_space == EPEG_GRAY8) {
      /* this whole block is pretty much the same thing we do for each
       * of the other enumeration values as well... could probably be simplified
       * with a function-like macro or some other sort of meta-programming: */
      unsigned char *pix, *p;

      /* '1': */
      pix = (unsigned char *)malloc((size_t)(w * h * 1L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = s[0];
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_YUV8) {
      unsigned char *pix, *p;

      /* '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGR8) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* reverse order: */
            p[0] = s[2];
            p[1] = s[1];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_RGBA8) {
      unsigned char *pix, *p;

      /* '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_BGRA8) {
      unsigned char *pix, *p;

      /* also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            p[0] = 0xff;
            /* reverse order: */
            p[1] = s[2];
            p[2] = s[1];
            p[3] = s[0];
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_ARGB32) {
      unsigned int *pix, *p;

      /* still also '4': */
      pix = (unsigned int *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox)));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (0xff000000 | (unsigned int)(s[0] << 16) | (unsigned int)(s[1] << 8) | (s[2]));
            p++;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } else if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* another thing that is also '4': */
      pix = (unsigned char *)malloc((size_t)(w * h * 4L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 4));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* back to same order again: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p[3] = 0xff;
            p += 4;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   } /* end "if-else" that seems like it should really be a "switch" */
   return NULL;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 *
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organized from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 *
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed, or the image data cannot be decoded.
 *
 * See also: epeg_pixels_get()
 */
extern const void *epeg_pixels_get_as_RGB8(Epeg_Image *im, int x, int y,
                                           int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;

   if (!im->pixels) {
	   if (_epeg_decode(im) != 0) {
		   return NULL;
	   }
   }

   if (!im->pixels) {
	   return NULL;
   }

   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) {
      ow = (iw - x);
   }
   if ((y + oh) > ih) {
      oh = (ih - y);
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }
   if (x < 0) {
      ow += x;
      ox = -x;
   }
   if (y < 0) {
      oh += y;
      oy = -y;
   }
   if (ow < 1) {
      return NULL;
   }
   if (oh < 1) {
      return NULL;
   }

   ww = (x + ox + ow);
   hh = (y + oy + oh);

   /* unlike the "non-'_as_RGB8'-version" of this function,
    * there are no 'else'-es separating the cases here. Also, we only go through
    * three of the enumeration values of type Epeg_Colorspace
    * (i.e. enum _Epeg_Colorspace) here: */
   if (im->color_space == EPEG_GRAY8) {
      unsigned char *pix, *p;

      /* get right into the '3' ones: */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* same order: */
            p[0] = s[0];
            p[1] = s[0];
            p[2] = s[0];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_RGB8) {
      unsigned char *pix, *p;

      /* also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* also same order: */
            p[0] = s[0];
            p[1] = s[1];
            p[2] = s[2];
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   if (im->color_space == EPEG_CMYK) {
      unsigned char *pix, *p;

      /* still also '3': */
      pix = (unsigned char *)malloc((size_t)(w * h * 3L));
      if (!pix) {
         return NULL;
      }
      for ((yy = (y + oy)); (yy < hh); yy++) {
         unsigned char *s;

         s = (im->lines[yy] + ((x + ox) * bpp));
         p = (pix + ((((yy - y) * w) + ox) * 3));
         for ((xx = (x + ox)); (xx < ww); xx++) {
            /* what? */
            p[0] = (unsigned char)(MIN(255, (s[0] * s[3]) / 255));
            p[1] = (unsigned char)(MIN(255, (s[1] * s[3]) / 255));
            p[2] = (unsigned char)(MIN(255, (s[2] * s[3]) / 255));
            p += 3;
            s += bpp;
         } /* end inner for-loop (for 'xx') */
      } /* end outer for-loop (for 'yy') */
      return pix;
   }
   return NULL;
}

/**
 * Free requested pixel block from an image.
 * @param im A handle to an opened Epeg image (unused).
 * @param data The pointer to the image pixels.
 * @return Nothing.
 *
 * This frees the data for a block of pixels requested from image @p im.
 * @p data must be a valid (non NULL) pointer to a pixel block taken from the
 * image @p im by epeg_pixels_get() and must be called before the image is
 * closed by epeg_close().
 */
extern void epeg_pixels_free(Epeg_Image *im, const void *data)
{
#pragma unused (im)
   free((void *)data);
}

/**
 * Get the image comment field as a string.
 * @param im A handle to an opened Epeg image.
 * @return A pointer to the loaded image comments.
 *
 * This function returns the comment field as a string (NUL byte terminated)
 * of the loaded image @p im, if there is a comment, or NULL if no comment is
 * saved with the image. Consider the string returned to be read-only.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern const char *epeg_comment_get(Epeg_Image *im)
{
   return im->in.comment;
}

/**
 * Get thumbnail comments of loaded image.
 * @param im A handle to an opened Epeg image.
 * @param info Pointer to a thumbnail info struct to be filled in.
 * @return Nothing.
 *
 * This function retrieves thumbnail comments written by Epeg to any saved
 * JPEG files. If no thumbnail comments were saved, the fields will be 0 in
 * the @p info struct on return.
 *
 * See also: epeg_comment_get(), epeg_thumbnail_comments_enable()
 */
extern void epeg_thumbnail_comments_get(Epeg_Image *im,
                                        Epeg_Thumbnail_Info *info)
{
   if (!info) {
      return;
   }
   info->uri = im->in.thumb_info.uri;
   info->mtime = im->in.thumb_info.mtime;
   info->w = im->in.thumb_info.w;
   info->h = im->in.thumb_info.h;
   info->mimetype = im->in.thumb_info.mime;
}

/**
 * Set the comment field of the image for saving.
 * @param im A handle to an opened Epeg image.
 * @param comment The comment to set.
 * @return Nothing.
 *
 * Set the comment for the image file for when it gets saved. This is a NUL
 * byte terminated C string. If @p comment is NULL the output file will have
 * no comment field.
 *
 * The default comment will be any comment loaded from the input file.
 *
 * See also: epeg_comment_get()
 */
extern void epeg_comment_set(Epeg_Image *im, const char *comment)
{
   if (im->out.comment) {
      free(im->out.comment);
   }
   if (!comment) {
      im->out.comment = NULL;
   } else {
      im->out.comment = strdup(comment);
   }
}

/**
 * Set the encoding quality of the saved image.
 * @param im A handle to an opened Epeg image.
 * @param quality The quality of encoding from 0 to 100.
 * @return Nothing.
 *
 * Set the quality of the output encoded image. Values from 0 to 100
 * inclusive are valid, with 100 being the maximum quality, and 0 being the
 * minimum. If the quality is set equal to or above 90%, the output U and V
 * color planes are encoded at 1:1 with the Y plane.
 *
 * The default quality is 75.
 *
 * See also: epeg_comment_set()
 */
extern void epeg_quality_set(Epeg_Image *im, int quality)
{
   if (quality < 0) {
      quality = 0;
   } else if (quality > 100) {
      quality = 100;
   }
   im->out.quality = quality;
}

/**
 * Enable thumbnail comments in saved image.
 * @param im A handle to an opened Epeg image.
 * @param onoff A boolean on and off enabling flag.
 * @return Nothing.
 *
 * if @p onoff is 1, the output file will have thumbnail comments added to
 * it, and if it is 0, it will not. The default is 0.
 *
 * See also: epeg_thumbnail_comments_get()
 */
extern void epeg_thumbnail_comments_enable(Epeg_Image *im, int onoff)
{
   im->out.thumbnail_info = (char)onoff;
}

/**
 * Set the output file path for the image when saved.
 * @param im A handle to an opened Epeg image.
 * @param file The path to the output file.
 * @return Nothing.
 *
 * This sets the output file path name (either a full or relative path name)
 * to where the file will be written when saved. @p file must be a NUL
 * terminated C string containing the path to the file to be saved to. If it is
 * NULL, then the image will not be saved to a file when calling epeg_encode().
 *
 * See also: epeg_memory_output_set(), epeg_encode()
 */
extern void epeg_file_output_set(Epeg_Image *im, const char *file)
{
   if (im->out.file) {
      free(im->out.file);
   }
   if (!file) {
      im->out.file = NULL;
   } else {
      im->out.file = strdup(file);
   }
}

/**
 * Set the output file to be a block of allocated memory.
 * @param im A handle to an opened Epeg image.
 * @param data A pointer to a pointer to a memory block.
 * @param size A pointer to a counter of the size of the memory block.
 * @return Nothing.
 *
 * This sets the output encoding of the image when saved to be allocated
 * memory. After epeg_close() is called the pointer pointed to by @p data
 * and the integer pointed to by @p size will contain the pointer to the
 * memory block and its size in bytes, respecitvely. The memory block can be
 * freed with the free() function call. If the save fails the pointer to the
 * memory block will be unaffected, as will the size.
 *
 * See also: epeg_file_output_set(), epeg_encode()
 */
extern void epeg_memory_output_set(Epeg_Image *im, unsigned char **data,
                                   int *size)
{
   im->out.mem.data = data;
   im->out.mem.size = size;
}

/**
 * This saves the image to its specified destination.
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * This saves the image @p im to its destination specified by
 * epeg_file_output_set() or epeg_memory_output_set(). The image will be
 * encoded at the decoded pixel size, using the quality, comment,
 * and thumbnail comment settings set on the image.
 *
 * See also: epeg_file_output_set(), epeg_memory_output_set()
 */
extern int epeg_encode(Epeg_Image *im)
{
   if (_epeg_decode(im) != 0) {
      return 1;
   }
   if (_epeg_scale(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * FIXME: Document this with a short, sentence-long description of epeg_trim()
 * @param im A handle to an opened Epeg image.
 * @return 1 if something happened, otherwise 0.
 *
 * FIXME: Document this with a longer, paragraph-long description.
 */
extern int epeg_trim(Epeg_Image *im)
{
   if (_epeg_decode_for_trim(im) != 0) {
      return 1;
   }
   if (_epeg_trim(im) != 0) {
      return 1;
   }
   if (_epeg_encode(im) != 0) {
      return 1;
   }
   return 0;
}

/**
 * Close an image handle.
 * @param im A handle to an opened Epeg image.
 * @return Nothing.
 *
 * This closes an opened image handle and frees all memory associated with it.
 * It does NOT free encoded data generated by epeg_memory_output_set() followed
 * by epeg_encode(), nor does it guarantee to free any data received by
 * epeg_pixels_get(). Once an image handle is closed, consider it invalid.
 *
 * See also: epeg_file_open(), epeg_memory_open()
 */
extern void epeg_close(Epeg_Image *im)
{
   if (im->pixels) {
      free(im->pixels);
   }
   if (im->lines) {
      free(im->lines);
   }
   if (im->in.file) {
      free(im->in.file);
   }
   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if (im->in.f) {
      fclose(im->in.f);
   }
   if (im->in.comment) {
      free(im->in.comment);
   }
   if (im->in.thumb_info.uri) {
      free(im->in.thumb_info.uri);
   }
   if (im->in.thumb_info.mime) {
      free(im->in.thumb_info.mime);
   }
   if (im->out.file) {
      free(im->out.file);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if (im->out.f) {
      fclose(im->out.f);
   }
   if (im->out.comment) {
      free(im->out.comment);
   }
   free(im);
}

/* static internal private-only function; unnecessary to document: */
static Epeg_Image *_epeg_open_header(Epeg_Image *im)
{
   struct jpeg_marker_struct *m;

   im->in.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      error:
      epeg_close(im);
      im = NULL;
      return NULL;
   }

   jpeg_create_decompress(&(im->in.jinfo));
   jpeg_save_markers(&(im->in.jinfo), (JPEG_APP0 + 7), 1024);
   jpeg_save_markers(&(im->in.jinfo), JPEG_COM, 65535);
   jpeg_stdio_src(&(im->in.jinfo), im->in.f);
   jpeg_read_header(&(im->in.jinfo), TRUE);
   im->in.w = (int)im->in.jinfo.image_width;
   im->in.h = (int)im->in.jinfo.image_height;
   if (im->in.w <= 1) {
      goto error;
   }
   if (im->in.h <= 1) {
      goto error;
   }

   im->out.w = im->in.w;
   im->out.h = im->in.h;

   im->color_space = (((im->in.color_space = im->in.jinfo.out_color_space) == JCS_GRAYSCALE) ? EPEG_GRAY8 : EPEG_RGB8);
   if (im->in.color_space == JCS_CMYK) {
      im->color_space = EPEG_CMYK;
   }

   for ((m = im->in.jinfo.marker_list); m; (m = m->next)) {
      if (m->marker == JPEG_COM) {
         if (im->in.comment) {
            free(im->in.comment);
         }
         im->in.comment = (char *)malloc((size_t)(m->data_length + 1));
         if (im->in.comment) {
            memcpy(im->in.comment, m->data, (size_t)m->data_length);
            im->in.comment[m->data_length] = 0;
         }
      } else if (m->marker == (JPEG_APP0 + 7))  {
         if ((m->data_length > 7) &&
             (!strncmp((char *)m->data, "Thumb::", (size_t)7))) {
            char *p, *p2;

            p = (char *)malloc((size_t)(m->data_length + 1));
            if (p) {
               memcpy(p, m->data, (size_t)m->data_length);
               p[m->data_length] = 0;
               p2 = strchr(p, '\n');
               if (p2) {
                  p2[0] = 0;
                  if (!strcmp(p, "Thumb::URI")) {
                     im->in.thumb_info.uri = strdup(p2 + 1);
                  } else if (!strcmp(p, "Thumb::MTime")) {
                     sscanf((p2 + 1), "%llu", &(im->in.thumb_info.mtime));
                  } else if (!strcmp(p, "Thumb::Image::Width")) {
                     im->in.thumb_info.w = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Image::Height")) {
                     im->in.thumb_info.h = atoi(p2 + 1);
                  } else if (!strcmp(p, "Thumb::Mimetype")) {
                     im->in.thumb_info.mime = strdup(p2 + 1);
                  }
               } /* end "if (p2)" */
               free(p);
            } /* end "if (p)" */
         }
      }
   } /* end for-loop */
   return im;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode(Epeg_Image *im)
{
   int scale, scalew, scaleh, y;

   if (im->pixels) {
      return 1;
   }

   scalew = (im->in.w / im->out.w);
   scaleh = (im->in.h / im->out.h);

   scale = scalew;
   if (scaleh < scalew) {
      scale = scaleh;
   }

   if (scale > 8) {
      scale = 8;
   } else if (scale < 1) {
      scale = 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = (unsigned int)scale;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_IFAST;

   switch (im->color_space) {
      case EPEG_GRAY8:
			 im->in.jinfo.out_color_space = JCS_GRAYSCALE;
			 im->in.jinfo.output_components = 1;
			 break;

      case EPEG_YUV8:
			 im->in.jinfo.out_color_space = JCS_YCbCr;
			 break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
			 im->in.jinfo.out_color_space = JCS_RGB;
			 break;

      case EPEG_CMYK:
			 im->in.jinfo.out_color_space = JCS_CMYK;
			 im->in.jinfo.output_components = 4;
			 break;

      default:
			 break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      epeg_close(im);
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
	   jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_scale(Epeg_Image *im)
{
   unsigned char *dst, *row, *src;
   int            x, y, w, h, i;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   for ((y = 0); (y < h); y++) {
      row = (im->pixels +
             ((((unsigned int)y * im->in.jinfo.output_height) / (unsigned int)h) * (unsigned int)im->in.jinfo.output_components * im->in.jinfo.output_width));
      dst = (im->pixels +
             ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));

      for ((x = 0); (x < im->out.w); x++) {
         src = (row +
                ((((unsigned int)x * im->in.jinfo.output_width) / (unsigned int)w) * (unsigned int)im->in.jinfo.output_components));

         for ((i = 0); (i < im->in.jinfo.output_components); i++) {
            dst[i] = src[i];
         } /* end inmost for-loop */

         dst += im->in.jinfo.output_components;
      } /* end inner for-loop */
   } /* end outer for-loop */
   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_decode_for_trim(Epeg_Image *im)
{
   int y;

   if (im->pixels) {
      return 1;
   }

   im->in.jinfo.scale_num = 1;
   im->in.jinfo.scale_denom = 1;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing = FALSE;
   im->in.jinfo.dct_method = JDCT_ISLOW;

   switch (im->color_space) {
      case EPEG_GRAY8:
		   im->in.jinfo.out_color_space = JCS_GRAYSCALE;
		   im->in.jinfo.output_components = 1;
		   break;

      case EPEG_YUV8:
		   im->in.jinfo.out_color_space = JCS_YCbCr;
		   break;

      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
		   im->in.jinfo.out_color_space = JCS_RGB;
		   break;

      case EPEG_CMYK:
		   im->in.jinfo.out_color_space = JCS_CMYK;
		   im->in.jinfo.output_components = 4;
		   break;

      default:
		   break;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = (unsigned char *)malloc((size_t)(im->in.jinfo.output_width * im->in.jinfo.output_height * (unsigned int)im->in.jinfo.output_components));
   if (!im->pixels) {
      return 1;
   }

   im->lines = (unsigned char **)malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines) {
      free(im->pixels);
      im->pixels = NULL;
      return 1;
   }

   jpeg_start_decompress(&(im->in.jinfo));

   for ((y = 0); (y < im->in.jinfo.output_height); y++) {
      im->lines[y] = (im->pixels +
                      ((unsigned int)(y * im->in.jinfo.output_components) * im->in.jinfo.output_width));
   }

   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height) {
      jpeg_read_scanlines(&(im->in.jinfo),
                          &(im->lines[im->in.jinfo.output_scanline]),
                          (JDIMENSION)im->in.jinfo.rec_outbuf_height);
   }

   jpeg_finish_decompress(&(im->in.jinfo));

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_trim(Epeg_Image *im)
{
   int y, a, b, w, h;

   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) {
      return 1;
   }
   if (im->scaled) {
      return 1;
   }

   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   a = im->out.x;
   b = im->out.y;

   /* dummy condition to use value stored to 'w': */
   if (w == 0) {
	   ;
   }

   for ((y = 0); (y < h); y++) {
	   im->lines[y] = (im->pixels +
                      ((unsigned int)((y + b) * im->in.jinfo.output_components) * im->in.jinfo.output_width)
                      + (a * im->in.jinfo.output_components));
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static int _epeg_encode(Epeg_Image *im)
{
   void  *data = NULL;
   size_t size = 0;

   if (im->out.f) {
      return 1;
   }

   if (im->out.file) {
      im->out.f = fopen(im->out.file, "wb");
   } else {
      im->out.f = _epeg_memfile_write_open(&data, &size);
   }
   if (!im->out.f) {
      im->error = 1;
      return 1;
   }

   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;

   if (setjmp(im->jerr.setjmp_buffer)) {
      return 1;
   }

   jpeg_create_compress(&(im->out.jinfo));
   jpeg_stdio_dest(&(im->out.jinfo), im->out.f);
   im->out.jinfo.image_width = (JDIMENSION)im->out.w;
   im->out.jinfo.image_height = (JDIMENSION)im->out.h;
   im->out.jinfo.input_components = im->in.jinfo.output_components;
   im->out.jinfo.in_color_space = im->in.jinfo.out_color_space;
   im->out.jinfo.dct_method = JDCT_IFAST;
   im->out.jinfo.dct_method = im->in.jinfo.dct_method;
   jpeg_set_defaults(&(im->out.jinfo));
   jpeg_set_quality(&(im->out.jinfo), im->out.quality, TRUE);

   if (im->out.quality >= 90) {
      im->out.jinfo.comp_info[0].h_samp_factor = 1;
      im->out.jinfo.comp_info[0].v_samp_factor = 1;
      im->out.jinfo.comp_info[1].h_samp_factor = 1;
      im->out.jinfo.comp_info[1].v_samp_factor = 1;
      im->out.jinfo.comp_info[2].h_samp_factor = 1;
      im->out.jinfo.comp_info[2].v_samp_factor = 1;
   }
   jpeg_start_compress(&(im->out.jinfo), TRUE);

   if (im->out.comment) {
      jpeg_write_marker(&(im->out.jinfo), JPEG_COM,
                        (const JOCTET *)im->out.comment,
                        (unsigned int)strlen(im->out.comment));
   }

   if (im->out.thumbnail_info) {
      char buf[8192];

      if (im->in.file) {
         snprintf(buf, sizeof(buf), "Thumb::URI\nfile://%s", im->in.file);
         jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7),
                           (const JOCTET *)buf, (unsigned int)strlen(buf));
         snprintf(buf, sizeof(buf), "Thumb::MTime\n%llu",
#if defined(HAVE_UINTMAX_T) && !defined(__LP64__)
                  (uintmax_t)im->stat_info.st_mtime);
#else
               (unsigned long long int)im->stat_info.st_mtime);
#endif /* HAVE_UINTMAX_T && !__LP64__ */
      }
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Width\n%i", im->in.w);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Image::Height\n%i", im->in.h);
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
      snprintf(buf, sizeof(buf), "Thumb::Mimetype\nimage/jpeg");
      jpeg_write_marker(&(im->out.jinfo), (JPEG_APP0 + 7), (const JOCTET *)buf,
                        (unsigned int)strlen(buf));
   }

   while (im->out.jinfo.next_scanline < im->out.h) {
      jpeg_write_scanlines(&(im->out.jinfo),
                           &(im->lines[im->out.jinfo.next_scanline]), 1);
   }

   jpeg_finish_compress(&(im->out.jinfo));

   if (im->in.f) {
      jpeg_destroy_decompress(&(im->in.jinfo));
   }
   if ((im->in.f) && (im->in.file)) {
      fclose(im->in.f);
   }
   if ((im->in.f) && (!im->in.file)) {
      _epeg_memfile_read_close(im->in.f);
   }
   if (im->out.f) {
      jpeg_destroy_compress(&(im->out.jinfo));
   }
   if ((im->out.f) && (im->out.file)) {
      fclose(im->out.f);
   }
   if ((im->out.f) && (!im->out.file)) {
      _epeg_memfile_write_close(im->out.f);
   }
   im->in.f = NULL;
   im->out.f = NULL;

   if (im->out.mem.data) {
	   *(im->out.mem.data) = (unsigned char *)data;
   }
   if (im->out.mem.size) {
	   *(im->out.mem.size) = (int)size;
   }

   return 0;
}

/* static internal private-only function; unnecessary to document: */
static void _epeg_fatal_error_handler(j_common_ptr cinfo)
{
   emptr errmgr;

   errmgr = (emptr)cinfo->err;
   longjmp(errmgr->setjmp_buffer, 1);
   return;
}

/* silence '-Wunused-macros' warnings: */
#ifdef MAX
# undef MAX
#endif /* MAX */

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
/* ADDED BY GENDOC: CONTENTS OF './src/lib/epeg_memfile.c': */
/* epeg_memfile.c */
/* gets built into the libepeg library */

#include <stdio.h>
#include "Epeg.h"
#include "epeg_private.h"

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_read_open(void *data, size_t size)
{
#ifdef HAVE_FMEMOPEN
   return (FILE *)fmemopen(data, size, "r");
#else
   FILE *f;

   f = tmpfile();
   if (!f) {
      return NULL;
   }
   fwrite(data, size, (size_t)1L, f);
   rewind(f);
   return f;
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_read_close(FILE *f)
{
#ifdef HAVE_FMEMOPEN
   fclose(f);
#else
   /* FIXME: make these different halves of the conditional actually different
    * from one another */
   fclose(f);
#endif /* HAVE_FMEMOPEN */
}

/* internal private-only struct and typedef; unnecessary to document: */
typedef struct _Eet_Memfile_Write_Info Eet_Memfile_Write_Info;
struct _Eet_Memfile_Write_Info
{
   FILE *f;
   void **data;
   size_t *size;
};

/* static internal private-only global variables; unnecessary to document: */
static int _epeg_memfile_info_alloc_num = 0;
static int _epeg_memfile_info_num = 0;
static Eet_Memfile_Write_Info *_epeg_memfile_info = NULL;

/* internal private-only function; unnecessary to document: */
FILE *_epeg_memfile_write_open(void **data, size_t *size)
{
#ifdef HAVE_OPEN_MEMSTREAM
   return open_memstream((char **)data, size);
#else
   FILE *f;

   _epeg_memfile_info_num++;
   if (_epeg_memfile_info_num > _epeg_memfile_info_alloc_num) {
      Eet_Memfile_Write_Info *tmp;

      _epeg_memfile_info_alloc_num += 16;
      tmp = (Eet_Memfile_Write_Info *)realloc(_epeg_memfile_info,
                                              ((unsigned long)_epeg_memfile_info_alloc_num *
                                               sizeof(Eet_Memfile_Write_Info)));
      if (!tmp) {
         _epeg_memfile_info_alloc_num -= 16;
         _epeg_memfile_info_num--;
         return NULL;
      }
      _epeg_memfile_info = tmp;
   }
   f = tmpfile();
   if (!f) {
      _epeg_memfile_info_num--;
      return NULL;
   }
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].f = f;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].data = data;
   _epeg_memfile_info[(_epeg_memfile_info_num - 1)].size = size;
   return f;
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* internal private-only function; unnecessary to document: */
void _epeg_memfile_write_close(FILE *f)
{
#ifdef HAVE_OPEN_MEMSTREAM
   fclose(f);
#else
   int i;

   for ((i = 0); (i < _epeg_memfile_info_num); i++) {
      if (_epeg_memfile_info[i].f == f) {
         int j;

         fseek(f, 0L, SEEK_END);
         (*(_epeg_memfile_info[i].size)) = (size_t)ftell(f);
         rewind(f);
         (*(_epeg_memfile_info[i].data)) = malloc(*(_epeg_memfile_info[i].size));
         if (!(*(_epeg_memfile_info[i].data))) {
            fclose(f);
            (*(_epeg_memfile_info[i].size)) = 0;
            return;
         }
         fread((*(_epeg_memfile_info[i].data)), (*(_epeg_memfile_info[i].size)),
               (size_t)1L, f);
         for ((j = (i + 1)); (j < _epeg_memfile_info_num); j++) {
            _epeg_memfile_info[(j - 1)] = _epeg_memfile_info[j];
         }
         _epeg_memfile_info_num--;
         fclose(f);
         return;
      } /* end "if" */
   } /* end for-loop */
   fclose(f);
#endif /* HAVE_OPEN_MEMSTREAM */
}

/* various text editor settings:
 * # Emacs: -*-
 * coding: utf-8;
 * mode: C;
 * tab-width: 3;
 * indent-tabs-mode: nil;
 * c-basic-offset: 3
 * # -*-
 * # Vi:
 * # vim:fenc=utf-8:ft=C:et:sw=3:ts=3:sts=3
 */
/* EOF */
