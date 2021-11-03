#ifndef CAFF_H
#define CAFF_H

#include <ciff.h>
#include <stdio.h>
#include <time.h>

struct frame {
	long            frame_dur;
	struct ciff    *frame_ciff;
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

struct ciff *   caff_get_frame(struct caff *, size_t);

void            caff_gif_compress(struct caff *, FILE *);

#endif /* CAFF_H */
