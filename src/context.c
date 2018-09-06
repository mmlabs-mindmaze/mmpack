/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmlib.h>
#include <mmsysio.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

#include <mmerrno.h>
#include <mmlib.h>

#include "binary-index.h"
#include "common.h"
#include "context.h"
#include "indextable.h"
#include "mmstring.h"
#include "package-utils.h"


LOCAL_SYMBOL
int mmpack_ctx_init(struct mmpack_ctx * ctx, struct mmpack_opts* opts)
{
	const char* prefix;
	char* default_prefix = (char*)get_default_mmpack_prefix();

	memset(ctx, 0, sizeof(*ctx));

	if (!yaml_parser_initialize(&ctx->parser))
		return mm_raise_error(ENOMEM, "failed to init yaml parse");

	indextable_init(&ctx->binindex, -1, -1);
	indextable_init(&ctx->installed, -1, -1);

	prefix = opts->prefix;
	if (!prefix)
		prefix = mm_getenv("MMPACK_PREFIX", default_prefix);

	ctx->prefix = mmstr_malloc_from_cstr(prefix);

	free(default_prefix);
	return 0;
}


LOCAL_SYMBOL
void mmpack_ctx_deinit(struct mmpack_ctx * ctx)
{
	struct it_iterator iter;
	struct it_entry * entry;

	mmstr_free(ctx->prefix);

	if (ctx->curl != NULL) {
		curl_easy_cleanup(ctx->curl);
		ctx->curl = NULL;
	}

	yaml_parser_delete(&ctx->parser);

	entry = it_iter_first(&iter, &ctx->binindex);
	while (entry != NULL) {
		struct mmpkg * pkg = entry->value;
		mmpkg_destroy(pkg);
		entry = it_iter_next(&iter);
	}
	indextable_deinit(&ctx->binindex);

	entry = it_iter_first(&iter, &ctx->installed);
	while (entry != NULL) {
		struct mmpkg * pkg = entry->value;
		mmpkg_destroy(pkg);
		entry = it_iter_next(&iter);
	}
	indextable_deinit(&ctx->installed);
}


/**
 * mmpack_ctx_init_pkglist() - parse repo cache and installed package list
 * @ctx:        initialized mmpack-context
 *
 * This inspect the prefix path set at init, parse the cache of repo
 * package list and installed package list.
 *
 * Return: 0 in case of success, -1 otherwise
 */
LOCAL_SYMBOL
int mmpack_ctx_init_pkglist(struct mmpack_ctx * ctx)
{
	mmstr* repo_index_path;
	mmstr* installed_index_path;
	size_t prefixlen = mmstrlen(ctx->prefix);

	// Form the path of installed package from prefix
	installed_index_path = mmstr_alloca(prefixlen + sizeof(INSTALLED_INDEX_RELPATH));
	mmstrcpy(installed_index_path, ctx->prefix);
	mmstrcat_cstr(installed_index_path, INSTALLED_INDEX_RELPATH);

	// Form the path of repo package list cache from prefix
	repo_index_path = mmstr_alloca(prefixlen + sizeof(REPO_INDEX_RELPATH));
	mmstrcpy(repo_index_path, ctx->prefix);
	mmstrcat_cstr(repo_index_path, REPO_INDEX_RELPATH);

	// populate the package lists
	if (  installed_index_populate(ctx, installed_index_path)
	   || binary_index_populate(ctx, repo_index_path))
		return -1;

	return 0;
}
