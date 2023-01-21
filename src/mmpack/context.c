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
#include <mmtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "context.h"
#include "hashset.h"
#include "mmstring.h"
#include "package-utils.h"
#include "prefix-list.h"
#include "repo.h"
#include "settings.h"
#include "strset.h"
#include "utils.h"

#define ALIAS_PREFIX_FOLDER "mmpack/prefix"
#define CACHE_KEEP_TIME_SEC (7*24*60*60)        // 7 days

static
int load_user_config(struct mmpack_ctx* ctx)
{
	mmstr* filename;
	int rv;

	// Reset any previously loaded configuration
	settings_reset(&ctx->settings);

	filename = get_xdg_subpath(MM_CONFIG_HOME, "mmpack-config.yaml");
	rv = settings_load(&ctx->settings, filename);
	mmstr_free(filename);

	return rv;
}


static
int load_prefix_config(struct mmpack_ctx* ctx)
{
	STATIC_CONST_MMSTR(cfg_relpath, CFG_RELPATH);
	mmstr* filename;
	int rv, len, bval;
	const char* val;

	// Reset any previously loaded configuration
	settings_reset(&ctx->settings);

	len = mmstrlen(ctx->prefix) + mmstrlen(cfg_relpath) + 1;
	filename = mmstr_malloca(len);

	mmstr_join_path(filename, ctx->prefix, cfg_relpath);
	rv = settings_load(&ctx->settings, filename);

	mmstr_freea(filename);

	val = mm_getenv("MMPACK_DISABLE_IMPORT_OTHER_PREFIX", "no");
	if (strchunk_parse_bool(&bval, (struct strchunk) {val, strlen(val)}))
		return -1;

	ctx->flags |= bval ? CTX_DISABLE_IMPORT_OTHER_PREFIX : 0;

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
 * cleanup_cachedir() - remove unused files from cache dir
 * @cachedir:   path to cache directory
 * @keep_time_sec: time after which a file is considered unused
 */
static
void cleanup_cachedir(const mmstr* cachedir, int keep_time_sec)
{
	MM_DIR* dir;
	const struct mm_dirent* entry;
	struct mm_stat buf;
	struct mm_timespec now;
	char* prev_cwd = NULL;

	mm_gettime(MM_CLK_REALTIME, &now);

	// Change current directory to target_dir (and store previous one)
	if ((prev_cwd = mm_getcwd(NULL, 0)) == NULL
	    || mm_chdir(cachedir)) {
		free(prev_cwd);
		return;
	}

	// Loop in cachedir and remove unused files
	dir = mm_opendir(".");
	while ((entry = mm_readdir(dir, NULL))) {
		if (entry->type != MM_DT_REG
		    || mm_stat(entry->name, &buf, MM_NOFOLLOW)
		    || (now.tv_sec - buf.atime) > keep_time_sec
		    || mm_unlink(entry->name))
			continue;
	}
	mm_closedir(dir);

	mm_chdir(prev_cwd);
	free(prev_cwd);
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
	mmstr * cwd;
	int len = 512;

	memset(ctx, 0, sizeof(*ctx));
	settings_init(&ctx->settings);
	if (load_user_config(ctx))
		return -1;

	srcindex_init(&ctx->srcindex);
	binindex_init(&ctx->binindex);
	install_state_init(&ctx->installed);

	strset_init(&ctx->manually_inst, STRSET_HANDLE_STRINGS_MEM);

	prefix = opts->prefix;
	if (!prefix)
		prefix =
			mm_getenv("MMPACK_PREFIX",
			          ctx->settings.default_prefix);

	// Store actual prefix path in context if prefix set.
	if (prefix && prefix_is_alias(prefix)) {
		ctx->prefix = mmstr_asprintf(NULL,
		                             "%s/"ALIAS_PREFIX_FOLDER "/%s",
		                             mm_get_basedir(MM_DATA_HOME),
		                             prefix);
	} else if (prefix) {
		ctx->prefix = mmstr_malloc_from_cstr(prefix);
	}

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

	if (ctx->pkgcachedir) {
		cleanup_cachedir(ctx->pkgcachedir, CACHE_KEEP_TIME_SEC);
		mmstr_free(ctx->pkgcachedir);
	}

	if (ctx->curl != NULL) {
		curl_easy_cleanup(ctx->curl);
		ctx->curl = NULL;
		curl_global_cleanup();
	}

	binindex_deinit(&ctx->binindex);
	srcindex_deinit(&ctx->srcindex);
	install_state_deinit(&ctx->installed);
	strset_deinit(&ctx->manually_inst);
	settings_deinit(&ctx->settings);
}


static
void mmpack_ctx_populate_from_repo(struct mmpack_ctx * ctx,
                                   const struct repo* repo)
{
	STATIC_CONST_MMSTR(binpath, REPO_INDEX_RELPATH);
	STATIC_CONST_MMSTR(srcpath, SRC_INDEX_RELPATH);
	mmstr* binindex_cache;
	mmstr* srcindex_cache;

	binindex_cache = mmpack_repo_cachepath(ctx, repo->name, binpath);
	srcindex_cache = mmpack_repo_cachepath(ctx, repo->name, srcpath);
	if (binindex_populate(&ctx->binindex, binindex_cache, repo)
	    || srcindex_populate(&ctx->srcindex, srcindex_cache, repo))
		printf("Cache file of repository %s is missing, "
		       "updating may fix the issue\n", repo->name);

	mmstr_free(binindex_cache);
	mmstr_free(srcindex_cache);
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
	mmstr* installed_index_path;
	int len;
	struct repo_iter iter;
	const struct repo* repo;
	int rv = -1;
	struct pkg_iter pkg_iter;
	struct binpkg* pkg;

	// Form the path of installed package from prefix
	len = mmstrlen(ctx->prefix) + mmstrlen(inst_relpath) + 1;
	installed_index_path = mmstr_malloca(len);
	mmstr_join_path(installed_index_path, ctx->prefix, inst_relpath);

	// populate the installed package list
	if (binindex_populate(&ctx->binindex, installed_index_path, NULL))
		goto exit;

	// Add package added from installed_index to installed_state
	pkg = pkg_iter_first(&pkg_iter, &ctx->binindex);
	for (; pkg != NULL; pkg = pkg_iter_next(&pkg_iter))
		install_state_add_pkg(&ctx->installed, pkg);

	// populate the repository cached package list
	repo = repo_iter_first(&iter, &ctx->settings.repo_list);
	for (; repo != NULL; repo = repo_iter_next(&iter)) {
		// discard repositories that are disable
		if (repo->enabled)
			mmpack_ctx_populate_from_repo(ctx, repo);
	}

	binindex_compute_rdepends(&ctx->binindex);
	rv = 0;

exit:
	mmstr_freea(installed_index_path);

	if (rv != 0)
		error("Failed to load package lists\n");

	return rv;
}


/**
 * load_manually_installed() - loads the name list of the manually installed
 *                             packages
 * @prefix: prefix
 * @manually_inst: name set of manually installed packages
 *
 * The file from which the names are read is
 * prefix/var/lib/mmpack/manually_installed.yaml
 *
 * Return: 0 in case of success, -1 otherwise
 */
static
int load_manually_installed(const mmstr * prefix, struct strset * manually_inst)
{
	STATIC_CONST_MMSTR(manually_inst_relpath, MANUALLY_INST_RELPATH);
	struct strchunk line, data_to_parse;
	mmstr* pkg_name = NULL;
	void* map = NULL;
	size_t mapsize;

	if (map_file_in_prefix(prefix, manually_inst_relpath, &map, &mapsize))
		return -1;

	data_to_parse = (struct strchunk) {.buf = map, .len = mapsize};
	while (data_to_parse.len != 0) {
		line = strchunk_getline(&data_to_parse);
		pkg_name = mmstr_copy_realloc(pkg_name, line.buf, line.len);
		strset_add(manually_inst, pkg_name);
	}

	mm_unmap(map);
	mmstr_free(pkg_name);
	return 0;
}


/**
 * save_manually_installed() - dump the name list of the manually installed
 *                             packages
 * @prefix: prefix
 * @manually_inst: name set of manually installed packages
 *
 * The file where the names are dumped is
 * prefix/var/lib/mmpack/manually_installed.yaml
 *
 * Return: 0 in case of success, -1 otherwise
 */
static
int save_manually_installed(const mmstr * prefix, struct strset * manually_inst)
{
	struct strset_iterator iter;
	mmstr * curr;
	int fd, oflag;
	char eol = '\n';

	STATIC_CONST_MMSTR(manually_inst_relpath, MANUALLY_INST_RELPATH);

	oflag = O_WRONLY|O_TRUNC;
	fd = open_file_in_prefix(prefix, manually_inst_relpath, oflag);
	if (fd == -1)
		return -1;

	for (curr = strset_iter_first(&iter, manually_inst); curr;
	     curr = strset_iter_next(&iter)) {
		if (mm_write(fd, curr, mmstrlen(curr)) == -1
		    || mm_write(fd, &eol, sizeof(eol)) == -1) {
			return -1;
		}
	}

	mm_close(fd);
	return 0;
}


static
int mmpack_ctx_save_installed_hashset(struct mmpack_ctx * ctx)
{
	mmstr* path;
	mmstr* tmp;
	int npkg, rv = 0;
	const struct binpkg* pkg;
	struct inststate_iter iter;
	struct buffer buff = {0};

	path = mmstr_asprintf(NULL, "%s/"HASHSET_RELPATH, ctx->prefix);
	tmp = mmstr_tmppath_from_path(NULL, path, 0);

	// Create array of sumsha all install packages in buffer
	pkg = inststate_first(&iter, &ctx->installed);
	for (; pkg != NULL; pkg = inststate_next(&iter))
		buffer_push(&buff, &pkg->sumsha, sizeof(pkg->sumsha));

	// Atomically update hashset
	npkg = buff.size / sizeof(pkg->sumsha);
	if (create_hashset(tmp, npkg, buff.base)
	    || mm_rename(tmp, path))
		rv = -1;

	buffer_deinit(&buff);
	mmstr_free(tmp);
	mmstr_free(path);

	if (rv == 0)
		rv = update_prefix_list_with_prefix(ctx->prefix);

	return rv;
}


LOCAL_SYMBOL
int mmpack_ctx_save_installed_list(struct mmpack_ctx * ctx)
{
	STATIC_CONST_MMSTR(inst_relpath, INSTALLED_INDEX_RELPATH);
	mmstr* installed_index_path;
	struct buffer buff = {0};
	int len, rv;

	if (save_manually_installed(ctx->prefix, &ctx->manually_inst))
		return -1;

	// Form the path of installed package from prefix
	len = mmstrlen(ctx->prefix) + mmstrlen(inst_relpath) + 1;
	installed_index_path = mmstr_malloca(len);
	mmstr_join_path(installed_index_path, ctx->prefix, inst_relpath);

	install_state_save_to_buffer(&ctx->installed, &buff);
	rv = save_compressed_file(installed_index_path, &buff);
	buffer_deinit(&buff);

	mmstr_freea(installed_index_path);

	if (rv == 0)
		rv = mmpack_ctx_save_installed_hashset(ctx);

	return rv;
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
	mmstr* cachedir = ctx->pkgcachedir;

	// If already computed, return it
	if (cachedir)
		return cachedir;

	ctx->pkgcachedir = get_xdg_subpath(MM_CACHE_HOME, "mmpack/pkgs");
	return ctx->pkgcachedir;
}


/**
 * mmpack_repo_cachepath() - Function that computes the path of a given file.
 * ctx: initialized mmpack context
 * repo_name: name of the repository where the index file is located
 * relpath: relative path of the file
 *
 * Return: the path of the file @relpath in the repository @repo_name.
 */
LOCAL_SYMBOL
mmstr* mmpack_repo_cachepath(struct mmpack_ctx* ctx, const char* repo_name,
                             const char* relpath)
{
	int len;
	mmstr * path;

	len = mmstrlen(ctx->prefix) + mmstrlen(relpath) +
	      strlen(repo_name) + 2;
	path = mmstr_malloc(len);

	// Form destination cache index basen in prefix
	mmstr_join_path(path, ctx->prefix, relpath);

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

	if (load_manually_installed(ctx->prefix, &ctx->manually_inst))
		return -1;

	if (!(flags & CTX_SKIP_REDIRECT_LOG)
	    && mmpack_ctx_use_prefix_log(ctx))
		return -1;

	if (flags & CTX_SKIP_PKGLIST)
		return 0;

	return mmpack_ctx_init_pkglist(ctx);
}
