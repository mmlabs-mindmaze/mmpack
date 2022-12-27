/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmerrno.h>
#include <mmsysio.h>

#include "mmstring.h"
#include "strlist.h"
#include "sumsha.h"
#include "utils.h"



/**
 * sha256sums_path() - get path to sha256sums file of given package
 * @pkg:      package whose sha256sums file must be obtained.
 *
 * Return:
 * An allocated sha256sums path string relative to a prefix path. The returned
 * pointer must be freed with mmstr_free() when done with it.
 */
LOCAL_SYMBOL
mmstr* sha256sums_path(const struct binpkg* pkg)
{
	int len = sizeof(METADATA_RELPATH "/.sha256sums") + mmstrlen(pkg->name);
	mmstr* sha256sums = mmstr_malloc(len);

	mmstrcat_cstr(sha256sums, METADATA_RELPATH "/");
	mmstrcat(sha256sums, pkg->name);
	mmstrcat_cstr(sha256sums, ".sha256sums");

	return sha256sums;
}


struct sumsha_reader {
	void* map;
	const char* sumsha_path;
	struct strchunk data;
	int failure;
};


static
int sumsha_reader_init(struct sumsha_reader* reader, const char* sumsha_path)
{
	void* map = NULL;
	size_t mapsize = 0;

	if (map_file_in_prefix(NULL, sumsha_path, &map, &mapsize))
		return -1;

	*reader = (struct sumsha_reader) {
		.data = {.buf = map, .len = mapsize},
		.map = map,
		.sumsha_path = sumsha_path,
	};
	return 0;
}


static
int sumsha_reader_deinit(struct sumsha_reader* reader)
{
	mm_unmap(reader->map);
	return reader->failure ? -1 : 0;
}


static
int sumsha_reader_next(struct sumsha_reader* reader,
                       struct strchunk* path, struct strchunk* hash)
{
	struct strchunk line;
	int pos;

	if (reader->data.len == 0)
		return 0;

	line = strchunk_getline(&reader->data);
	pos = strchunk_rfind(line, ':');
	if (pos == -1) {
		reader->failure = 1;
		mm_raise_error(EBADMSG, "Error while parsing %s",
		               reader->sumsha_path);
		return 0;
	}

	*path = strchunk_lpart(line, pos);
	if (hash) {
		// Skip space after colon before reading hash value
		*hash = strchunk_rpart(line, pos+1);
	}

	return 1;
}


/**
 * read_sha256sums() - parse the sha256sums of an installed package
 * @sha256sums_path: path to the sha256sums file to read.
 * @filelist: string list receiving the list of file in package
 * @hashlist: string list receiving the hash for each file in @filelist. If
 *            NULL, the hash list is ignored.
 *
 * Open and parse the sha256sums file of the installed package @pkg. The
 * installed package is assumed to be located relatively to the current
 * directory.
 *
 * Return: 0 in case of success, -1 otherwise.
 */
LOCAL_SYMBOL
int read_sha256sums(const mmstr* sha256sums_path,
                    struct strlist* filelist, struct strlist* hashlist)
{
	struct sumsha_reader reader;
	struct strchunk path, hash;

	if (sumsha_reader_init(&reader, sha256sums_path))
		return -1;

	while (sumsha_reader_next(&reader, &path, &hash)) {
		strlist_add_strchunk(filelist, path);

		if (!hashlist)
			continue;

		strlist_add_strchunk(hashlist, hash);
	}

	return sumsha_reader_deinit(&reader);
}


/**
 * read_sumsha_filelist() - parse the sha256sums and fill the file list
 * @sumsha_path:        path to the sha256sums file to read.
 * @filelist:           string list receiving the list of file in package
 *
 * Open and parse the sha256sums file and fill the @filelist with the
 * referenced files.
 *
 * Return: 0 in case of success, -1 otherwise.
 */
LOCAL_SYMBOL
int read_sumsha_filelist(const char* sumsha_path, struct strlist* filelist)
{
	struct sumsha_reader reader;
	struct strchunk path;

	if (sumsha_reader_init(&reader, sumsha_path))
		return -1;

	while (sumsha_reader_next(&reader, &path, NULL))
		strlist_add_strchunk(filelist, path);

	return sumsha_reader_deinit(&reader);
}
