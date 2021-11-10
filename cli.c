#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ciff.h>

#include "caff.h"

#define CHUNKSIZ        4096


static void             _usage(void);
static void             _slurp(char **, size_t *, FILE *);
static void             _dump(FILE *, char *, unsigned long);

static void             _err(int eval, char *(*)(int), int,
			    char const *fmt, ...);
static char const       *_progname;


static void
_usage(void)
{
	(void)fprintf(stderr,
	    "usage: %s [-dhv] [-i index] [-o output]\n", _progname);
	exit(1);
}

static void
_slurp(char **dst, size_t *len, FILE *stream)
{
        size_t  n, size;
        char   *tmp;

        *len = size = 0;
        *dst = NULL;
        while (1) {
                /* Grow allocated memory as needed */
                if (*len + CHUNKSIZ + 1 > size) {
                        size = *len + CHUNKSIZ + 1;

                        /* overflow check */
                        if (size <= *len) {
                                free(*dst);
                                errx(1, "%s: overflow", __func__);
                        }

                        /* reallocation */
                        if ((tmp = realloc(*dst, size)) == NULL) {
                                free(*dst);
                                err(1, "%s: realloc", __func__);
                        }
                        *dst = tmp;
                }

                /* Read a chunk */
                n = fread(*dst + *len, 1, CHUNKSIZ, stream);
                if (n == 0)
                        break;

                *len += n;
        }

        if (ferror(stream)) {
                free(*dst);
                err(1, "%s: ferror", __func__);
        }
}

static void
_dump(FILE *target, char *data, unsigned long len)
{
        if (fwrite(data, 1, len, target) < len)
                err(1, "%s: fwrite", __func__);
}

static void
_err(int eval, char *(*fn)(int), int eno, char const *fmt, ...)
{
        va_list ap;
        size_t  newfmtlen, fmtlen, seplen, estrlen;
        char   *estr, *sep, *newfmt;

        va_start(ap, fmt);

        estr = fn(eno);
        sep = ": ";

        fmtlen = strlen(fmt);
        seplen = strlen(sep);
        estrlen = strlen(estr);

        newfmtlen = fmtlen + seplen + estrlen;
        if ((newfmt = malloc(newfmtlen + 1)) == NULL)
                err(1, "malloc");
        (void)strncpy(newfmt, fmt, newfmtlen);
        newfmt[fmtlen] = '\0';
        (void)strncat(newfmt, sep, newfmtlen - fmtlen);
        (void)strncat(newfmt, estr, newfmtlen - fmtlen - seplen);

        verrx(eval, newfmt, ap);

        va_end(ap);
        free(newfmt);
}


int
main(int argc, char **argv)
{
	size_t          len, idx;
	unsigned long   outlen;
	uintmax_t       uim;
	int             vflag, iflag, c;
	char           *input, *ep;
	unsigned char  *output;
	FILE           *in, *out;
	struct caff    *caff;
	struct frame   *fr;

	_progname = argv[0];

	vflag = 0, iflag = 0;
	in = stdin;
	out = stdout;
	while ((c = getopt(argc, argv, "hvi:o:")) != -1) {
		switch (c) {
		case 'h':
			_usage();
			break;
		case 'v':
			vflag = 1;
			break;
		case 'i':
			errno = 0;
			uim = strtoumax(optarg, &ep, 10);
			if (optarg[0] == '\0' || *ep != '\0')
				errx(1, "index must be a number");
			if (errno == ERANGE
			    && (uim == UINTMAX_MAX || uim > INT_MAX))
				errx(1, "index value out of range");
			idx = uim;
			iflag = 1;
			break;
		case 'o':
			if ((out = fopen(optarg, "wb")) == NULL)
				err(1, "%s: %s", __func__, optarg);
			break;
		default:
			_usage();
		}
	}
	argc -= optind, argv += optind;

	switch (argc) {
        case 0:         /* everything OK, input will be stdin */
                break;
        case 1:         /* explicit input file given */
                if (strcmp(argv[0], "-") == 0)
                        break;
                if ((in = fopen(argv[0], "rb")) == NULL)
                        err(1, "%s: %s", __func__, argv[0]);
                break;
        default:
                _usage();
        }


	if ((caff = malloc(sizeof (struct caff))) == NULL)
		err(1, "could not allocate memory");

	_slurp(&input, &len, in);
	if (caff_parse(caff, input, len) == NULL)
		errx(1, "parse failure");
	if (vflag)
		caff_dump_info(stderr, caff);

	if (iflag) {
		if (idx >= caff->caff_nframe)
			errx(1, "frame index out of bounds");
		fr = &caff->caff_frames[idx];
		if (vflag)
			(void)fprintf(stderr, "Duration: %llu\n",
			    fr->fr_dur);
		if (ciff_jpeg_compress(&output, &outlen, fr->fr_ciff)
		    == NULL)
			_err(1, ciff_strerror, (int)cifferrno,
			    "JPEG-compression failure");

		_dump(out, (char *)output, outlen);
	} else {
		if (caff_gif_compress(&output, &outlen, caff) == NULL)
			_err(1, caff_strerror, (int)cafferrno,
			    "GIF-compression failure");
		_dump(out, (char *)output, outlen);
	}

	if (fclose(out) == EOF)
		warn("%s: fclose", __func__);
	free(caff->caff_creator);
	free(caff->caff_frames);
	free(caff);
	return 0;
}
