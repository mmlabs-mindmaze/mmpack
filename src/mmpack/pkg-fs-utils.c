/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <assert.h>
#include <mmsysio.h>
#include <mmlib.h>
#include <mmerrno.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"
#include "common.h"
#include "context.h"
#include "crypto.h"
#include "download.h"
#include "mmstring.h"
#include "package-utils.h"
#include "pkg-fs-utils.h"
#include "strlist.h"
#include "strset.h"
#include "utils.h"
#include "sumsha.h"
#include "sysdeps.h"
#include "tar.h"


/**
 * struct fschange - data carried over file system actions
 * ctx: mmpack prefix context
 * curr_action: pointer to action being applied
 * inst_files:  list of files being installed
 * rm_files:    list of files being removed
 * rm_dirs:     set of folders to try to remove at the end of stack application
 * py_scripts:  set of installed pyscripts
 *
 * This structure holds data that needs to be shared over the installation,
 * removal or upgrade of the different packages when applying the actions
 * stack.
 */
struct fschange {
	struct mmpack_ctx* ctx;
	struct action* curr_action;
	struct strlist inst_files;
	struct strlist rm_files;
	struct strset rm_dirs;
	struct strset py_scripts;
};


/**
 * split_path() - split path into, parent folder, base and extension components
 * @path:       mmstr* pointer to path to split
 * @base:       pointer to struct strchunk receiving the base without extension
 * @ext:        pointer to struct strchunk receiving the extension
 *
 * Split @path into <dir>/<base.><ext>
 *
 * Return: struct strchunk holding the parent folder component
 */
static
struct strchunk split_path(const mmstr* path,
                           struct strchunk * base,
                           struct strchunk * ext)
{
	int basepos, extpos;
	struct strchunk filename;
	struct strchunk path_chunk = {
		.buf = path,
		.len = mmstrlen(path)
	};

	basepos = strchunk_rfind(path_chunk, '/');
	filename = strchunk_rpart(path_chunk, basepos);

	extpos = strchunk_rfind(filename, '.');
	*ext = strchunk_rpart(filename, extpos);
	*base = strchunk_lpart(filename, extpos + 1);

	return strchunk_lpart(path_chunk, basepos);
}


/**************************************************************************
 *                                                                        *
 *                      Packages files unpacking                          *
 *                                                                        *
 **************************************************************************/


/**
 * is_mmpack_metadata() - given file is an internal mmpack metadata
 * @path: full path to the file
 *
 * Return: 1 if the file is an internal mmpack metadata, 0 otherwise
 */
LOCAL_SYMBOL
int is_mmpack_metadata(mmstr const * path)
{
	return STR_STARTS_WITH(path, (size_t) mmstrlen(path), "MMPACK");
}


/**
 * fschange_move_instfiles() - Move files from unpackdir to final location
 * @fsc:          file system change data
 * @unpackdir:    path where the source file can be found
 *
 * In order for the install and upgrade commands to be atomic, the extraction is
 * done in two steps: first all the regular files and symlink are extracted in a
 * temporary directory (in var/cache/unpack), then they are all renamed, to be
 * placed into their final directories. Note that the directories are not
 * extracted during initial tar extraction, hence must be created if not
 * existing yet.
 *
 * The current function permits to rename the regular and symlink files.
 *
 * Return: 0 on success, a negative value otherwise.
 */
static
int fschange_move_instfiles(struct fschange* fsc, const char* unpackdir)
{
	mmstr* path = mmstr_malloc(32);
	const mmstr* d;
	struct strlist_elt* curr;
	struct strset set;
	struct strset_iterator it;
	int cpt, rv = -1;

	strset_init(&set, STRSET_HANDLE_STRINGS_MEM);

	// Drop files being installed from file planned to be remove (in case
	// of upgrade) and collect target directories in a set (deduplicate).
	for (curr = fsc->inst_files.head; curr; curr = curr->next) {
		strlist_remove(&fsc->rm_files, curr->str.buf);
		path = mmstr_dirname(path, curr->str.buf);
		strset_add(&set, path);
	}

	// Create collected target folders (with parents if needed)
	for (d = strset_iter_first(&it, &set); d; d = strset_iter_next(&it)) {
		if (mm_mkdir(d, 0777, MM_RECURSIVE))
			goto exit;
	}

	// Move file from unpackdir to final location
	cpt = 0;
	for (curr = fsc->inst_files.head; curr; curr = curr->next) {
		path = mmstr_asprintf(path, "%s/%d", unpackdir, cpt++);
		if (mm_rename(path, curr->str.buf))
			goto exit;
	}

	rv = 0;

exit:
	strset_deinit(&set);
	mmstr_free(path);
	return rv;
}


/**
 * fschange_unpack_mpk() - extract mpk file to a specified temporary folder
 * @fsc:          file system change data
 * @mpk_filename: filename of the downloaded package file
 * @unpackdir:    path where the file in mpk must be extracted
 *
 * This extracts the files from mpk package located at @mpk_filename and store
 * them in @unpackdir, named after the rank they appear in the tarball (first
 * will be named @unpackdir/0, second @unpackdir/1, ...). @fsc->inst_files is
 * filled at the same time with the path where they should be located
 * eventually after package installation.
 *
 * Return: 0 in case of success, -1 otherwise with error state set accordingly.
 */
static
int fschange_unpack_mpk(struct fschange* fsc, const char* mpk_filename, const char* unpackdir)
{
	struct tarstream tar;
	mmstr* path = NULL;
	int rv = 0;
	int cpt = 0;

	if (tarstream_open(&tar, mpk_filename))
		return -1;

	// Loop over each entry in the archive and process them
	while (!rv && ((rv = tarstream_read_next(&tar)) == 0)) {

		// Obtain the pathname (with leading "./" stripped) of the
		// file being extracted and skip metadata (MMPACK/*)
		path = mmstrcpy_cstr_realloc(path, tar.entry_path+2);
		if (!mmstrlen(path) || is_mmpack_metadata(path))
			continue;

		// If not a directory, copy extracted files in a temporary
		// folder. They will be moved to final destination once the
		// whole package has been extracted.
		if (tar.entry_type == AE_IFDIR)
			continue;

		strlist_add(&fsc->inst_files, path);

		path = mmstr_asprintf(path, "%s/%d", unpackdir, cpt++);
		rv = tarstream_extract(&tar, path);
	}

	// Cleanup
	tarstream_close(&tar);
	mmstr_free(path);

	return (rv == READ_ARCHIVE_EOF) ? 0 : -1;
}


/**
 * fschange_pkg_unpack() - extract files of a given package
 * @fsc:        file system change data
 * @mpk_filename: filename of the downloaded package file
 *
 * In order the install and upgrade commands to be atomic, the extraction is
 * done in two steps: first all the regular files and symlink are extracted in a
 * temporary directory (in var/cache/upack), then they are all renamed, to be
 * placed in the good directory. Note that the directories of the package are
 * extracted directly in the good directory during the first step.
 *
 * Return: 0 on success, a negative value otherwise.
 */
static
int fschange_pkg_unpack(struct fschange* fsc, const char* mpk_filename)
{
	const char* unpackdir;

	unpackdir = UNPACK_CACHEDIR_RELPATH;
	if (fschange_unpack_mpk(fsc, mpk_filename, unpackdir))
		return -1;

	return fschange_move_instfiles(fsc, unpackdir);
}


static
void fschange_check_installed_pyscripts(struct fschange* fsc)
{
	struct strlist_elt* elt;
	struct strchunk base, ext;
	const mmstr* path;

	for (elt = fsc->inst_files.head; elt != NULL; elt = elt->next) {
		path = elt->str.buf;

		// Extract path component and test for .py file
		split_path(path, &base, &ext);
		if (strncmp(ext.buf, "py", ext.len) == 0) {
			strset_add(&fsc->py_scripts, elt->str.buf);
		}
	}
}


static
void fschange_compile_pyscripts(struct fschange* fsc)
{
	struct mm_remap_fd fdmap;
	int pipe_fds[2];
	mm_pid_t pid;
	mmstr* script;
	struct strset_iterator it;
	int num_item = fsc->py_scripts.num_item;
	char* argv[] = {
		"python3",
		"-m", "compileall",
		"-l",
		"-q",
		"-i", "-",
		NULL
	};

	if (!num_item)
		return;

	// Execute external command with STDIN connected to a pipe
	mm_pipe(pipe_fds);
	fdmap.parent_fd = pipe_fds[0];
	fdmap.child_fd = STDIN_FILENO;
	if (mm_spawn(&pid, argv[0], 1, &fdmap, 0, argv, NULL)) {
		mm_close(pipe_fds[0]);
		mm_close(pipe_fds[1]);
		return;
	}

	// Push each python scripts to pipe for being compiled
	script = strset_iter_first(&it, &fsc->py_scripts);
	for (; script != NULL; script = strset_iter_next(&it)) {
		mm_write(pipe_fds[1], script, mmstrlen(script));
		mm_write(pipe_fds[1], "\n", 1);
	}

	mm_close(pipe_fds[0]);
	mm_close(pipe_fds[1]);
	mm_wait_process(pid, NULL);
}


static
int fschange_preinst(struct fschange* fsc,
                     const struct binpkg* old, const struct binpkg* pkg)
{
	(void) fsc;
	(void) pkg;
	(void) old;

	return 0;
}


static
int fschange_postinst(struct fschange* fsc,
                      const struct binpkg* old, const struct binpkg* pkg)
{
	(void) pkg;
	(void) old;

	fschange_check_installed_pyscripts(fsc);
	return 0;
}


/**************************************************************************
 *                                                                        *
 *                         Package files extraction                       *
 *                                                                        *
 **************************************************************************/

/**
 * metadata_read_value() - extract value from metadata file in buffer
 * @metadata_buffer:    buffer holding content of metadata file
 * @key:                string holding the key to search
 * @out:                pointer to mmstring receiving the result
 *
 * Returns: 0 in case of success, -1 otherwise with error state set
 */
static
int metadata_read_value(const struct buffer* metadata_buffer, const char* key,
                        mmstr** out)
{
	int pos;
	struct strchunk line, lkey, lval;
	struct strchunk remaining = strchunk_from_buffer(metadata_buffer);

	while (1) {
		line = strchunk_getline(&remaining);

		// Extract key, value
		pos = strchunk_rfind(line, ':');
		lkey = strchunk_strip(strchunk_lpart(line, pos));
		if (strchunk_equal(lkey, key)) {
			lval = strchunk_strip(strchunk_rpart(line, pos));
			*out = mmstr_copy_realloc(*out, lval.buf, lval.len);
			return 0;
		}
	}

	mm_raise_error(MM_EBADFMT, "Could not find key %s", key);
	return -1;
}


/**
 * pkg_load_pkginfo() - read pkginfo from package into buffer
 * @mpk_filename: mmpack package to read from
 * @buffer: buffer structure to receive the raw data
 *
 * Open, scans for the metadata and then for pkginfo file, and load its data
 * into given buffer structure. The buffer will be enlarged as needed, and must
 * be freed by the caller after usage by calling the buffer_deinit() function.
 *
 * Return: 0 on success, -1 on error
 */
LOCAL_SYMBOL
int pkg_load_pkginfo(const char* mpk_filename, struct buffer * buffer)
{
	struct buffer metadata;
	mmstr* value = NULL;
	int rv = 0;
	char* line;

	buffer_init(&metadata);

	if (tar_load_file(mpk_filename, "./MMPACK/metadata", &metadata)
	    || metadata_read_value(&metadata, "pkginfo-path", &value)
	    || tar_load_file(mpk_filename, value, buffer)
	    || metadata_read_value(&metadata, "sumsha256sums", &value)) {
		rv = -1;
		goto exit;
	}

	// Append sumsha256sums field to buffer
	const char line_fmt[] = "sumsha256sums: %s\n";
	line = buffer_reserve_data(buffer, sizeof(line_fmt) + mmstrlen(value));
	buffer_inc_size(buffer, sprintf(line, line_fmt, value));

exit:
	mmstr_free(value);
	buffer_deinit(&metadata);
	return rv;
}


/**************************************************************************
 *                                                                        *
 *                          Packages files removal                        *
 *                                                                        *
 **************************************************************************/

/**
 * fschange_list_pkg_rm_files() - list files of a package to be removed
 * @fsc:        file system change data
 * @pkg:        package about to be removed
 *
 * @fsc->rm_files is updated with the list of files of package to be removed.
 *
 * Return: 0 in case of success, -1 otherwise with error
 */
static
int fschange_list_pkg_rm_files(struct fschange* fsc, const struct binpkg* pkg)
{
	int rv;
	mmstr* path;

	path = sha256sums_path(NULL, pkg);

	strlist_add(&fsc->rm_files, path);
	rv = read_sumsha_filelist(path, &fsc->rm_files);

	mmstr_free(path);
	return rv;
}


static
int fschange_apply_rm_files_list(struct fschange* fsc)
{
	struct strlist_elt * elt;
	const mmstr* path;

	for (elt = fsc->rm_files.head; elt != NULL; elt = elt->next) {
		path = elt->str.buf;
		if (mm_unlink(path)) {
			// If this has failed because the file is not found,
			// nothing prevent us to continue (maybe the user
			// has removed the file by mistake... but this
			// should not block). If something else, let's halt
			if (mm_get_lasterror_number() != ENOENT)
				return -1;
		}
	}

	return 0;
}


/**
 * fschange_update_rmdirs() - add parent dirs of path to rm_dirs set
 * @fsc:        file system change data
 * @path:       path of removed file
 *
 * NOTE: this alters the contents of @fsc->rm_list elements
 */
static
void fschange_update_rm_dirs(struct fschange* fsc)
{
	STATIC_CONST_MMSTR(prefix_root, ".");
	mmstr* dirpath;
	struct strlist_elt* elt;

	for (elt = fsc->rm_files.head; elt != NULL; elt = elt->next) {
		dirpath = elt->str.buf;
		do {
			// Change dirpath inplace to get the parent directory
			mmstr_setlen(dirpath, mm_dirname(dirpath, dirpath));
			if (mmstrequal(dirpath, prefix_root))
				break;

			// Try to add path to the set. If it did not get added,
			// it is already present, hence the parent dir doesn't
			// need to be processed
		} while (strset_add(&fsc->rm_dirs, dirpath));
	}
}


static
int reverse_mmstr_cmp(const void* s1, const void* s2)
{
	return mmstrcmp(*(mmstr* const*) s2, *(mmstr* const*) s1);
}


static
void fschange_apply_rm_dirs(struct fschange* fsc)
{
	struct strset_iterator it;
	const mmstr* dir;
	const mmstr** dirs;
	int i, numdir, previous;

	numdir = fsc->rm_dirs.num_item;
	dirs = xx_malloca(numdir * sizeof(*dirs));

	// Copy the elements of the set into the dirs array
	dir = strset_iter_first(&it, &fsc->rm_dirs);
	for (i = 0; dir != NULL; dir = strset_iter_next(&it), i++)
		dirs[i] = dir;

	// Get the sorted array of directories to try to remove in the reverse
	// order. This makes leaves of file hierarchy removed first
	qsort(dirs, numdir, sizeof(*dirs), reverse_mmstr_cmp);

	// Try to remove all listed directories, but make any error silent.
	previous = mm_error_set_flags(MM_ERROR_SET, MM_ERROR_NOLOG);
	for (i = 0; i < numdir; i++)
		mm_rmdir(dirs[i]);

	mm_error_set_flags(previous, MM_ERROR_NOLOG);

	mm_freea(dirs);
}


static
void fschange_remove_rmfiles_pycache(struct fschange* fsc)
{
	STATIC_CONST_MMSTR(pycache_subdir, "/__pycache__/");
	struct strlist_elt* elt;
	const struct mm_dirent* f;
	mmstr* cachedir = NULL;
	mmstr* cache = NULL;
	struct strchunk base, dir, ext;
	int cachedirlen;
	MM_DIR* d;

	// Scan all files listed for removal
	for (elt = fsc->rm_files.head; elt != NULL; elt = elt->next) {
		// Extract path component and skip if not .py file
		dir = split_path(elt->str.buf, &base, &ext);
		if (strncmp(ext.buf, "py", ext.len) != 0)
			continue;

		// Remove file for compilation if it has been installed  from
		// a previous package installation or upgrade
		strset_remove(&fsc->py_scripts, elt->str.buf);

		// Set cachedir to cache folder of pyfile
		cachedir = mmstr_copy_realloc(cachedir, dir.buf, dir.len);
		cachedir = mmstrcat_realloc(cachedir, pycache_subdir);

		// Add cachedir to the set of folders to be removed
		strset_add(&fsc->rm_dirs, cachedir);

		// Prepare cache filename to starts with cachedir
		cache = mmstrcpy_realloc(cache, cachedir);
		cachedirlen = mmstrlen(cachedir);

		d = mm_opendir(cachedir);
		while ((f = mm_readdir(d, NULL))) {
			// skip if file in cache path is not cache of the
			// python source script
			if (strncmp(base.buf, f->name, base.len))
				continue;

			// Set basename of the found cache file to cache string
			mmstr_setlen(cache, cachedirlen);
			cache = mmstr_realloc(cache, cachedirlen + f->reclen);
			mmstrcat_cstr(cache, f->name);
		}

		mm_closedir(d);
	}

	mmstr_free(cachedir);
	mmstr_free(cache);
}


static
int fschange_prerm(struct fschange* fsc,
                   const struct binpkg* pkg, const struct binpkg* new)
{
	(void) pkg;
	(void) new;

	fschange_remove_rmfiles_pycache(fsc);
	return 0;
}


static
int fschange_postrm(struct fschange* fsc,
                    const struct binpkg* pkg, const struct binpkg* new)
{
	(void) pkg;
	(void) new;

	fschange_update_rm_dirs(fsc);
	return 0;
}


/**************************************************************************
 *                                                                        *
 *                            Packages retrieval                          *
 *                                                                        *
 **************************************************************************/
/**
 * check_installed_pkg() - check integrity of installed package
 * @pkg:        installed package whose integrity must be checked
 *
 * Current dir is assumed to be the root of the mmpack prefix where to search
 * the installed files.
 *
 * Return: 0 if no issue has been found, -1 otherwise
 */
LOCAL_SYMBOL
int check_installed_pkg(const struct binpkg* pkg)
{
	int rv = -1;
	struct sumsha sumsha;
	struct sumsha_entry* e;
	struct sumsha_iterator iter;
	mmstr* sumsha_path;

	sumsha_init(&sumsha);

	sumsha_path = sha256sums_path(NULL, pkg);
	rv = sumsha_load(&sumsha, sumsha_path);
	mmstr_free(sumsha_path);
	if (rv == -1)
		goto exit;

	for (e = sumsha_first(&iter, &sumsha); e; e = sumsha_next(&iter)) {
		if (check_typed_hash(&e->hash, e->path.buf) != 0)
			goto exit;
	}
	rv = 0;

exit:
	sumsha_deinit(&sumsha);
	return rv;
}


/**
 * fetch_pkgs() - download packages that are going to be installed
 * @ctx:       initialized mmpack context
 * @act_stk:   action stack to be applied
 *
 * NOTE: this function assumes current directory is the prefix path
 *
 * Return: 0 in case of success, -1 otherwise
 */
static
int fetch_pkgs(struct mmpack_ctx* ctx, struct action_stack* act_stk)
{
	const struct binpkg* pkg;
	struct action* act;
	int i;

	for (i = 0; i < act_stk->index; i++) {
		act = &act_stk->actions[i];
		pkg = act->pkg;
		if (act->action != INSTALL_PKG && act->action != UPGRADE_PKG)
			continue;

		if (download_remote_resource(ctx, pkg->remote_res,
		                             &act->pathname)) {
			printf("Failed to fetch %s (%s): %s\n",
			       pkg->name, pkg->version,
			       mm_get_lasterror_desc());
			return -1;
		}
	}

	return 0;
}


/**************************************************************************
 *                                                                        *
 *                           Packages manipulation                        *
 *                                                                        *
 **************************************************************************/


/**
 * fschange_install_pkg() - install a package in the prefix
 * @fsc:        file system change data
 * @pkg:        mmpack package to be installed
 * @mpkfile:    filename of the downloaded package file
 *
 * This function install a package in a prefix hierarchy. The list of installed
 * package of context @fsc->ctx will be updated.
 *
 * NOTE: this function assumes current directory is the prefix path
 *
 * Return: 0 in case of success, -1 otherwise
 */
static
int fschange_install_pkg(struct fschange* fsc,
                         const struct binpkg* pkg, const mmstr* mpkfile)
{
	struct mmpack_ctx* ctx = fsc->ctx;
	char hexstr[SHA_HEXLEN + 1] = {0};

	info("Installing package %s (%s)... ", pkg->name, pkg->version);

	hexstr_from_digest(hexstr, &pkg->sumsha);
	mm_log_info("\tsumsha: %s", hexstr);

	if (fschange_preinst(fsc, NULL, pkg)
	    || fschange_pkg_unpack(fsc, mpkfile)
	    || fschange_postinst(fsc, NULL, pkg)) {
		error("Failed!\n");
		return -1;
	}

	install_state_add_pkg(&ctx->installed, pkg);
	info("OK\n");
	return 0;
}


/**
 * fschange_remove_pkg() - remove a package from the prefix
 * @fsc:        file system change data
 * @pkg:        package to remove
 *
 * This function removes a package from a prefix hierarchy. The list of
 * installed package of context @ctx will be updated.
 *
 * NOTE: this function assumes current directory is the prefix path
 *
 * Return: 0 in case of success, -1 otherwise
 */
static
int fschange_remove_pkg(struct fschange* fsc, const struct binpkg* pkg)
{
	struct mmpack_ctx* ctx = fsc->ctx;

	info("Removing package %s ... ", pkg->name);

	if (fschange_list_pkg_rm_files(fsc, pkg)
	    || fschange_prerm(fsc, pkg, NULL)
	    || fschange_apply_rm_files_list(fsc)
	    || fschange_postrm(fsc, pkg, NULL)) {
		error("Failed!\n");
		return -1;
	}

	install_state_rm_pkgname(&ctx->installed, pkg->name);
	strset_remove(&ctx->manually_inst, pkg->name);

	info("OK\n");

	return 0;
}


/**
 * fschange_upgrade_pkg() - remove a package from the prefix
 * @fsc:        file system change data
 * @pkg:        package to install
 * @oldpkg:     package to remove (replaced by installed package)
 * @mpkfile:    filename of the downloaded package file
 *
 * This function upgrades a package in a prefix hierarchy. The list of
 * installed package of context @ctx will be updated.
 *
 * NOTE: this function assumes current directory is the prefix path
 *
 * Return: 0 in case of success, -1 otherwise
 */
static
int fschange_upgrade_pkg(struct fschange* fsc, const struct binpkg* pkg,
                         const struct binpkg* oldpkg, const mmstr* mpkfile)
{
	int rv = 0;
	struct mmpack_ctx* ctx = fsc->ctx;
	const char* operation;

	if (pkg_version_compare(pkg->version, oldpkg->version) < 0)
		operation = "Downgrading";
	else
		operation = "Upgrading";

	info("%s package %s (%s) over (%s) ... ", operation,
	     pkg->name, pkg->version, oldpkg->version);

	if (fschange_list_pkg_rm_files(fsc, oldpkg)
	    || fschange_prerm(fsc, oldpkg, pkg)
	    || fschange_pkg_unpack(fsc, mpkfile)
	    || fschange_apply_rm_files_list(fsc)
	    || fschange_postrm(fsc, oldpkg, pkg)
	    || fschange_postinst(fsc, oldpkg, pkg)) {
		rv = -1;
	}

	install_state_add_pkg(&ctx->installed, pkg);

	if (rv)
		error("Failed!\n");
	else
		info("OK\n");

	return rv;
}


static
int fschange_apply_action(struct fschange* fsc, struct action* act)
{
	int rv, type;

	fsc->curr_action = act;
	strlist_init(&fsc->inst_files);
	strlist_init(&fsc->rm_files);

	type = act->action;

	switch (type) {
	case INSTALL_PKG:
		rv = fschange_install_pkg(fsc, act->pkg, act->pathname);
		break;

	case REMOVE_PKG:
		rv = fschange_remove_pkg(fsc, act->pkg);
		break;

	case UPGRADE_PKG:
		rv = fschange_upgrade_pkg(fsc,
		                          act->pkg,
		                          act->oldpkg,
		                          act->pathname);
		break;

	default:
		rv = mm_raise_error(EINVAL, "invalid action: %i", type);
		break;
	}

	strlist_deinit(&fsc->inst_files);
	strlist_deinit(&fsc->rm_files);
	fsc->curr_action = NULL;

	return rv;
}


static
void fschange_init(struct fschange* fsc, struct mmpack_ctx* ctx)
{
	*fsc = (struct fschange) {.ctx = ctx};

	strset_init(&fsc->rm_dirs, STRSET_HANDLE_STRINGS_MEM);
	strset_init(&fsc->py_scripts, STRSET_HANDLE_STRINGS_MEM);
}


static
void fschange_deinit(struct fschange* fsc)
{
	fschange_compile_pyscripts(fsc);
	fschange_apply_rm_dirs(fsc);
	strset_deinit(&fsc->rm_dirs);
	strset_deinit(&fsc->py_scripts);
}


static
int check_new_sysdeps(struct action_stack* stack)
{
	int i, rv;
	struct strlist_elt* dep;
	struct strset sysdeps;

	strset_init(&sysdeps, STRSET_FOREIGN_STRINGS);

	// Add all system dependencies to the set if a package is installed
	for (i = 0; i < stack->index; i++) {
		if (stack->actions[i].action != INSTALL_PKG)
			continue;

		// Add all sysdeps if the package
		dep = stack->actions[i].pkg->sysdeps.head;
		while (dep) {
			strset_add(&sysdeps, dep->str.buf);
			dep = dep->next;
		}
	}

	rv = check_sysdeps_installed(&sysdeps);

	strset_deinit(&sysdeps);
	return rv;
}


/**
 * apply_action_stack() - execute the action listed in the stack
 * @ctx:        mmpack context to use
 * @stack:      action stack to apply
 *
 * Return: 0 in case of success, -1 otherwise
 */
int apply_action_stack(struct mmpack_ctx* ctx, struct action_stack* stack)
{
	int i, rv;
	struct fschange fschange;

	if (check_new_sysdeps(stack) != DEPS_OK)
		return -1;

	// Change current directory to prefix... All the prefix relpath can
	// now be used directly.
	if (mm_chdir(ctx->prefix)
	    || mm_mkdir(METADATA_RELPATH, 0777, MM_RECURSIVE)
	    || mm_mkdir(UNPACK_CACHEDIR_RELPATH, 0777, MM_RECURSIVE))
		return -1;

	// Fetch missing packages
	rv = fetch_pkgs(ctx, stack);
	if (rv != 0)
		return rv;

	/* Apply individual action changes
	 * Stop processing actions on error: the actions are ordered so that a
	 * package will not be installed if its dependencies fail to be
	 * installed and not be removed if a back-dependency failed to be
	 * removed.
	 */
	fschange_init(&fschange, ctx);
	for (i = 0; i < stack->index; i++) {
		rv = fschange_apply_action(&fschange, &stack->actions[i]);
		if (rv != 0)
			break;
	}

	fschange_deinit(&fschange);

	// suppress the content of the directory in which the files are unpacked
	mm_remove(UNPACK_CACHEDIR_RELPATH, MM_DT_ANY|MM_RECURSIVE);

	// Restore previous current directory
	mm_chdir(ctx->cwd);

	// Store the updated installed package list in prefix
	if (mmpack_ctx_save_installed_list(ctx))
		rv = -1;

	return rv;
}
