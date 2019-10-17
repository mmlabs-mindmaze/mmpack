/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmargparse.h>

#include "context.h"
#include "download.h"
#include "mmpack-update.h"
#include "mmstring.h"
#include "utils.h"


static
int download_repo_index(struct mmpack_ctx * ctx, int repo_index)
{
	STATIC_CONST_MMSTR(pkglist, "binary-index");
	const mmstr* cacheindex;
	struct repolist_elt * repo;

	repo = settings_get_repo(&ctx->settings, repo_index);

	cacheindex = mmpack_ctx_get_cache_index(ctx, repo_index);

	if (download_from_repo(ctx, repo->url, pkglist, NULL,
	                       cacheindex)) {

		error("Failed to download package list from %s (%s)\n",
		      repo->name, repo->url);

		return -1;
	}

	info("Updated package list from repository: %s\n", repo->name);

	return 0;
}


/**
 * mmpack_update_all() - main function for the update command
 * @ctx: mmpack context
 * @argc: number of arguments
 * @argv: array of arguments
 *
 * update local package list from repository list in config file.
 *
 * Return: 0 on success, -1 otherwise
 */
LOCAL_SYMBOL
int mmpack_update_all(struct mmpack_ctx * ctx, int argc, char const ** argv)
{
	int i, num_repo;

	if (mmarg_is_completing())
		return 0;

	if (argc == 2
	    && (STR_EQUAL(argv[1], strlen(argv[1]), "--help")
	        || STR_EQUAL(argv[1], strlen(argv[1]), "-h"))) {
		fprintf(stderr, "Usage:\n\tmmpack "UPDATE_SYNOPSIS "\n");
		return 0;
	}

	// Load prefix configuration
	if (mmpack_ctx_use_prefix(ctx, CTX_SKIP_PKGLIST))
		return -1;

	num_repo = settings_num_repo(&ctx->settings);
	if (num_repo == 0) {
		error("Repository URL unspecified\n");
		return -1;
	}

	for (i = 0; i < num_repo; i++) {
		download_repo_index(ctx, i);
	}

	return 0;
}
