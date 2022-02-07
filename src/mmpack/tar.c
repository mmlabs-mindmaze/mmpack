/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmsysio.h>
#include <mmerrno.h>

#include "buffer.h"
#include "tar.h"


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


/**************************************************************************
 *                                                                        *
 *                       Archive extraction as stream                     *
 *                                                                        *
 **************************************************************************/
#define READ_ARCHIVE_BLOCK 10240


LOCAL_SYMBOL
int tarstream_open(struct tarstream* tar, const char* filename)
{
	struct archive* a;

	// Initialize an archive stream
	a = archive_read_new();
	archive_read_support_filter_all(a);
	archive_read_support_format_all(a);

	// Open binary package in the archive stream
	if (archive_read_open_filename(a, filename, READ_ARCHIVE_BLOCK)) {
		mm_raise_error(archive_errno(a), "opening %s failed: %s",
		               filename, archive_error_string(a));
		archive_read_free(a);
		return -1;
	}

	*tar = (struct tarstream) {.archive = a, .filename = filename};
	return 0;
}


LOCAL_SYMBOL
int tarstream_close(struct tarstream* tar)
{
	int rv;

	rv = archive_read_free(tar->archive);
	*tar = (struct tarstream) {.archive = NULL};

	return rv;
}


LOCAL_SYMBOL
int tarstream_read_next(struct tarstream* tar)
{
	int r;

	r = archive_read_next_header(tar->archive, &tar->entry);
	if (r == ARCHIVE_EOF)
		return READ_ARCHIVE_EOF;

	if (r != ARCHIVE_OK) {
		return mm_raise_error(archive_errno(tar->archive),
		                      "reading archive %s failed: %s",
		                      tar->filename,
		                      archive_error_string(tar->archive));
	}

	tar->entry_type = archive_entry_filetype(tar->entry);
	tar->entry_path = archive_entry_pathname_utf8(tar->entry);
	return 0;
}


/**
 * tarstream_extract_regfile() - extract current tar entry as regular file
 * @tar:        tarstream being extracted
 * @path:       path to which the file must be extracted
 *
 * Return: 0 in case of success, -1 otherwise
 */
static
int tarstream_extract_regfile(struct tarstream* tar, const char* path)
{
	int r, rv, fd, mode;
	const void * buff;
	size_t size;
	int64_t offset;

	// Create the new file. This step should not fail because the caller
	// must have created parent dir
	mode = archive_entry_perm(tar->entry);
	fd = mm_open(path, O_CREAT|O_EXCL|O_WRONLY, mode);
	if (fd < 0)
		return -1;

	// Resize file as reported by archive. If no file size is actually
	// set in archive, 0 will be reported which is harmless
	mm_ftruncate(fd, archive_entry_size(tar->entry));

	rv = 0;
	while (rv == 0) {
		r = archive_read_data_block(tar->archive,
		                            &buff, &size, &offset);
		if (r == ARCHIVE_EOF)
			break;

		if (r != ARCHIVE_OK) {
			rv = mm_raise_from_errno("Unpacking %s failed",
			                         tar->entry_path);
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
 * tarstream_extract() - extract current tar entry
 * @tar:        tarstream being extracted
 * @path:       destination path (it may be different from the one advertised
 *              in entry)
 *
 * Return: 0 on success, a negative value otherwise.
 */
LOCAL_SYMBOL
int tarstream_extract(struct tarstream* tar, const char* path)
{
	const char* target;

	switch (tar->entry_type) {
	case AE_IFDIR:
		return mm_mkdir(path, 0777, MM_RECURSIVE);

	case AE_IFLNK:
		target = archive_entry_symlink_utf8(tar->entry);
		return mm_symlink(target, path);

	case AE_IFREG:
		return tarstream_extract_regfile(tar, path);

	default:
		return mm_raise_error(MM_EBADFMT,
		                      "unexpected file type of %s",
		                      tar->entry_path);
	}
}


/* same as archive_read_data_into_fd(), but into a buffer */
LOCAL_SYMBOL
int tarstream_extract_into_buffer(struct tarstream* tar,
                                  struct buffer * buffer)
{
	ssize_t r;
	int64_t info_size = archive_entry_size(tar->entry);

	if (info_size == 0)
		return -1;

	buffer_reserve_data(buffer, info_size);
	buffer_inc_size(buffer, info_size);

	r = archive_read_data(tar->archive, buffer->base, buffer->size);
	if (r >= ARCHIVE_OK && (size_t) r == buffer->size)
		return 0;

	return -1;
}


/**************************************************************************
 *                                                                        *
 *                         Archive extraction                             *
 *                                                                        *
 **************************************************************************/

/**
 * tar_load_file() - read specified file from archive into buffer
 * @filename: mmpack package to read from
 * @path_in_archive: path in archive of the file to read
 * @buffer: buffer structure to receive the raw data
 *
 * Open, scans for the @path_in_archive file, and load its data into given
 * buffer structure. The buffer will be enlarged as needed, and must be freed
 * by the caller after usage by calling the buffer_deinit() function.
 *
 * Return: 0 on success, -1 on error
 */
LOCAL_SYMBOL
int tar_load_file(const char* filename, const char* path_in_archive,
                  struct buffer* buffer)
{
	int rv;
	struct tarstream tar;

	if (tarstream_open(&tar, filename))
		return -1;

	while ((rv = tarstream_read_next(&tar)) == 0) {
		if (!strcmp(tar.entry_path, path_in_archive)) {
			rv = tarstream_extract_into_buffer(&tar, buffer);
			break;
		}
	}

	tarstream_close(&tar);

	if (rv == READ_ARCHIVE_EOF)
		rv = mm_raise_error(MM_ENOTFOUND,
		                    "Could not find %s in %s",
		                    path_in_archive, filename);

	return rv;
}


/**
 * tar_extract_all() - extracts all files from an archive to a directory
 * @filename:   path to archive
 * @target_dir: directory where file are extracted, it must exists
 *
 * Return: 0 on success, -1 otherwise.
 */
LOCAL_SYMBOL
int tar_extract_all(const char* filename, const char* target_dir)
{
	struct tarstream tar;
	char* prev_cwd = NULL;
	int rv;

	if (tarstream_open(&tar, filename))
		return -1;

	// Change current directory to target_dir (and store previous one)
	if ((prev_cwd = mm_getcwd(NULL, 0)) == NULL
	    || mm_chdir(target_dir)) {
		rv = -1;
		goto exit;
	}

	// Loop over each entry in the archive and process them
	rv = 0;
	while ((rv = tarstream_read_next(&tar)) == 0) {
		rv = tarstream_extract(&tar, tar.entry_path);
		if (rv == -1)
			break;
	}

exit:
	// restore previous current directory
	if (prev_cwd) {
		if (mm_chdir(prev_cwd))
			rv = -1;

		free(prev_cwd);
	}

	tarstream_close(&tar);
	return rv;
}


