/* epeg_test.c */
/* the example from the comment in epeg.c.in, extracted for easy compilation */

#include <stdio.h> /* for printf() */
#include <stdlib.h> /* for exit() */
#include "Epeg.h"

/* main function: */
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

/* EOF */
