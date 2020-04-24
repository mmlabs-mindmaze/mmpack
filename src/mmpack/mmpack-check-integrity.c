/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "mmpack-check-integrity.h"

#include <mmargparse.h>
#include <mmerrno.h>
#include <mmlib.h>
#include <mmpredefs.h>
#include <mmsysio.h>
#include <stdint.h>
#include <string.h>

#include "cmdline.h"
#include "context.h"
#include "mmstring.h"
#include "package-utils.h"
#include "pkg-fs-utils.h"
#include "utils.h"

struct cb_data {
	char const * pkg_name;
	const struct mmpack_ctx* ctx;
	int found;
	int error;
};


static
int binindex_cb(struct mmpkg* pkg, void * void_data)
{
	int rv;
	struct cb_data * data = (struct cb_data*) void_data;

	if (pkg->state == MMPACK_PKG_INSTALLED
	    && (data->pkg_name == NULL
	        || strcmp(pkg->name, data->pkg_name) == 0)) {
		info("Checking %s (%s) ... ", pkg->name, pkg->version);
		data->found = 1;

		rv = check_installed_pkg(data->ctx, pkg);
		if (rv == 0) {
			info("OK\n");
		} else {
			info("Failed!\n");
			data->error = 1;
			if (data->pkg_name != NULL)
				return rv;
		}
	}

	return 0;
}


/**
 * mmpack_check_integrity() - main function for the check integrity command
 * @ctx: mmpack context
 * @argc: number of arguments
 * @argv: array of arguments
 *
 * check package integrity, or all packages if none given
 *
 * Return: 0 on success, -1 otherwise
 */
LOCAL_SYMBOL
int mmpack_check_integrity(struct mmpack_ctx * ctx, int argc,
                           char const* argv[])
{
	struct cb_data data = {
		.pkg_name = (argc < 2) ? NULL : argv[1],
		.ctx = ctx,
		.found = 0,
		.error = 0,
	};

	if (mm_arg_is_completing()) {
		// Complete only first command argument and if not empty
		if (argc != 2 || argv[1][0] == '\0')
			return 0;

		return complete_pkgname(ctx, argv[1], ONLY_INSTALLED);
	}

	if (argc > 2
	    || (argc == 2
	        && (STR_EQUAL(argv[1], strlen(argv[1]), "--help")
	            || STR_EQUAL(argv[1], strlen(argv[1]), "-h")))) {
		fprintf(stderr,
		        "Usage:\n\tmmpack "CHECK_INTEGRITY_SYNOPSIS "\n");
		return argc > 2;
	}

	/* Load prefix configuration and caches */
	if (mmpack_ctx_use_prefix(ctx, 0))
		return -1;

	binindex_foreach(&ctx->binindex, binindex_cb, (void*) &data);
	if (data.pkg_name && !data.found)
		printf("Package \"%s\" not found\n", data.pkg_name);

	return data.error;
}
