/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <assert.h>
#include <archive.h>
#include <archive_entry.h>
#include <mmsysio.h>
#include <mmerrno.h>

#include "common.h"
#include "context.h"
#include "download.h"
#include "mmstring.h"
#include "package-utils.h"
#include "pkg-fs-utils.h"
#include "utils.h"
#include "sysdeps.h"


/**
 * sha256sums_path() - get path to sha256sums file of given package
 * @pkg:      package whose sha256sums file must be obtained.
 *
 * Return:
 * An allocated sha256sums path string relative to a prefix path. The returned
 * pointer must be freed with mmstr_free() when done with it.
 */
static
mmstr* sha256sums_path(const struct mmpkg* pkg)
{
	int len = sizeof(METADATA_RELPATH "/.sha256sums") + mmstrlen(pkg->name);
	mmstr* sha256sums = mmstr_malloc(len);

	mmstrcat_cstr(sha256sums, METADATA_RELPATH "/");
	mmstrcat(sha256sums, pkg->name);
	mmstrcat_cstr(sha256sums, ".sha256sums");

	return sha256sums;
}


/**************************************************************************
 *                                                                        *
 *                      Packages files unpacking                          *
 *                                                                        *
 **************************************************************************/
#define READ_ARCHIVE_BLOCK 10240
#define READ_ARCHIVE_EOF 1

/**
 * fullwrite() - write fully data buffer to a file
 * @fd:         file descriptor where to write data
 * @data:       data buffer to write
 * @size:       size of @data
 *
 * Return: 0 if @data has been fully written to @fd, -1 otherwise
 */
static
int fullwrite(int fd, const char* data, size_t size)
{
	ssize_t rsz;

	do {
		rsz = mm_write(fd, data, size);
		if (rsz < 0)
			return -1;

		size -= rsz;
		data += rsz;
	} while (size > 0);

	return 0;
}


/**
 * pkg_unpack_regfile() - extract a regular file from archive
 * @entry:      entry header of the file being extracted
 * @path:       path to which the file must be extracted
 * @a:          archive stream from which to read the file content
 *
 * Return: 0 in case of success, -1 otherwise
 */
static
int pkg_unpack_regfile(struct archive_entry * entry, const char* path,
                       struct archive * a)
{
	int r, rv, fd, mode;
	const void * buff;
	size_t size;
	int64_t offset;

	// If previous file exists, remove it first
	if (mm_check_access(path, F_OK) != ENOENT) {
		if (mm_unlink(path))
			return -1;
	}

	// Create the new file. This step should not fail because the caller
	// must have created parent dir
	mode = archive_entry_perm(entry);
	fd = mm_open(path, O_CREAT|O_EXCL|O_WRONLY, mode);
	if (fd < 0)
		return -1;

	// Resize file as reported by archive. If no file size is actually
	// set in archive, 0 will be reported which is harmless
	mm_ftruncate(fd, archive_entry_size(entry));

	rv = 0;
	while (rv == 0) {
		r = archive_read_data_block(a, &buff, &size, &offset);
		if (r == ARCHIVE_EOF)
			break;

		if (r != ARCHIVE_OK) {
			rv = mm_raise_from_errno("Unpacking %s failed", path);
			break;
		}

		if (mm_seek(fd, offset, SEEK_SET) == -1) {
			rv = -1;
			break;
		}

		rv = fullwrite(fd, buff, size);
	}

	mm_close(fd);
	return rv;
}


/**
 * pkg_unpack_symlink() - extract a symbolic link from archive
 * @entry:      entry header of the symlink being extracted
 * @path:       path to which the symlink must be extracted
 *
 * Return: 0 in case of success, -1 otherwise
 */
static
int pkg_unpack_symlink(struct archive_entry * entry, const char* path)
{
	const char* target;
	int rv;

	// If previous file exists, remove it first
	if (mm_check_access(path, F_OK) != ENOENT) {
		if (mm_unlink(path))
			return -1;
	}

	// Create a symlink (path -> target)
	target = archive_entry_symlink_utf8(entry);
	rv = mm_symlink(target, path);

	return rv;
}


/**
 * pkg_unpack_entry() - extract archive entry
 * @a:          archive stream from which to read the entry and file content
 * @entry:      archive entry to read
 * @path:       filename of package file being unpacked (it may be
 *              different from the one advertised in entry)
 * @cpt:        counter permitting to create the name of the file in which
 *              regular and symlink files are extracted to
 *
 * Return: 0 or 1 on success, a negative value otherwise. If 1 is returned, this
 * implies that a file has been unpacked in a temporary directory and should be
 * renamed later.
 */
static
int pkg_unpack_entry(struct archive * a, struct archive_entry* entry,
                     const mmstr* path, int cpt)
{
	int type, rv;
	mmstr* file;
	int len;

	type = archive_entry_filetype(entry);
	switch (type) {
	case AE_IFDIR:
		rv = mm_mkdir(path, 0777, MM_RECURSIVE);
		break;

	case AE_IFREG:
	case AE_IFLNK:
		len = sizeof(UNPACK_CACHEDIR_RELPATH) + 10;
		file = mmstr_malloc(len);
		sprintf(file, "%s/%d", UNPACK_CACHEDIR_RELPATH, cpt);

		if (type == AE_IFREG)
			rv = pkg_unpack_regfile(entry, file, a);
		else
			rv = pkg_unpack_symlink(entry, file);

		if (rv == 0)
			rv = 1;

		mmstr_free(file);
		break;

	default:
		rv = mm_raise_error(MM_EBADFMT,
		                    "unexpected file type of %s", path);
		break;
	}

	return rv;
}


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
 * rename_all() - rename all the files
 * @to_rename:    files to be renamed
 *
 * In order for the install and upgrade commands to be atomic, the extraction is
 * done in two steps: first all the regular files and symlink are extracted in a
 * temporary directory (in var/cache/unpack), then they are all renamed, to be
 * placed into their final directories. Note that the directories of the package
 * are extracted directly in the good directory during the first step.
 *
 * The current function permits to rename the regular and symlink files.
 *
 * Return: 0 on success, a negative value otherwise.
 */
static
int rename_all(struct strlist* to_rename)
{
	mmstr * file = NULL;
	int len = sizeof(UNPACK_CACHEDIR_RELPATH) + 10;
	struct strlist_elt * curr = to_rename->head;
	int cpt = 0;

	file = mmstr_malloc(len);
	while (curr) {
		sprintf(file, "%s/%d", UNPACK_CACHEDIR_RELPATH, cpt);

		if (mm_rename(file, curr->str.buf) == -1)
			return -1;

		curr = curr->next;
		cpt++;
	}

	mmstr_free(file);
	return 0;
}


/**
 * pkg_unpack_files() - extract files of a given package
 * @mpk_filename: filename of the downloaded package file
 * @files:        files to be removed
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
int pkg_unpack_files(const char* mpk_filename, struct strlist* files)
{
	const char* entry_path;
	struct archive_entry * entry;
	struct archive * a;
	int r, rv;
	mmstr* path = NULL;
	struct strlist to_rename;
	int cpt = 0;

	strlist_init(&to_rename);

	// Initialize an archive stream
	a = archive_read_new();
	archive_read_support_filter_all(a);
	archive_read_support_format_all(a);

	// Open binary package in the archive stream
	if (archive_read_open_filename(a, mpk_filename, READ_ARCHIVE_BLOCK)) {
		mm_raise_error(archive_errno(a), "opening mpk %s failed: %s",
		               mpk_filename, archive_error_string(a));
		archive_read_free(a);
		return -1;
	}

	// Loop over each entry in the archive and process them
	rv = 0;
	while (rv == 0) {
		r = archive_read_next_header(a, &entry);
		if (r == ARCHIVE_EOF) {
			rv = READ_ARCHIVE_EOF;
			break;
		}

		if (r != ARCHIVE_OK) {
			mm_raise_error(archive_errno(a),
			               "reading mpk %s failed: %s",
			               mpk_filename,
			               archive_error_string(a));
			rv = -1;
			break;
		}

		// Obtain the pathname (with leading "./" stripped) of the
		// file being extracted and skip metadata (MMPACK/*)
		entry_path = archive_entry_pathname_utf8(entry);
		path = mmstrcpy_cstr_realloc(path, entry_path+2);
		if (!mmstrlen(path) || is_mmpack_metadata(path))
			continue;

		rv = pkg_unpack_entry(a, entry, path, cpt);

		if (rv == 1) {
			strlist_add(&to_rename, path);
			cpt++;
			rv = 0;
		}

		if (files)
			strlist_remove(files, path);
	}

	mmstr_free(path);

	// Cleanup
	archive_read_close(a);
	archive_read_free(a);

	if (rv != -1) {
		// proceed to the rename of the files that have been unpacked in
		// another directory than the "true" one, in order to execute an
		// atomic upgrade
		rv = rename_all(&to_rename);
	}

	strlist_deinit(&to_rename);

	return rv;
}


/* same as archive_read_data_into_fd(), but into a buffer */
static
int unpack_entry_into_buffer(struct archive * archive,
                             struct archive_entry * entry,
                             struct buffer * buffer)
{
	ssize_t r;
	int64_t info_size = archive_entry_size(entry);

	if (info_size == 0)
		return -1;

	buffer_reserve_data(buffer, info_size);
	buffer_inc_size(buffer, info_size);

	r = archive_read_data(archive, buffer->base, buffer->size);
	if (r >= ARCHIVE_OK && (size_t) r == buffer->size)
		return 0;

	return -1;
}

/**
 * pkg_get_mmpack_info() - load MMPACK/info file from package into buffer
 * @mpk_filename: mmpack package to read from
 * @buffer: buffer structure to receive the raw data
 *
 * Open, scans for the MMPACK/info file, and load its data into given buffer
 * structure. The buffer will be enlarged as needed, and must be freed by the
 * caller after usage by calling the buffer_deinit() function.
 *
 * Return: 0 on success, -1 on error
 */
LOCAL_SYMBOL
int pkg_get_mmpack_info(char const * mpk_filename, struct buffer * buffer)
{
	int rv;
	struct archive * a;
	struct archive_entry * entry;

	a = archive_read_new();
	archive_read_support_filter_all(a);
	archive_read_support_format_all(a);

	if (archive_read_open_filename(a, mpk_filename, READ_ARCHIVE_BLOCK)) {
		mm_raise_error(archive_errno(a), "opening mpk %s failed: %s",
		               mpk_filename, archive_error_string(a));
		archive_read_free(a);
		return -1;
	}

	rv = -1;
	while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
		if (!strcmp(archive_entry_pathname(entry), "./MMPACK/info")) {
			rv = unpack_entry_into_buffer(a, entry, buffer);
			break;
		}

		archive_read_data_skip(a);
	}

	archive_read_close(a);
	archive_read_free(a);

	return rv;
}

/**************************************************************************
 *                                                                        *
 *                          Packages files removal                        *
 *                                                                        *
 **************************************************************************/

#define UNPACK_MAXPATH 512

/**
 * pkg_list_rm_files() - list files of a package to be removed
 * @pkg:        package about to be removed
 * @files:      pointer to initialized strlist to populate
 *
 * Return: 0 in case of success, -1 otherwise with error
 */
static
int pkg_list_rm_files(const struct mmpkg* pkg, struct strlist* files)
{
	int rv = -1;
	FILE* fp;
	mmstr* path;

	path = sha256sums_path(pkg);
	fp = fopen(path, "rb");
	if (fp == NULL) {
		mm_raise_from_errno("Can't open %s", path);
		goto exit;
	}

	// Add immediately the path of sha256sums
	strlist_add(files, path);

	path = mmstr_realloc(path, UNPACK_MAXPATH);
	while (fscanf(fp, "%"MM_STRINGIFY (UNPACK_MAXPATH)"[^:]: %*s ",
	              path) == 1) {
		mmstr_update_len_from_buffer(path);

		// Check the path has not been truncated
		if (mmstrlen(path) == UNPACK_MAXPATH) {
			mm_raise_error(ENAMETOOLONG, "Path of file"
			               " installed by %s has been truncated"
			               " (truncated path: %s)",
			               pkg->name, path);
			goto exit;
		}

		// Skip folder (path terminated with '/')
		if (is_path_separator(path[mmstrlen(path)-1]))
			continue;

		strlist_add(files, path);
	}

	fclose(fp);
	rv = 0;

exit:
	mmstr_free(path);
	return rv;
}


static
int rm_files_from_list(struct strlist* files)
{
	struct strlist_elt * elt;
	const mmstr* path;

	elt = files->head;

	while (elt) {
		path = elt->str.buf;

		if (mm_unlink(path)) {
			// If this has failed because the file is not found,
			// nothing prevent us to continue (maybe the user
			// has removed the file by mistake... but this
			// should not block). If something else, let's halt
			if (mm_get_lasterror_number() != ENOENT)
				return -1;
		}

		elt = elt->next;
	}

	return 0;
}


/**************************************************************************
 *                                                                        *
 *                            Packages retrieval                          *
 *                                                                        *
 **************************************************************************/
/**
 * check_file_pkg() - Check integrity of given file
 * @ref_sha: reference file sha256 to compare against
 * @parent: prefix directory to prepend to @filename to get the
 *          final path of the file to hash. This may be NULL
 * @filename: path of file whose hash must be computed
 *
 * Return: 0 if no issue has been found, -1 otherwise
 */
static
int check_file_pkg(const mmstr * ref_sha, const mmstr * parent,
                   const mmstr * filename)
{
	int follow;
	mmstr* sha = mmstr_alloca(SHA_HEXSTR_LEN);

	// If reference hash contains type prefix (ie its length is
	// SHA_HEXSTR_LEN), symlink must not be followed
	follow = 0;
	if (mmstrlen(ref_sha) != SHA_HEXSTR_LEN)
		follow = 1;

	if (sha_compute(sha, filename, parent, follow))
		return -1;

	if (!mmstrequal(sha, ref_sha)) {
		mm_raise_error(EBADMSG, "bad SHA-256 detected %s", filename);
		return -1;
	}

	return 0;
}


/**
 * check_installed_pkg() - check integrity of installed package
 * @ctx:        mmpack context. If NULL, current dir is assumed to be the root
 *              the mmpack prefix where to search the installed files.
 * @pkg:        installed package whose integrity must be checked
 *
 * Return: 0 if no issue has been found, -1 otherwise
 */
LOCAL_SYMBOL
int check_installed_pkg(const struct mmpack_ctx* ctx, const struct mmpkg* pkg)
{
	mmstr * filename;
	mmstr * ref_sha;
	mmstr * sumsha_path;
	char * line, * eol;
	size_t line_len, filename_len, mapsize;
	void * map;
	int rv;

	sumsha_path = sha256sums_path(pkg);
	rv = map_file_in_prefix(ctx->prefix, sumsha_path, &map, &mapsize);
	mmstr_free(sumsha_path);
	if (rv == -1)
		return -1;

	line = map;
	filename = ref_sha = NULL;
	while ((eol = strchr(line, '\n')) != NULL) {
		line_len = eol - line;

		if (line_len < SHA_HEXSTR_LEN) {
			mm_raise_error(EBADMSG,
			               "Error while parsing SHA-256 file");
			break;
		}

		ref_sha = mmstr_copy_realloc(ref_sha,
		                             &line[line_len - SHA_HEXSTR_LEN],
		                             SHA_HEXSTR_LEN);

		/* 2 is for len(': ') */
		if (line_len <= SHA_HEXSTR_LEN + 2) {
			mm_raise_error(EBADMSG,
			               "Error while parsing SHA-256 file");
			break;;
		}

		filename_len = line_len - SHA_HEXSTR_LEN - 2;
		filename = mmstr_copy_realloc(filename, line, filename_len);
		if (is_mmpack_metadata(filename))
			goto check_continue;

		if (check_file_pkg(ref_sha, ctx->prefix, filename) != 0)
			break;

check_continue:
		line = eol + 1;
	}

	mmstr_free(filename);
	mmstr_free(ref_sha);
	mm_unmap(map);

	return (eol == NULL) ? 0 : -1;
}


/**
 * action_set_pathname_into_dir() - construct path of cached package file
 * @act:        action struct whose pathname field is to be set
 * @from_repo:  repository from which the package was downloaded
 * @dir:        folder in which the cached downloaded packages must reside
 *
 * This function set @act->pathname to point to a file whose basename is
 * the name of the binary package file associed with @act->pkg and whose
 * folder is specified by @dir. This is typically used to set the name of
 * the downloaded file in the local folder of cached packages (specified by
 * @dir).
 */
static
void action_set_pathname_into_dir(struct action* act, const mmstr* dir)
{
	mmstr* pkgbase;
	int len;
	struct from_repo * from_repo = act->pkg->from_repo;

	pkgbase = mmstr_malloca(mmstrlen(from_repo->filename));

	// Get base filename of downloaded package
	mmstr_basename(pkgbase, from_repo->filename);

	// Store the joined cachedir and base filename in act->pathname
	len = mmstrlen(pkgbase) + mmstrlen(dir) + 1;
	act->pathname = mmstr_malloc(len);
	mmstr_join_path(act->pathname, dir, pkgbase);

	mmstr_freea(pkgbase);
}


/**
 * download_package() - download package helper
 * @ctx: initialized mmpack context
 * @pkg: package to download
 * @pathname: path where the package is to be written
 *
 * This function will:
 * - find out the correct repository url
 * - download the package
 * - check the package's integrity
 *
 * This function does print info and error messages to the console.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly
 */
LOCAL_SYMBOL
int download_package(struct mmpack_ctx * ctx, struct mmpkg const * pkg,
                     mmstr const * pathname)
{
	struct from_repo * from;

	info("Downloading %s (%s)... ", pkg->name, pkg->version);

	for (from = pkg->from_repo; from != NULL; from = from->next) {
		if (!from->repo)
			return -1;

		/* Download package from repo */
		if (download_from_repo(ctx, from->repo->url,
		                       from->filename, NULL, pathname)) {
			continue;
		}

		/* verify integrity of what has been downloaded */
		if (check_file_pkg(from->sha256, NULL, pathname)) {
			error("Integrity check failed!\n");
			return -1;
		}

		info("OK\n");
		return 0;
	}

	error("Failed!\n");
	return -1;
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
	mmstr* mpkfile = NULL;
	const struct mmpkg* pkg;
	struct from_repo * from;
	struct action* act;
	const mmstr* cachedir = mmpack_ctx_get_pkgcachedir(ctx);
	int i;

	for (i = 0; i < act_stk->index; i++) {
		act = &act_stk->actions[i];
		pkg = act->pkg;
		if (act->action != INSTALL_PKG && act->action != UPGRADE_PKG)
			continue;

		from = act->pkg->from_repo;
		if (!from)
			return -1;

		if (!from->repo) {
			act->pathname = mmstrdup(from->filename);
			mm_log_info(
				"Going to install %s (%s) directly from file",
				pkg->name,
				pkg->version);
			continue;
		}

		// Package is going to be installed from a downloaded package
		action_set_pathname_into_dir(act, cachedir);
		mpkfile = act->pathname;

		// Skip if there is a valid package already downloaded
		if (mm_check_access(mpkfile, F_OK) == 0
		    && check_file_pkg(from->sha256, NULL, mpkfile) == 0) {
			mm_log_info("Going to install %s (%s) from cache",
			            pkg->name, pkg->version);
			continue;
		}

		if (download_package(ctx, pkg, mpkfile) != 0)
			return -1;
	}

	return 0;
}


/**************************************************************************
 *                                                                        *
 *                           Packages manipulation                        *
 *                                                                        *
 **************************************************************************/

/**
 * install_package() - install a package in the prefix
 * @ctx:        mmpack context
 * @pkg:        mmpack package to be installed
 * @mpkfile:    filename of the downloaded package file
 *
 * This function install a package in a prefix hierarchy. The list of installed
 * package of context @ctx will be updated.
 *
 * NOTE: this function assumes current directory is the prefix path
 *
 * Return: 0 in case of success, -1 otherwise
 */
static
int install_package(struct mmpack_ctx* ctx,
                    const struct mmpkg* pkg, const mmstr* mpkfile)
{
	int rv;

	info("Installing package %s (%s)... ", pkg->name, pkg->version);

	mm_log_info("\tsumsha: %s", pkg->sumsha);

	rv = pkg_unpack_files(mpkfile, NULL);
	if (rv) {
		error("Failed!\n");
		return -1;
	}

	install_state_add_pkg(&ctx->installed, pkg);
	info("OK\n");
	return rv;
}


/**
 * remove_package() - remove a package from the prefix
 * @ctx:        mmpack context
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
int remove_package(struct mmpack_ctx* ctx, const struct mmpkg* pkg)
{
	int rv = 0;
	struct strlist files;

	info("Removing package %s ... ", pkg->name);

	strlist_init(&files);

	if (pkg_list_rm_files(pkg, &files)
	    || rm_files_from_list(&files)) {
		rv = -1;
		goto exit;
	}

	install_state_rm_pkgname(&ctx->installed, pkg->name);

exit:
	strlist_deinit(&files);

	if (rv)
		error("Failed!\n");
	else
		info("OK\n");

	return rv;
}


/**
 * upgrade_package() - remove a package from the prefix
 * @ctx:        mmpack context
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
int upgrade_package(struct mmpack_ctx* ctx, const struct mmpkg* pkg,
                    const struct mmpkg* oldpkg, const mmstr* mpkfile)
{
	int rv = 0;
	struct strlist files;
	const char* operation;

	if (pkg_version_compare(pkg->version, oldpkg->version) < 0)
		operation = "Downgrading";
	else
		operation = "Upgrading";

	info("%s package %s (%s) over (%s) ... ", operation,
	     pkg->name, pkg->version, oldpkg->version);

	strlist_init(&files);

	if (pkg_list_rm_files(oldpkg, &files)
	    || pkg_unpack_files(mpkfile, &files)
	    || rm_files_from_list(&files)) {
		rv = -1;
	}

	strlist_deinit(&files);

	install_state_add_pkg(&ctx->installed, pkg);

	if (rv)
		error("Failed!\n");
	else
		info("OK\n");

	return rv;
}


static
int apply_action(struct mmpack_ctx* ctx, struct action* act)
{
	int rv, type;

	type = act->action;

	switch (type) {
	case INSTALL_PKG:
		rv = install_package(ctx, act->pkg, act->pathname);
		break;

	case REMOVE_PKG:
		rv = remove_package(ctx, act->pkg);
		if (!rv)
			strset_remove(&ctx->manually_inst, act->pkg->name);

		break;

	case UPGRADE_PKG:
		rv = upgrade_package(ctx, act->pkg, act->oldpkg, act->pathname);
		break;

	default:
		rv = mm_raise_error(EINVAL, "invalid action: %i", type);
		break;
	}

	return rv;
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
 * @ctx:        mmpack contect to use
 * @stack:      action stack to apply
 *
 * Return: 0 in case of success, -1 otherwise
 */
int apply_action_stack(struct mmpack_ctx* ctx, struct action_stack* stack)
{
	int i, rv;

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
	for (i = 0; i < stack->index; i++) {
		rv = apply_action(ctx, &stack->actions[i]);
		if (rv != 0)
			break;
	}

	// suppress the content of the directory in which the files are unpacked
	mm_remove(UNPACK_CACHEDIR_RELPATH, MM_DT_ANY|MM_RECURSIVE);

	// Restore previous current directory
	mm_chdir(ctx->cwd);

	// Store the updated installed package list in prefix
	if (mmpack_ctx_save_installed_list(ctx))
		rv = -1;

	return rv;
}
