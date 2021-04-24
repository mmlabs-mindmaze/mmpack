/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "mmpack-source.h"

#include <mmargparse.h>
#include <mmerrno.h>
#include <mmlib.h>
#include <mmsysio.h>
#include <string.h>

#include "cmdline.h"
#include "context.h"
#include "download.h"
#include "mmstring.h"
#include "package-utils.h"
#include "repo.h"
#include "srcindex.h"
#include "utils.h"


static
int download_pkg_sources(struct mmpack_ctx * ctx, struct binpkg const * pkg)
{
	const struct srcpkg* srcpkg;
	mmstr* dst;
	int rv;

	srcpkg = srcindex_lookup(&ctx->srcindex,
	                         pkg->source, pkg->version, pkg->srcsha);
	if (!srcpkg) {
		printf("Cannot source of package %s %s (%s)\n",
		       pkg->source, pkg->version, pkg->srcsha);
		return -1;
	}

	dst = mmstr_basename(NULL, srcpkg->remote_res->filename);
	rv = download_remote_resource(ctx, srcpkg->remote_res, dst);
	mmstr_free(dst);

	return rv;
}


/**
 * mmpack_source() - main function for the source command
 * @ctx: mmpack context
 * @argc: number of arguments
 * @argv: array of arguments
 *
 * download given package sources into current directory.
 *
 * Return: 0 on success, -1 otherwise
 */
LOCAL_SYMBOL
int mmpack_source(struct mmpack_ctx * ctx, int argc, const char* argv[])
{
	struct binpkg const * pkg;

	if (mm_arg_is_completing()) {
		if (argc != 2)
			return 0;

		return complete_pkgname(ctx, argv[1], AVAILABLE_PKGS);
	}

	if (argc != 2
	    || STR_EQUAL(argv[1], strlen(argv[1]), "--help")
	    || STR_EQUAL(argv[1], strlen(argv[1]), "-h")) {
		fprintf(stderr, "missing package argument in command line\n"
		        "Usage:\n\tmmpack "SOURCE_SYNOPSIS "\n");
		return argc != 2 ? -1 : 0;
	}

	/* Load prefix configuration and caches */
	if (mmpack_ctx_use_prefix(ctx, 0))
		return -1;

	if ((pkg = parse_pkg(ctx, argv[1])) == NULL)
		return -1;

	return download_pkg_sources(ctx, pkg);
}
