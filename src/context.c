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

#include <mmerrno.h>
#include <mmlib.h>

#include "binary-index.h"
#include "common.h"
#include "context.h"
#include "indextable.h"
#include "mmstring.h"
#include "package-utils.h"
#include "settings.h"

static
int read_user_config(struct mmpack_ctx* ctx)
{
	mmstr* filename;
	int rv;

	filename = get_config_filename();
	rv = settings_load(&ctx->settings, filename);
	mmstr_free(filename);

	return rv;
}


static
int read_prefix_config(struct mmpack_ctx* ctx)
{
	STATIC_CONST_MMSTR(cfg_relpath, CFG_RELPATH)
	mmstr* filename;
	int rv, len;

	len = mmstrlen(ctx->prefix) + mmstrlen(cfg_relpath) + 1;
	filename = mmstr_malloca(len);

	mmstr_join_path(filename, ctx->prefix, cfg_relpath);
	rv = settings_load(&ctx->settings, filename);

	mmstr_freea(filename);
	return rv;
}


LOCAL_SYMBOL
int mmpack_ctx_init(struct mmpack_ctx * ctx, struct mmpack_opts* opts)
{
	const char* prefix;
	mmstr* default_prefix = get_default_mmpack_prefix();

	memset(ctx, 0, sizeof(*ctx));
	settings_init(&ctx->settings);
	if (read_user_config(ctx))
		return -1;

	indextable_init(&ctx->binindex, -1, -1);
	indextable_init(&ctx->installed, -1, -1);

	prefix = opts->prefix;
	if (!prefix)
		prefix = mm_getenv("MMPACK_PREFIX", default_prefix);

	ctx->prefix = mmstr_malloc_from_cstr(prefix);

	mmstr_free(default_prefix);
	return read_prefix_config(ctx);
}


LOCAL_SYMBOL
void mmpack_ctx_deinit(struct mmpack_ctx * ctx)
{
	struct it_iterator iter;
	struct it_entry * entry;

	mmstr_free(ctx->prefix);
	mmstr_free(ctx->pkgcachedir);

	if (ctx->curl != NULL) {
		curl_easy_cleanup(ctx->curl);
		ctx->curl = NULL;
	}

	entry = it_iter_first(&iter, &ctx->binindex);
	while (entry != NULL) {
		struct mmpkg * pkg = entry->value;
		mmpkg_destroy(pkg);
		entry = it_iter_next(&iter);
	}
	indextable_deinit(&ctx->binindex);
	indextable_deinit(&ctx->installed);

	settings_deinit(&ctx->settings);
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
	STATIC_CONST_MMSTR(inst_relpath, INSTALLED_INDEX_RELPATH)
	STATIC_CONST_MMSTR(repo_relpath, REPO_INDEX_RELPATH)
	mmstr* repo_index_path;
	mmstr* installed_index_path;
	int len;

	// Form the path of installed package from prefix
	len = mmstrlen(ctx->prefix) + mmstrlen(inst_relpath) + 1;
	installed_index_path = mmstr_alloca(len);
	mmstr_join_path(installed_index_path, ctx->prefix, inst_relpath);

	// Form the path of repo package list cache from prefix
	len = mmstrlen(ctx->prefix) + mmstrlen(repo_relpath) + 1;
	repo_index_path = mmstr_alloca(len);
	mmstr_join_path(repo_index_path, ctx->prefix, repo_relpath);

	// populate the package lists
	if (  installed_index_populate(ctx, installed_index_path)
	   || binary_index_populate(ctx, repo_index_path))
		return -1;

	return 0;
}


/**
 * mmpack_ctx_get_pkgcachedir() - get prefix dir where to download packages
 * @ctx:	initialized mmpack context
 *
 * Return: a mmstr pointer to the folder in prefix where to put downloaded
 * packages
 */
LOCAL_SYMBOL
const mmstr* mmpack_ctx_get_pkgcachedir(struct mmpack_ctx * ctx)
{
	STATIC_CONST_MMSTR(pkgcache_relpath, PKGS_CACHEDIR_RELPATH)
	int len;

	// If already computed, return it
	if (ctx->pkgcachedir)
		return ctx->pkgcachedir;

	len = mmstrlen(ctx->prefix) + mmstrlen(pkgcache_relpath) + 1;
	ctx->pkgcachedir = mmstr_malloc(len);
	mmstr_join_path(ctx->pkgcachedir, ctx->prefix, pkgcache_relpath);

	return ctx->pkgcachedir;
}
