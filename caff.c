#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ciff.h>
#include <wand/MagickWand.h>

#include "caff.h"

#define MAGIC           "CAFF"
#define MAGIC_LENGTH    4

#define ID_HEADER       0x01
#define ID_CREDS        0x02
#define ID_ANIM         0x03

/* Get a 16-bit little endian integer value */
#define INT16(x) (                                      \
          ((unsigned long long)((x)[0] & 0xff) << 0)    \
        | ((unsigned long long)((x)[1] & 0xff) << 8)    \
)

/* Get a 64-bit little endian integer value */
#define INT64(x) (                                      \
          ((unsigned long long)((x)[0] & 0xff) << 0)    \
        | ((unsigned long long)((x)[1] & 0xff) << 8)    \
        | ((unsigned long long)((x)[2] & 0xff) << 16)   \
        | ((unsigned long long)((x)[3] & 0xff) << 24)   \
        | ((unsigned long long)((x)[4] & 0xff) << 32)   \
        | ((unsigned long long)((x)[5] & 0xff) << 40)   \
        | ((unsigned long long)((x)[6] & 0xff) << 48)   \
        | ((unsigned long long)((x)[7] & 0xff) << 56)   \
)

/*
 * Parse a 64-bit integer from the input, advance input pointer by 8,
 * and decrease rem counter by 8.
 */
#define PARSE64(v, in, len)                             \
    if ((len) < 8) {                                    \
        return NULL;                        /* FIXME */ \
    }                                                   \
    v = INT64(in);                                      \
    in += 8; len -= 8;                                  \

/*
 * Parse a 16-bit integer from the input, advance input pointer by 2,
 * and decrease rem counter by 2.
 */
#define PARSE16(v, in, len)                             \
    if ((len) < 2) {                                    \
        return NULL;                        /* FIXME */ \
    }                                                   \
    v = INT16(in);                                      \
    in += 2; len -= 2;                                  \

/*
 * Parse a 8-bit unsigned integer from the input, increment input
 * pointer, decrement rem counter.
 */
#define PARSEU8(v, in, len)                             \
    if ((len) < 1) {                                    \
        return NULL;                        /* FIXME */ \
    }                                                   \
    v = (unsigned char)(*in);                           \
    ++in; --len;                                        \


struct _date {
	unsigned short          dt_year;
	unsigned char           dt_month;
	unsigned char           dt_day;
	unsigned char           dt_hour;
	unsigned char           dt_minute;
};

struct _block {
	unsigned char           blk_id;
	unsigned long long      blk_len;
	char                   *blk_data;
};

static int                      _verify_magic(char *);

static struct _block *          _read_block(struct _block *,
				    char **, size_t *);
static struct caff *            _parse_block(struct caff *,
				    struct _block *);
static struct caff *            _parse_header(struct caff *,
				    char **, unsigned long long);
static struct caff *            _parse_credits(struct caff *,
				    char **, unsigned long long);
static struct caff *            _parse_frame(struct caff *,
				    char **, unsigned long long);

static size_t                   _print_separator(FILE *, char, size_t);

/* caff.h */
struct caff *                   caff_parse(struct caff *, char *,
				    size_t);
void                            caff_dump_info(FILE *, struct caff *);
unsigned char **                caff_gif_compress(unsigned char **,
				    size_t *, struct caff *);
char *                          caff_strerror(enum caff_error);
void                            caff_destroy(struct caff *);

enum caff_error                 cafferrno;

static unsigned long long       curframe;
static int                      seen_credits;


static int
_verify_magic(char *magic)
{
	return memcmp(magic, MAGIC, MAGIC_LENGTH) == 0;
}

static int
_verify_date(struct _date *date)
{
	if (date->dt_year < 1900) {
		cafferrno = CAFF_EYEAR;
		return -1;
	}
	if (date->dt_month < 1 || date->dt_month > 12) {
		cafferrno = CAFF_EMONTH;
		return -1;
	}
	if (date->dt_day < 1 || date->dt_day > 31) {
		cafferrno = CAFF_EDAY;
		return -1;
	}
	if (date->dt_hour > 23) {
		cafferrno = CAFF_EHOUR;
		return -1;
	}
	if (date->dt_minute > 59) {
		cafferrno = CAFF_EMINUTE;
		return -1;
	}

	return 1;
}

static struct _block *
_read_block(struct _block *dst, char **in, size_t *len)
{
	if (len == 0)
		return NULL;

	dst->blk_id = *(*in)++;
	--*len;

	PARSE64(dst->blk_len, *in, *len)

	if ((dst->blk_data = malloc(dst->blk_len)) == NULL) {
		cafferrno = CAFF_EERRNO;
		return NULL;
	}
	if (*len < dst->blk_len) {
		cafferrno = CAFF_ENOMORE;
		return NULL;
	}
	(void)memcpy(dst->blk_data, *in, dst->blk_len);
	*in += dst->blk_len;
	*len -= dst->blk_len;

	return dst;
}

static struct caff *
_parse_header(struct caff *dst, char **in, unsigned long long len)
{
	if (len == 0)
		return NULL;

	*in += 8; len -= 8; /* ignore header size field, it's useless */
	PARSE64(dst->caff_nframe, *in, len)

	if ((dst->caff_frames
	    = malloc(dst->caff_nframe * sizeof (struct frame))) == NULL)
	{
		cafferrno = CAFF_EERRNO;
		return 0;
	}

	if (len > 0) {
		cafferrno = CAFF_ETRAIL;
		return NULL;
	}

	return dst;
}

static struct caff *
_parse_credits(struct caff *dst, char **in, unsigned long long len)
{
	struct _date            dt;
	unsigned long long      creatorlen;

	if (len == 0)
		return NULL;

	PARSE16(dt.dt_year, *in, len)
	PARSEU8(dt.dt_month, *in, len)
	PARSEU8(dt.dt_day, *in, len)
	PARSEU8(dt.dt_hour, *in, len)
	PARSEU8(dt.dt_minute, *in, len)
	PARSE64(creatorlen, *in, len)

	if (!_verify_date(&dt))
		return NULL;
	dst->caff_date.tm_year = dt.dt_year - 1900;
	dst->caff_date.tm_mon = dt.dt_month - 1;
	dst->caff_date.tm_mday = dt.dt_day;
	dst->caff_date.tm_hour = dt.dt_hour;
	dst->caff_date.tm_min = dt.dt_minute;

	if ((dst->caff_creator = malloc(creatorlen + 1)) == NULL) {
		cafferrno = CAFF_EERRNO;
		return NULL;
	}
	if (len < creatorlen) {
		cafferrno = CAFF_ENOMORE;
		return NULL;
	}
	(void)strncpy(dst->caff_creator, *in, creatorlen);
	dst->caff_creator[creatorlen] = '\0';
	len -= creatorlen;

	if (len > 0) {
		cafferrno = CAFF_ETRAIL;
		return NULL;
	}

	return dst;
}

static struct caff *
_parse_frame(struct caff *dst, char **in, unsigned long long len)
{
	if (len == 0)
		return NULL;

	if (curframe >= dst->caff_nframe) {
		cafferrno = CAFF_EFRAMEC;
		return NULL;
	}

	PARSE64(dst->caff_frames[curframe].fr_dur, *in, len)

	if ((dst->caff_frames[curframe].fr_ciff
	     = malloc(sizeof (struct ciff))) == NULL) {
		cafferrno = CAFF_EERRNO;
		return NULL;
	}

	/* TODO unsigned long long <-> size_t */
	if (ciff_parse(dst->caff_frames[curframe].fr_ciff, *in, &len)
	    == NULL) {
		cafferrno = CAFF_ECIFF;
		return NULL;
	}
	++curframe;

	if (len > 0) {
		cafferrno = CAFF_ETRAIL;
		return NULL;
	}

	return dst;
}

struct caff *
_parse_block(struct caff *caff, struct _block *blk)
{
	switch (blk->blk_id) {
	case ID_HEADER:
		cafferrno = CAFF_E2HDR;
		return NULL;

	case ID_CREDS:
		if (seen_credits) {
			cafferrno = CAFF_E2CREDS;
			return NULL;
		}
		if (_parse_credits(caff, &blk->blk_data, blk->blk_len)
		    == NULL)
			return NULL;
		seen_credits = 1;
		break;

	case ID_ANIM:
		if (_parse_frame(caff, &blk->blk_data, blk->blk_len)
		    == NULL)
			return NULL;
		break;

	default:
		cafferrno = CAFF_EBLKID;
		return NULL;
	}

	return caff;
}

static size_t
_print_separator(FILE *stream, char ch, size_t len)
{
        size_t  i;

        for (i = 0; i < len; ++i)
                if (putc(ch, stream) == EOF) {
                        cafferrno = CAFF_EERRNO;
                        return i;
                }

        if (putc('\n', stream) == EOF) {
                cafferrno = CAFF_EERRNO;
                return i;
        }

        return i + 1;
}


/*---------------------------------------------------------------*
 *      caff.h                                                   *
 *---------------------------------------------------------------*/

struct caff *
caff_parse(struct caff *dst, char *in, size_t len)
{
	struct _block     *blk;
	char              *p;

	if ((blk = malloc(sizeof (struct _block))) == NULL) {
		cafferrno = CAFF_EERRNO;
		return NULL;
	}

	/*
	 * First block must be the header.
	 *
	 * While not explicitly stated in the spec, we ignore any
	 * further header blocks that may appear in the file.
	 */
	if (_read_block(blk, &in, &len) == NULL) {
		free(blk);
		return NULL;
	}
	p = blk->blk_data;
	if (blk->blk_id != ID_HEADER) {
		cafferrno = CAFF_EHDRFRST;
		free(blk);
		return NULL;
	}
	if (!_verify_magic(blk->blk_data)) {
		cafferrno = CAFF_EMAGIC;
		free(blk);
		return NULL;
	}
	blk->blk_data += MAGIC_LENGTH; blk->blk_len -= MAGIC_LENGTH;
	if (_parse_header(dst, &blk->blk_data, blk->blk_len) == NULL) {
		free(blk);
		return NULL;
	}
	free(p);

	/* Parse blocks */
	while (len > 0) {
		if (_read_block(blk, &in, &len) == NULL) {
			free(blk);
			return NULL;
		}
		p = blk->blk_data;
		if (_parse_block(dst, blk) == NULL) {
			free(blk);
			return NULL;
		}
		free(p);
	}

	/* Verify all frames have been read */
	if (curframe != dst->caff_nframe) {
		cafferrno = CAFF_ENOMORE;
		return NULL;
	}

	free(blk);
	return dst;
}

void
caff_dump_info(FILE *stream, struct caff *caff)
{
	char    dtbuf[BUFSIZ];

	(void)strftime(dtbuf, BUFSIZ, "%Y-%m-%d %H:%M",
	    &caff->caff_date);

	(void)_print_separator(stream, '-', 64);
	(void)fprintf(stream, "Frame count:\t%llu\n",
	    caff->caff_nframe);
	(void)fprintf(stream, "Creation date:\t%s\n", dtbuf);
	(void)fprintf(stream, "Creator name:\t%s\n",
	    caff->caff_creator);
	(void)_print_separator(stream, '-', 64);
}

unsigned char **
caff_gif_compress(unsigned char **out, size_t *len, struct caff *caff)
{
	char                   *jpeg, delay[BUFSIZ];
	size_t                  jpegsize, i;
	int                     delaylen;
	MagickWand             *wand;

	/* Call in the old wizard */
	MagickWandGenesis();
	wand = NewMagickWand();

	/* Build image list */
	delaylen = sizeof delay;
	for (i = 0; i < caff->caff_nframe; ++i) {
		/* get jpeg image from ciff */
		ciff_jpeg_compress((unsigned char **)&jpeg, &jpegsize,
		    caff->caff_frames[i].fr_ciff);

		/* set duration of frame */
		if (snprintf(delay, delaylen, "%llu",
		    caff->caff_frames[i].fr_dur / 10) >= delaylen) {
			cafferrno = CAFF_EMISC;
			return NULL;
		}
		if (MagickSetOption(wand, "delay", delay)
		    == MagickFalse) {
			cafferrno = CAFF_EIMAGICK;
			return NULL;
		}

		/* read frame */
		if (MagickReadImageBlob(wand, jpeg, jpegsize)
		    == MagickFalse) {
			cafferrno = CAFF_EIMAGICK;
			return NULL;
		}
		if (MagickSetImageFormat(wand, "gif") == MagickFalse) {
			cafferrno = CAFF_EIMAGICK;
			return NULL;
		}

		free(jpeg);
	}

	/* Images will be written starting from current iterator */
	MagickResetIterator(wand);
	if ((*out = MagickGetImagesBlob(wand, len)) == NULL) {
		cafferrno = CAFF_EIMAGICK;
		return NULL;
	}

	/* later, Gandalf */
	wand = DestroyMagickWand(wand);

	return out;
}

char *
caff_strerror(enum caff_error err)
{
	switch (err) {
	case CAFF_EERRNO:
		return strerror(errno);
	case CAFF_EHDRFRST:
		return "First block must be a header block";
	case CAFF_EMAGIC:
		return "Invalid magic value";
	case CAFF_EBLKID:
		return "Unknown block ID encountered";
	case CAFF_EYEAR:
		return "Year must be greater than 1900";
	case CAFF_EMONTH:
		return "Month must be in range [1,12]";
	case CAFF_EDAY:
		return "Day must be in range [1,31]";
	case CAFF_EHOUR:
		return "Hour must be in range [0,23]";
	case CAFF_EMINUTE:
		return "Hour must be in range [0,59]";
	case CAFF_ETRAIL:
		return "Trailing data detected";
	case CAFF_EFRAMEC:
		return "Too many frames";
	case CAFF_ECIFF:
		return ciff_strerror(cifferrno);
	case CAFF_E2HDR:
		return "More than one header block detected";
	case CAFF_E2CREDS:
		return "More than one credits block detected";
	case CAFF_ENOMORE:
		return "Unexpected end of data";
	case CAFF_EIMAGICK:
		return "Unspecified ImageMagick error";
	case CAFF_EMISC: /* FALLTHROUGH */
	default:
		return "Unknown error";
	}
}

void
caff_destroy(struct caff *caff)
{
	unsigned long long      i;

	free(caff->caff_creator);
	for (i = 0; i < caff->caff_nframe; ++i)
		ciff_destroy(caff->caff_frames[i].fr_ciff);
	free(caff->caff_frames);
	free(caff);
}
