#include <err.h>
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

static unsigned long long       curframe;
static int                      seen_credits;

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
void                            caff_gif_compress(unsigned char **,
				    size_t *, struct caff *);

static int
_verify_magic(char *magic)
{
	return memcmp(magic, MAGIC, MAGIC_LENGTH) == 0;
}

static int
_verify_date(struct _date *date)
{
	if (date->dt_year < 1900) {
		warnx("%s: year must be greater than 1900", __func__);
		return 0;
	}
	if (date->dt_month < 1 || date->dt_month > 12) {
		warnx("%s: month must be in [1,12]", __func__);
		return 0;
	}
	if (date->dt_day < 1 || date->dt_day > 31) {
		warnx("%s: day must be in [1,31]", __func__);
		return 0;
	}
	if (date->dt_hour > 23) {
		warnx("%s: hour must be in [0,23]", __func__);
		return 0;
	}
	if (date->dt_minute > 59) {
		warnx("%s: minute must be in [0,59]", __func__);
		return 0;
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
		warn("%s: malloc", __func__);
		return NULL;
	}
	if (*len < dst->blk_len) {
		warnx("%s: unexpected end", __func__);
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

	PARSE64(dst->caff_hsize, *in, len)
	PARSE64(dst->caff_nframe, *in, len)

	if ((dst->caff_frames
	    = malloc(dst->caff_nframe * sizeof (struct frame))) == NULL)
	{
		warn("%s: malloc", __func__);
		return 0;
	}

	if (len > 0)
		warnx("%s: trailing data", __func__);

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

	if (!_verify_date(&dt)) {
		warnx("%s: invalid date", __func__);
		return NULL;
	}
	dst->caff_date.tm_year = dt.dt_year - 1900;
	dst->caff_date.tm_mon = dt.dt_month - 1;
	dst->caff_date.tm_mday = dt.dt_day;
	dst->caff_date.tm_hour = dt.dt_hour;
	dst->caff_date.tm_min = dt.dt_minute;

	if ((dst->caff_creator = malloc(creatorlen + 1)) == NULL) {
		warn("%s: malloc", __func__);
		return NULL;
	}
	if (len < creatorlen) {
		warnx("%s: unexpected end", __func__);
		return NULL;
	}
	(void)strncpy(dst->caff_creator, *in, creatorlen);
	dst->caff_creator[creatorlen] = '\0';
	len -= creatorlen;

	if (len > 0)
		warnx("%s: trailing data", __func__);

	return dst;
}

static struct caff *
_parse_frame(struct caff *dst, char **in, unsigned long long len)
{
	if (len == 0)
		return NULL;

	PARSE64(dst->caff_frames[curframe].fr_dur, *in, len)

	if (curframe >= dst->caff_nframe) {
		warnx("%s: too many frames", __func__);
		/* tolerated, but ignored */
		return dst;
	}

	if ((dst->caff_frames[curframe].fr_ciff
	     = malloc(sizeof (struct ciff))) == NULL) {
		warn("%s: malloc", __func__);
		return NULL;
	}

	/* TODO unsigned long long <-> size_t */
	if (ciff_parse(dst->caff_frames[curframe].fr_ciff, *in, &len)
	    == NULL) {
		warnx("%s: failed to parse frame as ciff", __func__);
		return NULL;
	}
	++curframe;

	if (len > 0)
		warnx("%s: trailing data", __func__);

	return dst;
}

struct caff *
_parse_block(struct caff *caff, struct _block *blk)
{
	switch (blk->blk_id) {
	case ID_HEADER:
		warnx("%s: ignoring superfluous header", __func__);
		break;

	case ID_CREDS:
		if (seen_credits) {
			warnx("%s: ignoring superfluous credits",
			    __func__);
			break;
		}
		if (_parse_credits(caff, &blk->blk_data, blk->blk_len)
		    == NULL) {
			warnx("%s: failed to parse credits block",
			    __func__);
			return NULL;
		}
		seen_credits = 1;
		break;

	case ID_ANIM:
		if (_parse_frame(caff, &blk->blk_data, blk->blk_len)
		    == NULL) {
			warnx("%s: failed to parse frame", __func__);
			return NULL;
		}
		break;

	default:
		warnx("%s: unrecognized block ID %d", __func__,
		    blk->blk_id);
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
                        //cifferno = CIFF_EERRNO;
                        return i;
                }

        if (putc('\n', stream) == EOF) {
                //cifferno = CIFF_EERRNO;
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
		warn("%s: malloc", __func__);
		return NULL;
	}

	/*
	 * First block must be the header.
	 *
	 * While not explicitly stated in the spec, we ignore any
	 * further header blocks that may appear in the file.
	 */
	if (_read_block(blk, &in, &len) == NULL) {
		warnx("%s: failed to read block", __func__);
		free(blk);
		return NULL;
	}
	p = blk->blk_data;
	if (blk->blk_id != ID_HEADER) {
		warnx("%s: first block must be a header", __func__);
		free(blk);
		return NULL;
	}
	if (!_verify_magic(blk->blk_data)) {
		warnx("%s: invalid magic", __func__);
		free(blk);
		return NULL;
	}
	blk->blk_data += MAGIC_LENGTH; blk->blk_len -= MAGIC_LENGTH;
	if (_parse_header(dst, &blk->blk_data, blk->blk_len) == NULL) {
		warnx("%s: failed to read header", __func__);
		free(blk);
		return NULL;
	}
	free(p);

	/* Parse blocks */
	while (len > 0) {
		if (_read_block(blk, &in, &len) == NULL) {
			warnx("%s: failed to read block", __func__);
			free(blk);
			return NULL;
		}
		p = blk->blk_data;
		if (_parse_block(dst, blk) == NULL)
			warnx("%s: failed to parse a block", __func__);
			/* tolerate */
		free(p);
	}

	free(blk);
	return dst;
}

void
caff_dump_info(FILE *stream, struct caff *caff)
{
	char    dtbuf[BUFSIZ];

	if (strftime(dtbuf, BUFSIZ, "%Y-%m-%d %H:%M", &caff->caff_date)
	    == 0) {
		warnx("%s: strftime", __func__);
		return;
	}

	(void)_print_separator(stream, '-', 64);
	(void)fprintf(stream, "Header size:\t%llu\n",
	    caff->caff_hsize);
	(void)fprintf(stream, "Frame count:\t%llu\n",
	    caff->caff_nframe);
	(void)fprintf(stream, "Creation date:\t%s\n", dtbuf);
	(void)fprintf(stream, "Creator name:\t%s\n",
	    caff->caff_creator);
	(void)_print_separator(stream, '-', 64);
}

void
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
		    caff->caff_frames[i].fr_dur / 10) >= delaylen)
			warnx("%s: truncated delay string", __func__);
		if (MagickSetOption(wand, "delay", delay)
		    == MagickFalse) {
			warnx("%s: failed to set frame delay",
			    __func__);
			return;
		}

		/* read frame */
		if (MagickReadImageBlob(wand, jpeg, jpegsize)
		    == MagickFalse) {
			warnx("%s: failed to read image", __func__);
			return;
		}
		if (MagickSetImageFormat(wand, "gif") == MagickFalse) {
			warnx("%s: failed to set image format",
			    __func__);
			return;
		}
	}

	/* Images will be written starting from current iterator */
	MagickResetIterator(wand);
	if ((*out = MagickGetImagesBlob(wand, len)) == NULL) {
		warnx("%s: failed to write image", __func__);
		return;
	}

	/* later, Gandalf */
	wand = DestroyMagickWand(wand);
}
