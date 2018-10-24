/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "context.h"
#include "download.h"
#include "mmpack-update.h"
#include "mmstring.h"
#include "utils.h"


LOCAL_SYMBOL
int mmpack_update_all(struct mmpack_ctx * ctx)
{
	STATIC_CONST_MMSTR(cacheindex, REPO_INDEX_RELPATH)
	STATIC_CONST_MMSTR(pkglist, "binary-index")
	const mmstr* url = ctx->settings.repo_url;
	const mmstr* prefix = ctx->prefix;

	if (!url) {
		fprintf(stderr, "Repository URL unspecified\n");
		return -1;
	}

	if (download_from_repo(ctx, url, pkglist, prefix, cacheindex)) {
		fprintf(stderr, "Failed to download package list\n");
		return -1;
	}

	printf("Updated package list from repository: %s\n", url);

	return 0;
}
