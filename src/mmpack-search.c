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


struct cb_data {
	const char * pkg_name;
	int found;
};

static
int binindex_cb(struct mmpkg* pkg, void * void_data)
{
	struct cb_data * data = (struct cb_data *) void_data;

	if (strstr(pkg->name, data->pkg_name) != 0) {
		data->found = 1;
		printf("%s (%s) %s\n", pkg->name, pkg->version,
		       pkg->state == MMPACK_PKG_INSTALLED ? "[installed]":"");
	}

	return 0;
}

LOCAL_SYMBOL
int mmpack_search(struct mmpack_ctx * ctx, int argc, const char* argv[])
{
	struct cb_data data;

	if (argc < 2) {
		fprintf(stderr, "missing package argument in command line\n"
		                "Usage:\n\tmmpack search "SEARCH_SYNOPSIS"\n");
		return -1;
	}
	data.pkg_name = argv[1];

	// Load prefix configuration and caches
	if (mmpack_ctx_use_prefix(ctx))
		return -1;

	data.found = 0;
	binindex_foreach(&ctx->binindex, binindex_cb, &data);
	if (!data.found)
		printf("No package found matching pattern: \"%s\"\n", data.pkg_name);

	return 0;
}
