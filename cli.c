#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "caff.h"

void    usage(void);

__dead void
usage(void)
{
	(void)fprintf(stderr, "usage: %s [-dhv] [-o output]\n",
	    getprogname());
	exit(1);
}

int
main(int argc, char **argv)
{
	FILE           *out;
	int             vflag, c;
	struct caff    *caff;

	vflag = 0;
	out = stdout;
	while ((c = getopt(argc, argv, "hvo:")) != -1) {
		switch (c) {
		case 'h':
			usage();
			break;
		case 'v':
			vflag = 1;
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

	/* ciff_gif_compress(caff, out); */

	free(caff->caff_creator);
	free(caff->caff_frames);
	free(caff);
	return 0;
}
