/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "mmpack-search.h"

#include <mmerrno.h>
#include <mmlib.h>
#include <mmsysio.h>
#include <string.h>
#include "context.h"
#include "package-utils.h"


static
int binindex_cb(struct mmpkg* pkg, void * data)
{
	char * pkg_name = (char *) data;

	if (strstr(pkg->name, pkg_name) != 0)
		printf("%s (%s)\n", pkg->name, pkg->version);

	return 0;
}

LOCAL_SYMBOL
int mmpack_search(struct mmpack_ctx * ctx, int argc, const char* argv[])
{
	char * pkg_name;

	if (argc < 2) {
		fprintf(stderr, "missing package argument in command line\n"
		                "Usage:\n\tmmpack search "SEARCH_SYNOPSIS"\n");
		return -1;
	}
	pkg_name = (char *) argv[1];

	if (mmpack_ctx_init_pkglist(ctx)) {
		fprintf(stderr, "Failed to load package lists\n");
		goto exit;
	}

	binindex_foreach(&ctx->binindex, binindex_cb, pkg_name);

exit:
	return 0;
}
