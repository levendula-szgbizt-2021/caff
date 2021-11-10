#ifndef CAFF_H
#define CAFF_H

#include <ciff.h>
#include <stdio.h>
#include <time.h>

enum caff_error {
	CAFF_EERRNO     = -1,   /* standard error, consult errno */
	CAFF_EHDRFRST   = -2,   /* header must be first block */
	CAFF_EMAGIC     = -3,   /* invalid magic value */
	CAFF_EBLKID     = -4,   /* unknown block ID */
	CAFF_EYEAR      = -5,   /* invalid year value */
	CAFF_EMONTH     = -6,   /* invalid month value */
	CAFF_EDAY       = -7,   /* invalid day value */
	CAFF_EHOUR      = -8,   /* invalid hour value */
	CAFF_EMINUTE    = -9,   /* invalid minute value */
	CAFF_ETRAIL     = -10,  /* trailing data detected */
	CAFF_EFRAMEC    = -11,  /* frame count exceeded */
	CAFF_ECIFF      = -12,  /* CIFF parsing error */
	CAFF_E2HDR      = -13,  /* two header blocks detected */
	CAFF_E2CREDS    = -14,  /* two credits blocks detected */
	CAFF_ENOMORE    = -15,  /* no more data, ie unexpected end */
	CAFF_EIMAGICK   = -16,  /* generic ImageMagick error */
	CAFF_EMISC      = -17,  /* unspecified error */
};

extern enum caff_error  cafferrno;

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

struct caff *           caff_parse(struct caff *, char *, size_t);

void                    caff_dump_info(FILE *, struct caff *);

unsigned char **        caff_gif_compress(unsigned char **, size_t *,
			    struct caff *);

char *                  caff_strerror(enum caff_error);

void                    caff_destroy(struct caff *);

#endif /* CAFF_H */
