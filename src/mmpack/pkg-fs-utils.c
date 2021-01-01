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
#include <mmlib.h>
#include <mmerrno.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "context.h"
#include "download.h"
#include "mmstring.h"
#include "package-utils.h"
#include "pkg-fs-utils.h"
#include "utils.h"
#include "sysdeps.h"


/**
 * struct fschange - data carried over file system actions
 * ctx: mmpack prefix context
 * inst_files:  list of files being installed
 * rm_files:    list of files being removed
 * rm_dirs:     set of folders to try to remove at the end of stack application
 *
 * This structure holds data that needs to be shared over the installation,
 * removal or upgrade of the different packages when applying the actions
 * stack.
 */
struct fschange {
	struct mmpack_ctx* ctx;
	struct strlist inst_files;
	struct strlist rm_files;
	struct strset rm_dirs;
};


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


/**
 * read_sha256sums() - parse the sha256sums of an installed package
 * @ctx:      mmpack prefix context containing @pkg (may be NULL)
 * @sha256sums_path: path to the sha256sums file to read. If @ctx is not NULL,
 *            this path is interpreted as relative to prefix folder of @ctx.
 * @filelist: string list receiving the list of file in package
 * @hashlist: string list receiving the hash for each file in @filelist. If
 *            NULL, the hash list is ignored.
 *
 * Open and parse the sha256sums file of the installed package @pkg from the
 * mmpack prefix context @ctx. If @ctx is NULL, the installed package is
 * assumed to be located relatively to the current directory.
 *
 * Return: 0 in case of success, -1 otherwise.
 */
static
int read_sha256sums(const struct mmpack_ctx* ctx, const mmstr* sha256sums_path,
                    struct strlist* filelist, struct strlist* hashlist)
{
	struct strchunk data_to_parse, line;
	int pos, rv;
	void* map = NULL;
	size_t mapsize = 0;
	const mmstr* prefix = ctx ? ctx->prefix : NULL;

	rv = map_file_in_prefix(prefix, sha256sums_path, &map, &mapsize);
	if (rv == -1)
		goto exit;

	data_to_parse = (struct strchunk) {.buf = map, .len = mapsize};
	while (data_to_parse.len) {
		line = strchunk_getline(&data_to_parse);
		pos = strchunk_rfind(line, ':');
		if (pos == -1) {
			rv = mm_raise_error(EBADMSG, "Error while parsing %s",
			                    sha256sums_path);
			break;
		}

		strlist_add_strchunk(filelist, strchunk_lpart(line, pos));

		if (!hashlist)
			continue;

		// Skip space after colon before reading hash value
		strlist_add_strchunk(hashlist, strchunk_rpart(line, pos+1));
	}


exit:
	mm_unmap(map);
	return rv;
}


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
	const char* entry_path;
	struct archive_entry * entry;
	struct archive * a;
	int r, rv;
	mmstr* path = NULL;
	int cpt = 0;

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
			strlist_add(&fsc->inst_files, path);
			cpt++;
			rv = 0;
		}

		strlist_remove(&fsc->rm_files, path);
	}

	mmstr_free(path);

	// Cleanup
	archive_read_close(a);
	archive_read_free(a);

	if (rv != -1) {
		// proceed to the rename of the files that have been unpacked in
		// another directory than the "true" one, in order to execute an
		// atomic upgrade
		rv = rename_all(&fsc->inst_files);
	}

	return rv;
}


static
void fschange_compile_pyscripts(struct fschange* fsc)
{
	struct strlist pyscripts;
	struct strlist_elt* elt;
	struct strchunk base, ext;
	const mmstr* path;
	int i, num_scripts;
	char** argv = NULL;

	strlist_init(&pyscripts);

	// Scan all files listed for installation
	num_scripts = 0;
	for (elt = fsc->inst_files.head; elt != NULL; elt = elt->next) {
		path = elt->str.buf;

		// Extract path component and skip if not .py file
		split_path(path, &base, &ext);
		if (strncmp(ext.buf, "py", ext.len) != 0)
			continue;

		strlist_add(&pyscripts, path);
		num_scripts++;
	}

	if (!num_scripts)
		goto exit;

	i = 0;
	argv = xx_malloca((num_scripts + 5) * sizeof(*argv));
	argv[i++] = "python3";
	argv[i++] = "-m";
	argv[i++] = "compileall";
	argv[i++] = "-q";
	for (elt = pyscripts.head; elt != NULL; elt = elt->next)
		argv[i++] = elt->str.buf;

	argv[i++] = NULL;
	execute_cmd(argv);
	mm_freea(argv);

exit:
	strlist_deinit(&pyscripts);
}


static
int fschange_preinst(struct fschange* fsc,
                     const struct mmpkg* old, const struct mmpkg* pkg)
{
	(void) fsc;
	(void) pkg;
	(void) old;

	return 0;
}


static
int fschange_postinst(struct fschange* fsc,
                      const struct mmpkg* old, const struct mmpkg* pkg)
{
	(void) pkg;
	(void) old;

	fschange_compile_pyscripts(fsc);
	return 0;
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
int fschange_list_pkg_rm_files(struct fschange* fsc, const struct mmpkg* pkg)
{
	int rv;
	mmstr* path;

	path = sha256sums_path(pkg);

	strlist_add(&fsc->rm_files, path);
	rv = read_sha256sums(NULL, path, &fsc->rm_files, NULL);

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
			if (!strncmp(base.buf, f->name, base.len))
				continue;

			// Set basename of the found cache file to cache string
			mmstr_setlen(cache, cachedirlen);
			cache = mmstr_realloc(cache, cachedirlen + f->reclen);
			mmstrcat_cstr(cache, f->name);

			// Remove cache file
			mm_unlink(cache);
		}

		mm_closedir(d);
	}

	mmstr_free(cachedir);
	mmstr_free(cache);
}


static
int fschange_prerm(struct fschange* fsc,
                   const struct mmpkg* pkg, const struct mmpkg* new)
{
	(void) pkg;
	(void) new;

	fschange_remove_rmfiles_pycache(fsc);
	return 0;
}


static
int fschange_postrm(struct fschange* fsc,
                    const struct mmpkg* pkg, const struct mmpkg* new)
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
 * @ctx:        mmpack context. If NULL, current dir is assumed to be the root
 *              the mmpack prefix where to search the installed files.
 * @pkg:        installed package whose integrity must be checked
 *
 * Return: 0 if no issue has been found, -1 otherwise
 */
LOCAL_SYMBOL
int check_installed_pkg(const struct mmpack_ctx* ctx, const struct mmpkg* pkg)
{
	mmstr * filename, * ref_sha, * sumsha_path;
	struct strlist filelist, hashlist;
	struct strlist_elt * file_elt, * hash_elt;
	int rv = -1;

	strlist_init(&filelist);
	strlist_init(&hashlist);

	sumsha_path = sha256sums_path(pkg);
	rv = read_sha256sums(ctx, sumsha_path, &filelist, &hashlist);
	mmstr_free(sumsha_path);
	if (rv == -1)
		goto exit;

	file_elt = filelist.head;
	hash_elt = hashlist.head;
	while (file_elt) {
		mm_check(hash_elt != NULL);
		filename = file_elt->str.buf;
		ref_sha = hash_elt->str.buf;

		if (check_hash(ref_sha, ctx->prefix, filename) != 0)
			goto exit;

		file_elt = file_elt->next;
		hash_elt = hash_elt->next;
	}

	rv = 0;

exit:
	strlist_deinit(&filelist);
	strlist_deinit(&hashlist);
	return rv;
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
	const struct remote_resource* res = act->pkg->remote_res;

	pkgbase = mmstr_malloca(mmstrlen(res->filename));

	// Get base filename of downloaded package
	mmstr_basename(pkgbase, res->filename);

	// Store the joined cachedir and base filename in act->pathname
	len = mmstrlen(pkgbase) + mmstrlen(dir) + 1;
	act->pathname = mmstr_malloc(len);
	mmstr_join_path(act->pathname, dir, pkgbase);

	mmstr_freea(pkgbase);
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
	struct action* act;
	const mmstr* cachedir = mmpack_ctx_get_pkgcachedir(ctx);
	int i;

	for (i = 0; i < act_stk->index; i++) {
		act = &act_stk->actions[i];
		pkg = act->pkg;
		if (act->action != INSTALL_PKG && act->action != UPGRADE_PKG)
			continue;

		// Set downloaded file path in cachedir
		action_set_pathname_into_dir(act, cachedir);
		mpkfile = act->pathname;

		if (download_remote_resource(ctx, pkg->remote_res, mpkfile)) {
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
                         const struct mmpkg* pkg, const mmstr* mpkfile)
{
	struct mmpack_ctx* ctx = fsc->ctx;

	info("Installing package %s (%s)... ", pkg->name, pkg->version);

	mm_log_info("\tsumsha: %s", pkg->sumsha);

	if (fschange_preinst(fsc, NULL, pkg)
	    || fschange_pkg_unpack(fsc, mpkfile)
	    || fschange_postinst(fsc, NULL, pkg)) {
		error("Failed!\n");
		return -1;
	}

	install_state_add_pkg(&ctx->installed, pkg);
	info("OK\n");

	mm_unlink(mpkfile);
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
int fschange_remove_pkg(struct fschange* fsc, const struct mmpkg* pkg)
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
int fschange_upgrade_pkg(struct fschange* fsc, const struct mmpkg* pkg,
                         const struct mmpkg* oldpkg, const mmstr* mpkfile)
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

	mm_unlink(mpkfile);
	return rv;
}


static
int fschange_apply_action(struct fschange* fsc, struct action* act)
{
	int rv, type;

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

	return rv;
}


static
void fschange_init(struct fschange* fsc, struct mmpack_ctx* ctx)
{
	*fsc = (struct fschange) {.ctx = ctx};

	strset_init(&fsc->rm_dirs, STRSET_HANDLE_STRINGS_MEM);
}


static
void fschange_deinit(struct fschange* fsc)
{
	fschange_apply_rm_dirs(fsc);
	strset_deinit(&fsc->rm_dirs);
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
