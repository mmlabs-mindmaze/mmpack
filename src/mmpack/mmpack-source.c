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
#include "package-utils.h"


static
int download_pkg_sources(struct mmpack_ctx * ctx, struct mmpkg const * pkg)
{
	int rv = -1;

	mmstr * source_pkg_name;
	const mmstr* url;
	size_t source_pkg_name_len;
	struct from_repo * from;


	/* source pkg name: name_version_src.tar.gz */
	source_pkg_name_len = strlen(pkg->source) + 1 + strlen(pkg->version)
	                      + 1 + sizeof("_src.tar.gz");
	source_pkg_name = mmstr_malloc(source_pkg_name_len);
	sprintf(source_pkg_name, "%s_%s_src.tar.gz", pkg->source, pkg->version);
	mmstr_setlen(source_pkg_name, source_pkg_name_len);

	for (from = pkg->from_repo; from != NULL; from = from->next) {
		mm_check(from->repo != NULL);

		url = from->repo->url;
		rv = download_from_repo(ctx, url, source_pkg_name,
		                        NULL, source_pkg_name);
		if (rv == 0)
			break;
	}

	if (rv == 0)
		info("Downloaded: %s\n", source_pkg_name);
	else
		error("Failed to download: %s\n", source_pkg_name);

	mmstr_free(source_pkg_name);
	return rv < 0 ? rv : 0;
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
	struct mmpkg const * pkg;

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
