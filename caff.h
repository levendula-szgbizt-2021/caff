#ifndef CAFF_H
#define CAFF_H

#include <ciff.h>
#include <stdio.h>
#include <time.h>

struct frame {
	long            fr_dur;
	struct ciff    *fr_ciff;
};

struct caff {
	long            caff_hsize;
	long            caff_nframe;
	struct tm       caff_date;
	char           *caff_creator;
	struct frame   *caff_frames;
};

struct caff *   caff_parse(struct caff *, FILE *);
void            caff_dump_info(struct caff *, FILE *);

void            caff_gif_compress(struct caff *, FILE *);

#endif /* CAFF_H */
