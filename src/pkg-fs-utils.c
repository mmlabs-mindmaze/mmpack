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

#include "context.h"
#include "download.h"
#include "mmstring.h"
#include "package-utils.h"
#include "pkg-fs-utils.h"
#include "utils.h"
#include "sysdeps.h"

struct filelist_elt {
	struct filelist_elt* next;
	struct mmstring str;
};

struct filelist {
	struct filelist_elt* head;
};


/**************************************************************************
 *                                                                        *
 *                              file list                                 *
 *                                                                        *
 **************************************************************************/
static
void filelist_init(struct filelist* files)
{
	*files = (struct filelist) {0};
}


static
void filelist_deinit(struct filelist* files)
{
	struct filelist_elt *elt, *next;

	elt = files->head;

	while (elt) {
		next = elt->next;
		free(elt);
		elt = next;
	}
}


static
int filelist_add_file(struct filelist* files, const mmstr* path)
{
	struct filelist_elt* elt;
	int pathlen;

	pathlen = mmstrlen(path);
	elt = mm_malloc(sizeof(*elt) + pathlen + 1);
	elt->str.max = pathlen;
	mmstrcpy(elt->str.buf, path);

	// push element to list
	elt->next = files->head;
	files->head = elt;

	return 0;
}


/**************************************************************************
 *                                                                        *
 *                      Packages files unpacking                          *
 *                                                                        *
 **************************************************************************/
#define READ_ARCHIVE_BLOCK      10240
#define READ_ARCHIVE_EOF        1
#define SKIP_UNPACK             1

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
int pkg_unpack_regfile(struct archive_entry *entry, const char* path,
                       struct archive *a)
{
	int r, rv, fd, mode;
	const void *buff;
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
int pkg_unpack_symlink(struct archive_entry *entry, const char* path)
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
 * pkg_unpack_next_entry() - extract archive entry
 * @a:          archive stream from which to read the entry and file content
 * @entry:      archive entry to read
 * @path:       filename of package file being unpacked (it may be
 *              different from the one advertised in entry)
 *
 * Return: 0 on success, a negative value otherwise.
 */
static
int pkg_unpack_entry(struct archive *a, struct archive_entry* entry,
                     const mmstr* path)
{
	int type, rv;

	type = archive_entry_filetype(entry);
	switch (type) {
	case AE_IFDIR:
		rv = mm_mkdir(path, 0777, MM_RECURSIVE);
		break;

	case AE_IFREG:
		rv = pkg_unpack_regfile(entry, path, a);
		break;

	case AE_IFLNK:
		rv = pkg_unpack_symlink(entry, path);
		break;

	default:
		rv = mm_raise_error(MM_EBADFMT,
		                    "unexpected file type of %s", path);
		break;
	}

	return rv;
}


/**
 * redirect_metadata() - modify path of inspected file if metadata
 * @pathname:   pointer to mmstr* holding the original path on input and
 *              modified path on output
 * @metadata_prefix: path to prefix to apply when metadata file
 *
 * This function inspect *@pathname of the file about to be inspected from
 * archive and identifies if it is a metadata or not. A file is a metadata
 * if it starts with "./MMPACK".
 *
 * Return: SKIP_UNPACK if the file entry of archive must be skipped, 0
 * otherwise.
 */
static
int redirect_metadata(mmstr** pathname, const mmstr* metadata_prefix)
{
	char tmp_data[64];
	mmstr* basename = mmstr_map_on_array(tmp_data);
	mmstr* path = *pathname;

	if (mmstrlen(path) == 0)
		return SKIP_UNPACK;

	// Keep path NOT starting with MMPACK untouched
	if (!STR_STARTS_WITH(path, (size_t)mmstrlen(path), "MMPACK"))
		return 0;

	// MMPACK/info is not installed and MMPACK/ must not be created
	if (  mmstrequal(path, mmstr_alloca_from_cstr("MMPACK/info"))
	   || mmstrequal(path, mmstr_alloca_from_cstr("MMPACK/")))
		return SKIP_UNPACK;

	// Change destination
	mmstr_basename(basename, path);
	path = mmstrcpy_realloc(path, metadata_prefix);
	path = mmstrcat_realloc(path, basename);

	*pathname = path;

	return 0;
}


STATIC_CONST_MMSTR(metadata_dirpath, METADATA_RELPATH)

static
int pkg_unpack_files(const struct mmpkg* pkg, const char* mpk_filename)
{
	mmstr* metadata_prefix;
	const char* entry_path;
	struct archive_entry *entry;
	struct archive *a;
	int len, r, rv;
	mmstr* path = NULL;

	// Set the metadata prefix (var/lib/mmpack/metadata/<pkgname>.)
	len = mmstrlen(pkg->name) + mmstrlen(metadata_dirpath) + 2;
	metadata_prefix = mmstr_alloca(len);
	mmstr_join_path(metadata_prefix, metadata_dirpath, pkg->name);
	mmstrcat_cstr(metadata_prefix, ".");

	// Initialize an archive stream
	a = archive_read_new();
	archive_read_support_filter_all(a);
	archive_read_support_format_all(a);

	// Open binary package in the archive stream
	if (archive_read_open_filename(a, mpk_filename, READ_ARCHIVE_BLOCK)) {
		fprintf(stderr, "opening mpk %s failed: %s\n",
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
			fprintf(stderr, "reading mpk %s failed: %s\n",
			                mpk_filename, archive_error_string(a));
			rv = -1;
			break;
		}

		// Obtain the pathname (with leading "./" stripped) of the
		// file being extracted and redirect to metadata folder if
		// it is a metadata file
		entry_path = archive_entry_pathname_utf8(entry);
		path = mmstrcpy_cstr_realloc(path, entry_path+2);
		if (redirect_metadata(&path, metadata_prefix) == SKIP_UNPACK)
			continue;

		rv = pkg_unpack_entry(a, entry, path);
	}
	mmstr_free(path);

	// Cleanup
	archive_read_close(a);
	archive_read_free(a);

	return (rv != READ_ARCHIVE_EOF) ? -1 : 0;
}


/**************************************************************************
 *                                                                        *
 *                          Packages files removal                        *
 *                                                                        *
 **************************************************************************/

#define UNPACK_MAXPATH  512

/**
 * pkg_list_rm_files() - list files a package to be removed
 * @pkg:        package about to be removed
 * @files:      pointer to initialized filelist to populate
 *
 * Return: 0 in case of success, -1 otherwise with error
 */
static
int pkg_list_rm_files(const struct mmpkg* pkg, struct filelist* files)
{
	int len, rv = -1;
	FILE* fp;
	mmstr* path;
	mmstr* metadata_prefix;

	path = mmstr_malloc(UNPACK_MAXPATH);

	// Set the metadata prefix (var/lib/mmpack/metadata/<pkgname>.)
	len = mmstrlen(metadata_dirpath) + mmstrlen(pkg->name) + 2;
	metadata_prefix = mmstr_alloca(len);
	mmstr_join_path(metadata_prefix, metadata_dirpath, pkg->name);
	mmstrcat_cstr(metadata_prefix, ".");

	// Open package's sha256sums file to get file list
	mmstrcpy(path, metadata_prefix);
	mmstrcat_cstr(path, "sha256sums");
	fp = fopen(path, "rb");
	if (fp == NULL) {
		mm_raise_from_errno("Can't open %s", path);
		goto exit;
	}

	// Add immediately the path of sha256sums
	filelist_add_file(files, path);

	while (fscanf(fp, "%"MM_STRINGIFY(UNPACK_MAXPATH)"[^:]: %*s ", path) == 1) {
		mmstr_update_len_from_buffer(path);

		// Check the path has not been truncated
		if (mmstrlen(path) == UNPACK_MAXPATH) {
			mm_raise_error(ENAMETOOLONG, "Path of file"
			               " installed by %s has been truncated"
			               " (truncated path: %s)",
			               pkg->name, path);
			goto exit;
		}

		if (redirect_metadata(&path, metadata_prefix) == SKIP_UNPACK)
			continue;

		// Skip folder (path terminated with '/')
		if (is_path_separator(path[mmstrlen(path)-1]))
			continue;

		filelist_add_file(files, path);
	}

	fclose(fp);
	rv = 0;

exit:
	mmstr_free(path);
	return rv;
}


static
int rm_files_from_list(struct filelist* files)
{
	struct filelist_elt *elt;
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
 * check_file_pkg() - Check integrity of downloaded package
 * @pkg:        package information as provided by repository
 * @filename:   location downloaded package
 *
 * Return: 0 if no issue has been found, -1 otherwise
 */
static
int check_file_pkg(const struct mmpkg* pkg, const mmstr* filename)
{
	mmstr* sha = mmstr_alloca(SHA_HEXSTR_LEN);

	if (sha_compute(sha, filename, NULL)) {
		fprintf(stderr, "Cannot compute SHA-256 of %s: %s\n",
		        filename, mmstrerror(mm_get_lasterror_number()));
		return -1;
	}

	if (!mmstrequal(sha, pkg->sha256)) {
		fprintf(stderr, "bad SHA-256 detected %s \n", filename);
		return -1;
	}

	return 0;
}


/**
 * fetch_pkgs() - download packages that are going to be installed
 * @ctx:        initialized mmpack context
 * @act_stak:   action stack to be applied
 *
 * NOTE: this function assumes current directory is the prefix path
 *
 * Return: 0 in case of success, -1 otherwise
 */
static
int fetch_pkgs(struct mmpack_ctx* ctx, struct action_stack* act_stk)
{
	char pkgbase_data[128];
	mmstr* pkgbase = mmstr_map_on_array(pkgbase_data);
	mmstr* mpkfile = NULL;
	const struct mmpkg* pkg;
	struct action* act;
	const mmstr* repo_url = ctx->settings.repo_url;
	const mmstr* cachedir = mmpack_ctx_get_pkgcachedir(ctx);
	int len, i, rv;

	rv = 0;
	for (i = 0; (i < act_stk->index) && (rv == 0); i++) {
		act = &act_stk->actions[i];
		pkg = act->pkg;
		if (act->action != INSTALL_PKG)
			continue;

		// Get filename of downloaded package and store the path in
		// a field of action structure being analyzed
		mmstr_basename(pkgbase, pkg->filename);
		len = mmstrlen(pkgbase) + mmstrlen(cachedir) + 1;
		mpkfile = mmstr_malloc(len);
		act->pathname = mmstr_join_path(mpkfile, cachedir, pkgbase);

		// Skip if there is a valid package already downloaded
		if (  mm_check_access(mpkfile, F_OK) == 0
		   && check_file_pkg(pkg, mpkfile) == 0)
			continue;

		// Dowload package from repo and store it in prefix
		// package cachedir and check hash
		if (  download_from_repo(ctx, repo_url, pkg->filename,
		                         NULL, mpkfile)
		   || check_file_pkg(pkg, mpkfile))
			rv = -1;
	}

	return rv;
}


/**************************************************************************
 *                                                                        *
 *                           Packages manipulation                        *
 *                                                                        *
 **************************************************************************/

/**
 * install_package() - install a package in the prefix
 * @ctx:        mmpack context
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

	rv = pkg_unpack_files(pkg, mpkfile);
	if (rv)
		return -1;

	install_state_add_pkg(&ctx->installed, pkg);
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
	struct filelist files;

	filelist_init(&files);

	if (  pkg_list_rm_files(pkg, &files)
	   || rm_files_from_list(&files)) {
		rv = -1;
	}

	filelist_deinit(&files);

	install_state_rm_pkgname(&ctx->installed, pkg->name);
	return rv;
}


static
int apply_action(struct mmpack_ctx* ctx, struct action* act)
{
	int rv, type;

	type = act->action;

	switch(type) {
	case INSTALL_PKG:
		rv = install_package(ctx, act->pkg, act->pathname);
		break;

	case REMOVE_PKG:
		rv = remove_package(ctx, act->pkg);
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
	struct mmpkg_dep* dep;
	struct strset sysdeps;

	strset_init(&sysdeps, STRSET_FOREIGN_STRINGS);

	// Add all system dependencies to the set if a package is installed
	for (i = 0; i < stack->index; i++) {
		if (stack->actions[i].action != INSTALL_PKG)
			continue;

		// Add all sysdeps if the package
		dep = stack->actions[i].pkg->sysdeps;
		while (dep) {
			strset_add(&sysdeps, dep->name);
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
	char old_currdir[512];

	if (check_new_sysdeps(stack) != DEPS_OK)
		return -1;

	// Change current directory to prefix... All the prefix relpath can
	// now be used directly.
	mm_getcwd(old_currdir, sizeof(old_currdir));
	if (  mm_chdir(ctx->prefix)
	   || mm_mkdir(METADATA_RELPATH, 0777, MM_RECURSIVE))
		return -1;

	// Fetch missing packages
	rv = fetch_pkgs(ctx, stack);

	// Apply individual action changes
	for (i = 0; (i < stack->index) && (rv == 0); i++)
		rv = apply_action(ctx, &stack->actions[i]);

	// Store the updated installed package list in prefix
	if (mmpack_ctx_save_installed_list(ctx))
		rv = -1;

	// Restore previous current directory
	mm_chdir(old_currdir);
	return rv;
}
