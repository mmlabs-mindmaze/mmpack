/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <curl/curl.h>
#include <mmerrno.h>
#include <mmlib.h>
#include <mmsysio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "context.h"
#include "indextable.h"
#include "mmstring.h"
#include "package-utils.h"
#include "settings.h"

#define ALIAS_PREFIX_FOLDER "mmpack-prefix"

static
int load_user_config(struct mmpack_ctx* ctx)
{
	mmstr* filename;
	int rv;

	// Reset any previously loaded configuration
	settings_reset(&ctx->settings);

	filename = get_config_filename();
	rv = settings_load(&ctx->settings, filename);
	mmstr_free(filename);

	return rv;
}


static
int load_prefix_config(struct mmpack_ctx* ctx)
{
	STATIC_CONST_MMSTR(cfg_relpath, CFG_RELPATH);
	mmstr* filename;
	int rv, len;

	// Reset any previously loaded configuration
	settings_reset(&ctx->settings);

	len = mmstrlen(ctx->prefix) + mmstrlen(cfg_relpath) + 1;
	filename = mmstr_malloca(len);

	mmstr_join_path(filename, ctx->prefix, cfg_relpath);
	rv = settings_load(&ctx->settings, filename);

	mmstr_freea(filename);
	return rv;
}


/**
 * prefix_is_alias
 * @prefix: prefix path or name
 *
 * Returns: 0 if prefix is a path and 1 otherwise
 */
static
int prefix_is_alias(const char * prefix)
{
	int alias = 1;

	while (alias && *prefix != '\0') {
		if (is_path_separator(*prefix))
			alias = 0;

		prefix++;
	}

	return alias;
}


/**
 * mmpack_ctx_init() - initialize mmpack context
 * @ctx: mmpack context
 * @opts: init options
 *
 * The mmpack context must be cleansed by calling mmpack_ctx_deinit()
 *
 * Return: 0 on success, -1 on error
 */
LOCAL_SYMBOL
int mmpack_ctx_init(struct mmpack_ctx * ctx, struct mmpack_opts* opts)
{
	const char* prefix;
	char * prefix_path = NULL;
	mmstr * cwd;
	int len = 512, dir_strlen;

	memset(ctx, 0, sizeof(*ctx));
	settings_init(&ctx->settings);
	if (load_user_config(ctx))
		return -1;

	binindex_init(&ctx->binindex);
	install_state_init(&ctx->installed);

	prefix = opts->prefix;
	if (!prefix)
		prefix =
			mm_getenv("MMPACK_PREFIX",
			          ctx->settings.default_prefix);

	if (prefix && prefix_is_alias(prefix)) {
		dir_strlen = strlen(mm_get_basedir(MM_DATA_HOME));
		dir_strlen += strlen(ALIAS_PREFIX_FOLDER) + 2;
		dir_strlen += strlen(prefix) + 1;
		prefix_path = mm_malloca(sizeof(char)*(dir_strlen));
		sprintf(prefix_path, "%s/%s/%s",
		        mm_get_basedir(MM_DATA_HOME), ALIAS_PREFIX_FOLDER,
		        prefix);
		prefix = prefix_path;
	}

	if (prefix)
		ctx->prefix = mmstr_malloc_from_cstr(prefix);

	mm_freea(prefix_path);

	cwd = mmstr_malloc(len);
	while (!mm_getcwd(cwd, len)) {
		len = len * 2;
		mm_check(len > 0);
		cwd = mmstr_realloc(cwd, len);
	}

	ctx->cwd = cwd;
	mmstr_setlen(ctx->cwd, strlen(ctx->cwd));

	return 0;
}


/**
 * mmpack_ctx_deinit() - clean mmpack context
 * @ctx: mmpack context
 */
LOCAL_SYMBOL
void mmpack_ctx_deinit(struct mmpack_ctx * ctx)
{
	mmstr_free(ctx->prefix);
	mmstr_free(ctx->cwd);
	mmstr_free(ctx->pkgcachedir);

	if (ctx->curl != NULL) {
		curl_easy_cleanup(ctx->curl);
		ctx->curl = NULL;
		curl_global_cleanup();
	}

	binindex_deinit(&ctx->binindex);
	install_state_deinit(&ctx->installed);

	settings_deinit(&ctx->settings);
}


static
int set_installed(struct mmpkg* pkg, void * data)
{
	struct mmpack_ctx * ctx = data;

	install_state_add_pkg(&ctx->installed, pkg);
	pkg->state = MMPACK_PKG_INSTALLED;
	return 0;
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
	STATIC_CONST_MMSTR(inst_relpath, INSTALLED_INDEX_RELPATH);
	mmstr* repo_cache;
	mmstr* installed_index_path;
	int i, num_repo, len;
	struct repolist_elt * repo;
	int rv = -1;

	// Form the path of installed package from prefix
	len = mmstrlen(ctx->prefix) + mmstrlen(inst_relpath) + 1;
	installed_index_path = mmstr_malloca(len);
	mmstr_join_path(installed_index_path, ctx->prefix, inst_relpath);

	// populate the installed package list
	if (binindex_populate(&ctx->binindex, installed_index_path, NULL))
		goto exit;

	binindex_foreach(&ctx->binindex, set_installed, ctx);

	// populate the repository cached package list
	num_repo = settings_num_repo(&ctx->settings);
	for (i = 0; i < num_repo; i++) {
		repo = settings_get_repo(&ctx->settings, i);

		// discard repositories that are disable
		if (repo->enabled == 0)
			continue;

		repo_cache = mmpack_get_repocache_path(ctx, repo->name);
		if (binindex_populate(&ctx->binindex, repo_cache, repo))
			printf("Cache file of repository %s is missing, "
			       "updating may fix the issue\n", repo->name);

		mmstr_free(repo_cache);
	}

	binindex_compute_rdepends(&ctx->binindex);
	rv = 0;

exit:
	mmstr_freea(installed_index_path);

	if (rv != 0)
		error("Failed to load package lists\n");

	return rv;
}


LOCAL_SYMBOL
int mmpack_ctx_save_installed_list(struct mmpack_ctx * ctx)
{
	STATIC_CONST_MMSTR(inst_relpath, INSTALLED_INDEX_RELPATH);
	mmstr* installed_index_path;
	int len;
	FILE* fp;

	// Form the path of installed package from prefix
	len = mmstrlen(ctx->prefix) + mmstrlen(inst_relpath) + 1;
	installed_index_path = mmstr_malloca(len);
	mmstr_join_path(installed_index_path, ctx->prefix, inst_relpath);

	fp = fopen(installed_index_path, "wb");
	mmstr_freea(installed_index_path);

	if (!fp)
		return -1;

	install_state_save_to_index(&ctx->installed, fp);
	fclose(fp);
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
	STATIC_CONST_MMSTR(pkgcache_relpath, PKGS_CACHEDIR_RELPATH);
	int len;

	// If already computed, return it
	if (ctx->pkgcachedir)
		return ctx->pkgcachedir;

	len = mmstrlen(ctx->prefix) + mmstrlen(pkgcache_relpath) + 1;
	ctx->pkgcachedir = mmstr_malloc(len);
	mmstr_join_path(ctx->pkgcachedir, ctx->prefix, pkgcache_relpath);

	return ctx->pkgcachedir;
}


/**
 * mmpack_get_repocache_path() - get path in prefix of repo cache pkglist
 * @ctx:        initialized mmpack context
 * @repo_name: name of the repository
 *
 * Return: An allocated mmstr pointer to the file in prefix where the repository
 * cached package info is stored. It must be freed with mmstr_free() when it is
 * no longer needed.
 */
LOCAL_SYMBOL
mmstr* mmpack_get_repocache_path(struct mmpack_ctx * ctx, char * repo_name)
{
	STATIC_CONST_MMSTR(repo_relpath, REPO_INDEX_RELPATH);
	int len;
	mmstr * path;

	// Alloc string if not done yet
	len = mmstrlen(ctx->prefix) + mmstrlen(repo_relpath) +
	      strlen(repo_name) + 2;
	path = mmstr_malloc(len);

	// Form destination cache index basen in prefix
	mmstr_join_path(path, ctx->prefix, repo_relpath);

	// Append the name of the repo
	mmstrcat_cstr(path, ".");
	mmstrcat_cstr(path, repo_name);

	return path;
}


/**
 * mmpack_ctx_use_prefix_log() - redirect stderr to prefix log
 * @ctx:        initialized mmpack context
 *
 * Return: 0 in case of success, -1 otherwise
 */
static
int mmpack_ctx_use_prefix_log(struct mmpack_ctx * ctx)
{
	STATIC_CONST_MMSTR(log_relpath, LOG_RELPATH);
	int fd, oflags;

	oflags = O_WRONLY|O_CREAT|O_APPEND;
	fd = open_file_in_prefix(ctx->prefix, log_relpath, oflags);
	if (fd < 0)
		goto error;

	if (mm_dup2(fd, STDERR_FILENO) < 0)
		goto error;

	mm_close(fd);
	return 0;

error:
	mm_close(fd);
	error("Unable to redirect log to %s/%s", ctx->prefix, log_relpath);
	return -1;
}


/**
 * mmpack_ctx_use_prefix() - load prefix settings and packages indices
 * @ctx:	initialized mmpack context
 * @flags:      OR combination of 0 and CTX_SKIP_* values
 *
 * If flags is set to CTX_SKIP_PKGLIST, the function will not try to load
 * package list.
 *
 * If flags is set to CTX_SKIP_REDIRECT_LOG, the function will not redirect
 * the error log.
 *
 * Return: 0 in case of success, -1 otherwise
 */
LOCAL_SYMBOL
int mmpack_ctx_use_prefix(struct mmpack_ctx * ctx, int flags)
{
	if (ctx->prefix == NULL)
		return -1;

	if (mm_check_access(ctx->prefix, F_OK)) {
		fprintf(stderr, "Fatal: \"%s\" does not exist\n", ctx->prefix);
		fprintf(stdout,
		        "To create it, type:\n\t mmpack mkprefix %s\n",
		        ctx->prefix);
		return -1;
	}

	if (load_prefix_config(ctx))
		return -1;

	if (!(flags & CTX_SKIP_REDIRECT_LOG)
	    && mmpack_ctx_use_prefix_log(ctx))
		return -1;

	if (flags & CTX_SKIP_PKGLIST)
		return 0;

	return mmpack_ctx_init_pkglist(ctx);
}
