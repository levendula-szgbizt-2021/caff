#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <ciff.h>

#include "caff.h"

void    usage(void);

__dead void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: %s [-dhv] [-i index] [-o output]\n", getprogname());
	exit(1);
}

int
main(int argc, char **argv)
{
	uintmax_t       uim;
	size_t          idx;
	int             vflag, iflag, c;
	char           *ep;
	FILE           *out;
	struct caff    *caff;
	struct frame   *fr;

	vflag = 0, iflag = 0;
	out = stdout;
	while ((c = getopt(argc, argv, "hvi:o:")) != -1) {
		switch (c) {
		case 'h':
			usage();
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
			usage();
		}
	}
	argc -= optind, argv += optind;

	if ((caff = malloc(sizeof (struct caff))) == NULL)
		err(1, "could not allocate memory");

	if (caff_parse(caff, stdin) == NULL)
		errx(1, "parse failure");
	if (vflag)
		caff_dump_info(caff, stderr);

	if (iflag) {
		if (idx >= caff->caff_nframe)
			errx(1, "frame index out of bounds");
		fr = &caff->caff_frames[idx];
		if (vflag)
			(void)fprintf(stderr, "Duration: %ld\n",
			    fr->fr_dur);
		ciff_jpeg_compress(fr->fr_ciff, out);
	} else
		caff_gif_compress(caff, out);

	if (fclose(out) == EOF) {
		warnx("%s: fclose", __func__);
	}
	free(caff->caff_creator);
	free(caff->caff_frames);
	free(caff);
	return 0;
}
