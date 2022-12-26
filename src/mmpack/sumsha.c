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
	struct strchunk data_to_parse, line;
	int pos, rv;
	void* map = NULL;
	size_t mapsize = 0;

	rv = map_file_in_prefix(NULL, sha256sums_path, &map, &mapsize);
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
