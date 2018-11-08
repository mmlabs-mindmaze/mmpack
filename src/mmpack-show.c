/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "mmpack-show.h"

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

	if (strcmp(pkg->name, data->pkg_name) == 0) {
		data->found = 1;
		printf("%s (%s) %s\n", pkg->name, pkg->version,
		       pkg->state == MMPACK_PKG_INSTALLED ? "[installed]":"");

		printf("Package file: %s\n", pkg->filename);
		printf("SHA256: %s\n", pkg->sha256);

		printf("Source package: %s\n", pkg->source);

		printf("Dependencies:\n");
		mmpkg_dep_dump(pkg->mpkdeps, "MMPACK");
		mmpkg_dep_dump(pkg->sysdeps, "SYSTEM");

		printf("\nDescription:\n");
		printf("%s\n", pkg->desc);

		return 1;
	}

	return 0;
}

LOCAL_SYMBOL
int mmpack_show(struct mmpack_ctx * ctx, int argc, const char* argv[])
{
	struct cb_data data;

	if (argc < 2) {
		fprintf(stderr, "missing package argument in command line\n"
		                "Usage:\n\tmmpack show "SHOW_SYNOPSIS"\n");
		return -1;
	}
	data.pkg_name = argv[1];

	// Load prefix configuration and caches
	if (mmpack_ctx_use_prefix(ctx))
		return -1;

	data.found = 0;
	binindex_foreach(&ctx->binindex, binindex_cb, &data);
	if (!data.found)
		printf("No package found matching: \"%s\"\n", data.pkg_name);

	return 0;
}
