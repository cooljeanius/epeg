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
