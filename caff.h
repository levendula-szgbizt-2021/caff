#ifndef CAFF_H
#define CAFF_H

#include <ciff.h>
#include <stdio.h>
#include <time.h>

struct frame {
	unsigned long long      fr_dur;
	struct ciff            *fr_ciff;
};

struct caff {
	unsigned long long      caff_hsize;
	unsigned long long      caff_nframe;
	struct tm               caff_date;
	char                   *caff_creator;
	struct frame           *caff_frames;
};

struct caff *   caff_parse(struct caff *, char *, size_t);

void            caff_dump_info(FILE *, struct caff *);

void            caff_gif_compress(unsigned char **, size_t *,
		    struct caff *);

#endif /* CAFF_H */
