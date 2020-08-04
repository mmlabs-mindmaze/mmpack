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
#include "settings.h"
#include "utils.h"


static
int download_repo_index(struct mmpack_ctx * ctx, const struct repo* repo)
{
	STATIC_CONST_MMSTR(pkglist, "binary-index");
	mmstr* path;
	int rv = -1;

	path = mmpack_get_repocache_path(ctx, repo->name);

	if (download_from_repo(ctx, repo->url, pkglist, NULL, path)) {
		error("Failed to download package list from %s (%s)\n",
		      repo->name, repo->url);

		goto exit;
	}

	rv = 0;
	info("Updated package list from repository: %s\n", repo->name);

exit:
	mmstr_free(path);
	return rv;
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
	struct repo_iter iter;
	struct repolist* list;
	const struct repo* r;

	if (mm_arg_is_completing())
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

	list = &ctx->settings.repo_list;
	if (repolist_num_repo(list) == 0) {
		info("No repository specified, nothing to update\n");
		return 0;
	}

	for (r = repo_iter_first(&iter, list); r; r = repo_iter_next(&iter))
		download_repo_index(ctx, r);

	return 0;
}
