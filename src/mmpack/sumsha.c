/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmerrno.h>
#include <mmsysio.h>

#include "common.h"
#include "indextable.h"
#include "mmstring.h"
#include "strlist.h"
#include "sumsha.h"
#include "utils.h"
#include "xx-alloc.h"



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


/***********************************************************************
 *                      sumsha file reader                             *
 ***********************************************************************/
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


static
void sumsha_reader_parse_typed_hash(struct sumsha_reader* reader,
                                    struct typed_hash* hash, struct strchunk sv)
{
	struct strchunk prefix, hexhash;

	hexhash =  strchunk_rpart(sv, SHA_HDRLEN - 1);
	if (digest_from_hexstr(&hash->digest, hexhash))
		goto failure;

	prefix = strchunk_lpart (sv, SHA_HDRLEN);
	if (STR_STARTS_WITH(prefix.buf, prefix.len, SHA_HDR_REG))
		hash->type = MM_DT_REG;
	else if (STR_STARTS_WITH(prefix.buf, prefix.len, SHA_HDR_SYM))
		hash->type = MM_DT_LNK;
	else
		goto failure;

	return;

failure:
	reader->failure = 1;
	mm_raise_error(EBADMSG, "Invalid hash %*s when parsing %s",
		       sv.len, sv.buf, reader->sumsha_path);
}


/***********************************************************************
 *                      sumsha data methods                            *
 ***********************************************************************/

/**
 * sumsha_init() - initialize sumsha table structure
 * @sumsha:     pointer to sumsha struct to initialize
 */
LOCAL_SYMBOL
void sumsha_init(struct sumsha* sumsha)
{
	indextable_init(&sumsha->idx, 64, -1);
}


/**
 * sumsha_deinit() - cleanup sumsha table structure
 * @sumsha:     pointer to sumsha struct to clean
 */
LOCAL_SYMBOL
void sumsha_deinit(struct sumsha* sumsha)
{
	struct sumsha_entry* e;
	struct sumsha_iterator iter;

	for (e = sumsha_first(&iter, sumsha); e; e = sumsha_next(&iter))
		free(e);

	indextable_deinit(&sumsha->idx);
}


/**
 * sumsha_load() - load sumsha table from a .sha256sums file
 * @sumsha:     initialized sumsha table to hold the file content
 * @sumsha_path: path to .sha256sums file
 *
 * Return: 0 in case of success, otherwise -1 with error state set.
 */
LOCAL_SYMBOL
int sumsha_load(struct sumsha* sumsha, const char* sumsha_path)
{
	struct sumsha_reader reader;
	struct strchunk path, hash;
	struct sumsha_entry* entry;
	struct it_entry* idx_entry;

	if (sumsha_reader_init(&reader, sumsha_path))
		return -1;

	while (sumsha_reader_next(&reader, &path, &hash)) {
		// Create sumsha entry
		entry = xx_malloc(sizeof(*entry) + path.len + 1);
		mmstr_copy(entry->path.buf, path.buf, path.len);
		sumsha_reader_parse_typed_hash(&reader, &entry->hash, hash);

		// Insert sumsha entry in indextable
		idx_entry = indextable_insert(&sumsha->idx, entry->path.buf);
		idx_entry->key = entry->path.buf;
		idx_entry->value = entry;
	}

	return sumsha_reader_deinit(&reader);
}


/***********************************************************************
 *                      General sumsha functions                       *
 ***********************************************************************/
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
