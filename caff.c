#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <wand/MagickWand.h>
#include <ciff.h>

#include "caff.h"

#define MAGIC "CAFF"
#define MAGIC_LENGTH 4

#define TMPPATT "/tmp/caff.XXXXXXXXXX"
#define TMPPATTLEN 21

struct _caff_block {
	char    cb_id;
	long    cb_len;
	char   *cb_data;
};

struct _caff_header {
	long    ch_hsize;
	long    ch_nframe;
};

struct _caff_credits {
	short   cc_year;
	char    cc_month;
	char    cc_day;
	char    cc_hour;
	char    cc_minute;
	long    cc_creatorlen;
	char   *cc_creator;
};

static size_t                   curframe;
static int                      seen_credits;

static size_t const             creditsiz
				    = sizeof (struct _caff_credits);
static size_t const             framesiz = sizeof (struct frame);

static int                      _verify_magic(char *);
static int                      _verify_header(struct _caff_header *);
static int                      _verify_credits_date(
				    struct _caff_credits *);
static int                      _verify_credits(struct _caff_credits *);
static int                      _verify_frame(struct frame *);

static struct _caff_block *     _read_block(struct _caff_block *,
				    FILE *);

static int                      _parse_header(struct caff *,
				    struct _caff_header *);
static int                      _parse_credits(struct caff *,
				    struct _caff_credits *);
static int                      _parse_frame(struct caff *,
				    struct frame *);
static int                      _parse_block(struct caff *,
				    struct _caff_block *);

FILE **                         _get_tmpfile(FILE **, char*, size_t);

/* caff.h */
struct caff *                   caff_parse(struct caff *, FILE *);
void                            caff_dump_info(struct caff *, FILE *);
void                            caff_gif_compress(struct caff *,
				    FILE *);

static int
_verify_magic(char *magic)
{
	return memcmp(magic, MAGIC, MAGIC_LENGTH) == 0;
}

static int
_verify_header(struct _caff_header *hdr)
{
	if (hdr->ch_nframe < 0) {
		warnx("%s: number of frames must not be negative",
		    __func__);
		return 0;
	}

	return 1;
}

static int
_verify_credits_date(struct _caff_credits *cred)
{
	if (cred->cc_year < 1900) {
		warnx("%s: year must be greater than 1900", __func__);
		return 0;
	}
	if (cred->cc_month < 1 || cred->cc_month > 12) {
		warnx("%s: month must be in [1,12]", __func__);
		return 0;
	}
	if (cred->cc_day < 1 || cred->cc_day > 31) {
		warnx("%s: day must be in [1,31]", __func__);
		return 0;
	}
	if (cred->cc_hour < 0 || cred->cc_hour > 23) {
		warnx("%s: hour must be in [0,23]", __func__);
		return 0;
	}
	if (cred->cc_minute < 0 || cred->cc_minute > 59) {
		warnx("%s: minute must be in [0,59]", __func__);
		return 0;
	}

	return 1;
}

static int
_verify_credits(struct _caff_credits *cred)
{
	if (cred->cc_creatorlen < 0) {
		warnx("%s: creator length must not be negative",
		    __func__);
		return 0;
	}
	if (!_verify_credits_date(cred)) {
		warnx("%s: invalid date in credits", __func__);
		return 0;
	}

	return 1;
}

static int
_verify_frame(struct frame *fr)
{
	if (fr->fr_dur < 0) {
		warnx("%s: frame duration must not be negative",
		    __func__);
		return 0;
	}

	return 1;
}

static struct _caff_block *
_read_block(struct _caff_block *dst, FILE *stream)
{
	dst->cb_id = getc(stream);
	if (fread(&dst->cb_len, sizeof (long), 1, stream) != 1) {
		warnx("%s: fread", __func__);
		return NULL;
	}
	if (dst->cb_len < 0) {
		warnx("%s: block length must not be negative", __func__);
		return NULL;
	}

	if ((dst->cb_data = malloc(dst->cb_len)) == NULL) {
		warn("%s: malloc", __func__);
		return NULL;
	}
	/*
	 * Casting to size_t is okay, since len >= 0 and long always
	 * fits in size_t.
	 */
	if (fread(dst->cb_data, 1, dst->cb_len, stream)
	    != (size_t)dst->cb_len)
	{
		warnx("%s: fread", __func__);
		return NULL;
	}

	return dst;
}

static int
_parse_header(struct caff *caff, struct _caff_header *hdr)
{
	if (!_verify_header(hdr)) {
		warnx("%s: invalid header block", __func__);
		return 0;
	}

	caff->caff_hsize = hdr->ch_hsize;
	caff->caff_nframe = hdr->ch_nframe;
	if ((caff->caff_frames
	    = malloc(caff->caff_nframe * sizeof (struct frame)))
	    == NULL) {
		warn("%s: malloc", __func__);
		return 0;
	}

	return 1;
}

static int
_parse_credits(struct caff *caff, struct _caff_credits *cred)
{
	if (!_verify_credits(cred)) {
		warnx("%s: invalid credits block", __func__);
		return 0;
	}

	caff->caff_date.tm_year = cred->cc_year - 1900;
	caff->caff_date.tm_mon = cred->cc_month - 1;
	caff->caff_date.tm_mday = cred->cc_day;
	caff->caff_date.tm_hour = cred->cc_hour;
	caff->caff_date.tm_min = cred->cc_minute;

	caff->caff_creator = cred->cc_creator;

	return 1;
}

static int
_parse_frame(struct caff *caff, struct frame *fr)
{
	if (!_verify_frame(fr)) {
		warnx("%s: invalid animation block", __func__);
		return 0;
	}

	caff->caff_frames[curframe].fr_dur = fr->fr_dur;
	if (curframe >= (size_t)caff->caff_nframe) {
		warnx("%s: too many frames", __func__);
		/* tolerated, but ignored */
		return 1;
	}

	caff->caff_frames[curframe].fr_ciff = fr->fr_ciff;

	++curframe;
	return 1;
}

static int
_parse_block(struct caff *caff, struct _caff_block *blk)
{
	size_t                  len;
	struct _caff_credits   *cred;
	struct frame           *fr;
	FILE                   *datastream;
	char                   *p;

	switch (blk->cb_id) {
	case 0x01:      /* header */
		warnx("%s: ignoring superfluous header", __func__);
		break;

	case 0x02:      /* credits */
		if (seen_credits) {
			warnx("%s: ignoring superfluous credits",
			    __func__);
			break;
		}
		if ((cred = malloc(creditsiz)) == NULL) {
			warn("%s: malloc", __func__);
			return 0;
		}
		/* Copy fixed length fields (all except creator) */
		p = blk->cb_data;
		(void)memcpy(&cred->cc_year, p, sizeof (short));
		p += sizeof (short);
		(void)memcpy(&cred->cc_month, p++, 1);
		(void)memcpy(&cred->cc_day, p++, 1);
		(void)memcpy(&cred->cc_hour, p++, 1);
		(void)memcpy(&cred->cc_minute, p++, 1);
		(void)memcpy(&cred->cc_creatorlen, p, sizeof (long));
		p += sizeof (long);
		//(void)memcpy(cred, blk->cb_data,
		//    creditsiz - sizeof (char *));
		//blk->cb_data += creditsiz - sizeof (char *);
		/* the +1 is for NUL-termination */
		len = cred->cc_creatorlen + 1;
		if ((cred->cc_creator = malloc(len)) == NULL) {
			warn("%s: malloc", __func__);
			free(cred);
			return 0;
		}
		if (strlcpy(cred->cc_creator, p, len) >= len)
			warnx("%s: truncated creator string", __func__);
			/* we tolerate this */
		if (!_parse_credits(caff, cred)) {
			warnx("%s: failed to parse credits", __func__);
			free(cred->cc_creator);
			free(cred);
			return 0;
		}
		seen_credits = 1;
		break;

	case 0x03:      /* animation / frame */
		if ((fr = malloc(framesiz)) == NULL) {
			warn("%s: malloc", __func__);
			return 0;
		}
		/* Copy fixed length fields (all except ciff) */
		p = blk->cb_data;
		(void)memcpy(&fr->fr_dur, p,
		    framesiz - sizeof (struct ciff *));
		p += framesiz - sizeof (struct ciff *);
		/* -8 : -8 for length field */
		/* XXX this goes against the spec !!! */
		if ((datastream
		    = fmemopen(p, blk->cb_len - 8, "r"))
		    == NULL) {
			warn("%s: fmemopen", __func__);
			return 0;
		}
		if ((fr->fr_ciff = malloc(sizeof (struct ciff)))
		    == NULL) {
			warn("%s: malloc", __func__);
			return 0;
		}
		/*
		 * cast to size_t is okay, since nframe >= 0 and long
		 * always fits in size_t
		 */
		if (ciff_parse(fr->fr_ciff, datastream) == NULL) {
			warnx("%s: failed to parse data as CIFF",
			    __func__);
			goto err_fr;
		}
		if (!_parse_frame(caff, fr)) {
			warnx("%s: failed to parse frame", __func__);
			goto err_fr;
		}

		free(fr);
		break;

err_fr:
		free(fr->fr_ciff);
		free(fr);
		if (fclose(datastream) != 0)
			warn("%s: fclose", __func__);
		return 0;

	default:
		warnx("%s: unrecognized block ID %d", __func__,
		    blk->cb_id);
		return 0;
	}

	return 1;
}

FILE **
_get_tmpfile(FILE **ptr, char *path, size_t len) {
	int     fd;

	(void)strncpy(path, TMPPATT, len - 1);
	path[len - 1] = '\0';

	if ((fd = mkstemp(path)) == -1) {
		warn("%s: mkstemp", __func__);
		return NULL;
	}
	if ((*ptr = fopen(path, "w+")) == NULL) {
		warn("%s: fopen", __func__);
		if (unlink(path) == -1)
			warn("%s: unlink", __func__);
		return NULL;
	}

	return ptr;
}



/*---------------------------------------------------------------*
 *      caff.h                                                   *
 *---------------------------------------------------------------*/

struct caff *
caff_parse(struct caff *dst, FILE *stream)
{
	char                    c;
	struct _caff_block     *blk;
	struct _caff_header    *hdr;

	if ((blk = malloc(sizeof (struct _caff_block))) == NULL) {
		warn("%s: malloc", __func__);
		return NULL;
	}

	/*
	 * First block must be the header.
	 *
	 * While not explicitly stated in the spec, we ignore any
	 * further header blocks that may appear in the file.
	 */
	if (_read_block(blk, stream) == NULL) {
		warnx("%s: failed to read block", __func__);
		return NULL;
	}
	if (blk->cb_id != 0x01) {
		warnx("%s: first block must be a header", __func__);
		return NULL;
	}
	if ((hdr = malloc(sizeof (struct _caff_header))) == NULL) {
		warn("%s: malloc", __func__);
		return NULL;
	}
	if (!_verify_magic(blk->cb_data)) {
		warnx("%s: invalid magic", __func__);
		return NULL;
	}
	blk->cb_data += MAGIC_LENGTH;
	(void)memcpy(hdr, blk->cb_data, sizeof (struct _caff_header));
	if (!_parse_header(dst, hdr)) {
		warnx("%s: failed to parse header", __func__);
		free(hdr);
		return NULL;
	}
	free(hdr);

	/* Parse blocks */
	while ((c = fgetc(stream)) != EOF) {
		if (ungetc(c, stream) == EOF) {
			warnx("%s: ungetc", __func__);
			return NULL;
		}
		if (_read_block(blk, stream) == NULL) {
			warnx("%s: failed to read block", __func__);
			return NULL;
		}
		if (!_parse_block(dst, blk))
			warnx("%s: failed to parse a block", __func__);
			/* tolerate */
		free(blk->cb_data);
	}

	free(blk);
	return dst;
}

void
caff_dump_info(struct caff *caff, FILE *stream)
{
	char    dtbuf[BUFSIZ];

	if (strftime(dtbuf, BUFSIZ, "%Y-%m-%d %H:%M", &caff->caff_date)
	    == 0) {
		warnx("%s: strftime", __func__);
		return;
	}

	(void)fprintf(stream, "-----------------------------\n");
	(void)fprintf(stream, "Header size:\t%ld\n",
	    caff->caff_hsize);
	(void)fprintf(stream, "Frame count:\t%ld\n",
	    caff->caff_nframe);
	(void)fprintf(stream, "Creation date:\t%s\n", dtbuf);
	(void)fprintf(stream, "Creator name:\t%s\n",
	    caff->caff_creator);
	(void)fprintf(stream, "-----------------------------\n");
}

void
caff_gif_compress(struct caff *caff, FILE *out)
{
	char                   *tmppath, delay[8], buf[BUFSIZ];
	size_t                  tmppathlen, delaylen, i;
	MagickWand             *wand;
	FILE                   *tmp;

	/* Get a temporary file */
	tmppathlen = strlen(TMPPATT) + 1;
	if ((tmppath = malloc(tmppathlen)) == NULL) {
		warn("%s: malloc", __func__);
		return;
	}

	/* Call in the old wizard */
	MagickWandGenesis();
	wand = NewMagickWand();

	/* Build image list */
	delaylen = sizeof delay;
	for (i = 0; i < caff->caff_nframe; ++i) {
		/* write jpeg to a tmp file */
		if (_get_tmpfile(&tmp, tmppath, tmppathlen) == NULL) {
			warnx("%s: failed to get tmp file", __func__);
			return;
		}
		ciff_jpeg_compress(caff->caff_frames[i].fr_ciff, tmp);

		/* set duration of frame */
		if (snprintf(delay, delaylen, "%ld",
		    caff->caff_frames[i].fr_dur / 10) >= delaylen)
			warnx("%s: truncated delay string", __func__);
		if (MagickSetOption(wand, "delay", delay)
		    == MagickFalse) {
			warnx("%s: failed to set frame delay",
			    __func__);
			return;
		}

		/* read frame */
		if (MagickReadImage(wand, tmppath) == MagickFalse) {
			warnx("%s: failed to read image", __func__);
			return;
		}
		if (MagickSetImageFormat(wand, "gif") == MagickFalse) {
			warnx("%s: failed to set image format",
			    __func__);
			return;
		}

		/* remove the tmp file */
		if (unlink(tmppath) == -1)
			warn("%s: unlink", __func__);
	}

	/* Images will be written starting from current iterator */
	MagickResetIterator(wand);

	/* Write image sequence to a single GIF in a tmp file */
	if (_get_tmpfile(&tmp, tmppath, tmppathlen) == NULL) {
		warnx("%s: failed to get tmp file", __func__);
		return;
	}
	if (MagickWriteImages(wand, tmppath, MagickTrue) == MagickFalse)
	{
		warnx("%s: failed to write image", __func__);
		return;
	}

	/* Write GIF to out file descriptor */
	rewind(tmp);
	while (fread(buf, 1, sizeof buf, tmp) > 0)
		if (fwrite(buf, 1, sizeof buf, out) != sizeof buf) {
			warn("%s: fwrite", __func__);
			return;
		}
	if (ferror(tmp)) {
		warnx("%s: ferror", __func__);
	}

	/* later, Gandalf */
	wand = DestroyMagickWand(wand);

	if (fclose(tmp) == EOF)
		warn("%s: fclose", __func__);
	if (unlink(tmppath) == -1)
		warn("%s: unlink", __func__);
}
