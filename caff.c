#include <ciff.h>
#include <err.h>

#include "caff.h"

/* caff.h */
struct caff *   caff_parse(struct caff *, FILE *);
void            caff_dump_info(struct caff *, FILE *);
struct ciff *   caff_get_frame(struct caff *, size_t);
void            caff_gif_compress(struct caff *, FILE *);


/*---------------------------------------------------------------*
 *      caff.h                                                   *
 *---------------------------------------------------------------*/

struct caff *
caff_parse(struct caff *dst, FILE *stream)
{
	warnx("%s: not yet implemented", __func__);
	return NULL;
}

void
caff_dump_info(struct caff *caff, FILE *stream)
{
	warnx("%s: not yet implemented", __func__);
}

struct ciff *
caff_get_frame(struct caff *caff, size_t index)
{
	warnx("%s: not yet implemented", __func__);
	return NULL;
}

void
caff_gif_compress(struct caff *caff, FILE *out)
{
	warnx("%s: not yet implemented", __func__);
}
