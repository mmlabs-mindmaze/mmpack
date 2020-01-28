/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "mmpack-search.h"

#include <mmargparse.h>
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
	struct cb_data * data = (struct cb_data*) void_data;

	if (strstr(pkg->name, data->pkg_name) != 0) {
		data->found = 1;
		printf("%s %s (%s)\n",
		       pkg->state == MMPACK_PKG_INSTALLED ? "[installed]" : "[available]",
		       pkg->name,
		       pkg->version);
	}

	return 0;
}


/**
 * mmpack_search() - main function for the search command
 * @ctx: mmpack context
 * @argc: number of arguments
 * @argv: array of arguments
 *
 * searches given pattern within all the available package names.
 *
 * Return: 0 on success, -1 otherwise
 */
LOCAL_SYMBOL
int mmpack_search(struct mmpack_ctx * ctx, int argc, const char* argv[])
{
	struct cb_data data;

	if (mm_arg_is_completing())
		return 0;

	if (argc != 2
	    || STR_EQUAL(argv[1], strlen(argv[1]), "--help")
	    || STR_EQUAL(argv[1], strlen(argv[1]), "-h")) {
		fprintf(stderr, "Usage:\n\tmmpack "SEARCH_SYNOPSIS "\n");
		return argc != 2 ? -1 : 0;
	}

	data.pkg_name = argv[1];

	// Load prefix configuration and caches
	if (mmpack_ctx_use_prefix(ctx, 0))
		return -1;

	data.found = 0;
	binindex_foreach(&ctx->binindex, binindex_cb, &data);
	if (!data.found)
		printf("No package found matching pattern: \"%s\"\n",
		       data.pkg_name);

	return 0;
}
