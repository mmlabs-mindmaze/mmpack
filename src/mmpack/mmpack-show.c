/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "mmpack-show.h"

#include <mmargparse.h>
#include <mmerrno.h>
#include <mmlib.h>
#include <mmsysio.h>
#include <string.h>
#include "cmdline.h"
#include "context.h"
#include "settings.h"
#include "package-utils.h"


struct cb_data {
	const char * pkg_name;
	int found;
};

static
int binindex_cb(struct mmpkg* pkg, void * void_data)
{
	struct cb_data * data = (struct cb_data*) void_data;
	struct from_repo * from = pkg->from_repo;

	if (strcmp(pkg->name, data->pkg_name) == 0) {
		data->found = 1;
		printf("%s (%s) %s\n", pkg->name, pkg->version,
		       pkg->state == MMPACK_PKG_INSTALLED ? "[installed]" : "");

		printf("SUMSHA256: %s\n", pkg->sumsha);

		if (from) {
			mm_check(from->repo != NULL);
			printf("Package file: %s\n", from->filename);
			printf("SHA256: %s\n", from->sha256);
			printf("Repository: %s\n", from->repo->name);
		}

		printf("Source package: %s\n", pkg->source);

		printf("Dependencies:\n");
		mmpkg_dep_dump(pkg->mpkdeps, "MMPACK");
		mmpkg_sysdeps_dump(&pkg->sysdeps, "SYSTEM");

		printf("\nDescription:\n");
		printf("%s\n", pkg->desc ? pkg->desc : "none");
		return 1;
	}

	return 0;
}


/**
 * mmpack_show() - main function for the show command
 * @ctx: mmpack context
 * @argc: number of arguments
 * @argv: array of arguments
 *
 * show given package metadatas.
 *
 * Return: 0 on success, -1 otherwise
 */
LOCAL_SYMBOL
int mmpack_show(struct mmpack_ctx * ctx, int argc, const char* argv[])
{
	struct cb_data data;

	if (mmarg_is_completing()) {
		if (argc != 2)
			return 0;

		return complete_pkgname(ctx, argv[1], AVAILABLE_PKGS);
	}

	if (argc != 2
	    || STR_EQUAL(argv[1], strlen(argv[1]), "--help")
	    || STR_EQUAL(argv[1], strlen(argv[1]), "-h")) {
		fprintf(stderr, "missing package argument in command line\n"
		        "Usage:\n\tmmpack show "SHOW_SYNOPSIS "\n");
		return argc != 2 ? -1 : 0;
	}

	data.pkg_name = argv[1];

	// Load prefix configuration and caches
	if (mmpack_ctx_use_prefix(ctx, 0))
		return -1;

	data.found = 0;
	binindex_foreach(&ctx->binindex, binindex_cb, &data);
	if (!data.found)
		printf("No package found matching: \"%s\"\n", data.pkg_name);

	return 0;
}
