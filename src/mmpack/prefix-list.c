/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmerrno.h>
#include <mmpredefs.h>
#include <mmsysio.h>
#include <stdbool.h>

#include "buffer.h"
#include "mmstring.h"
#include "prefix-list.h"
#include "strset.h"
#include "utils.h"


#define PREFIX_LIST_SUBPATH     "mmpack/known_prefixes"

static mmstr* prefix_list_path = NULL;


MM_DESTRUCTOR(clean_list_path)
{
	mmstr_free(prefix_list_path);
	prefix_list_path = NULL;
}


/**
 * get_list_path() - get path to the global known prefix file
 *
 * Return: mmstr* pointer to the path. This must not be deallocated.
 */
static
const mmstr* get_list_path(void)
{
	if (!prefix_list_path)
		prefix_list_path = get_xdg_subpath(MM_CACHE_HOME,
		                                   PREFIX_LIST_SUBPATH);

	return prefix_list_path;
}


/**
 * set_prefix_list_path() - configure prefix list path for next access
 * @path:       pointer to path, NULL to reset to the default one
 */
LOCAL_SYMBOL
void set_prefix_list_path(const char* path)
{
	mmstr_free(prefix_list_path);
	prefix_list_path = path ? mmstr_malloc_from_cstr(path) : NULL;
}


static
int save_list(struct strset* set)
{
	struct buffer buff = {0};
	mmstr* list_dir;
	const mmstr *p, *list_path = get_list_path();
	int rv = 0;
	struct strset_iterator it;

	for (p = strset_iter_first(&it, set); p; p = strset_iter_next(&it)) {
		buffer_push(&buff, p, mmstrlen(p));
		buffer_push(&buff, "\n", 1);
	}

	list_dir = mmstr_malloc(mmstrlen(list_path));
	mmstr_dirname(list_dir, list_path);

	if (mm_mkdir(list_dir, 0777, MM_RECURSIVE)
	    || save_file_atomically(list_path, &buff))
		rv = -1;

	mmstr_free(list_dir);
	buffer_deinit(&buff);

	return rv;
}


static
int filter_list(struct strset* set)
{
	STATIC_CONST_MMSTR(hashset_subpath, HASHSET_RELPATH);
	mmstr* hpath = NULL;
	const mmstr* p;
	int i, num, rv = 0;
	struct strset_iterator it;
	struct buffer drop_table_buf;
	const mmstr** drop_table;

	buffer_init(&drop_table_buf);

	// Collect the known prefixes that cannot be access into a drop table
	// (an element of indextable, hence strset cannot be remove while being
	// iterated)
	for (p = strset_iter_first(&it, set); p; p = strset_iter_next(&it)) {
		hpath = mmstr_join_path_realloc(hpath, p, hashset_subpath);
		if (mm_check_access(hpath, R_OK) == 0)
			continue;

		// add prefix to drop table
		buffer_push(&drop_table_buf, &p, sizeof(p));
	}
	mmstr_free(hpath);

	// Remove from set of known prefixes those in the drop list
	drop_table = drop_table_buf.base;
	num = drop_table_buf.size / sizeof(*drop_table);
	for (i = 0; i < num; i++)
		strset_remove(set, drop_table[i]);

	buffer_deinit(&drop_table_buf);

	if (num != 0)
		rv = save_list(set);

	return rv;
}


static
void list_load(struct strset* set)
{
	struct strchunk content, line;
	mmstr* prefix = NULL;
	const mmstr* list_path = get_list_path();
	void* map = NULL;
	size_t len = 0;
	int flags;

	flags = mm_error_set_flags(MM_ERROR_SET, MM_ERROR_NOLOG);

	// Load file, but missing file or issue to open remained silent
	if (map_file_in_prefix(NULL, list_path, &map, &len))
		goto exit;

	content.buf = map;
	content.len = len;
	while (content.len) {
		line = strchunk_getline(&content);
		prefix = mmstr_copy_realloc(prefix, line.buf, line.len);
		strset_add(set, prefix);
	}

	mm_unmap(map);

exit:
	mmstr_free(prefix);
	mm_error_set_flags(flags, MM_ERROR_NOLOG);
}


/**
 * load_other_prefixes() - load the set of other known prefixes
 * @set:        initialized strset meant to receive the set of prefixes
 * @ignore_prefix: path to a prefix that must be ignored
 *
 * This loads the set of known prefixes from the global prefix list file and
 * add them (absolute path) in @set. When performing this, the different
 * prefixes are checked for being still present and removed from the global
 * file if absent.
 *
 * The return set will not contain the path pointed to by @ignore_prefix which
 * is meant to be used with the current prefix being operated on.
 * @ignore_prefix can be a relative or absolute path.
 *
 * Return: 0 in case of success, -1 otherwise with error state set accordingly.
 */
LOCAL_SYMBOL
int load_other_prefixes(struct strset* set, const char* ignore_prefix)
{
	int rv = 0;
	char* prefix_abspath;

	list_load(set);
	rv = filter_list(set);

	// remove current prefix from prefix list
	prefix_abspath = expand_abspath(ignore_prefix);
	strset_remove(set, prefix_abspath);
	mmstr_free(prefix_abspath);

	return rv;
}


/**
 * update_prefix_list_with_prefix() - update global list with prefix
 * @prefix:     prefix path that must be added to the list
 *
 * This updates the global list of known prefixes with the specified path
 * pointed to by @prefix. This path can be relative or absolute. The global
 * list is rewritten only if the @prefix is not found in the global list. In
 * case of actual rewrite, the previous prefies in the list may be reordered.
 *
 * Return: 0 in case of success, -1 otherwise with error state set.
 */
LOCAL_SYMBOL
int update_prefix_list_with_prefix(const char* prefix)
{
	struct strset set;
	char* prefix_abspath;
	int rv = 0;

	strset_init(&set, STRSET_HANDLE_STRINGS_MEM);

	list_load(&set);
	prefix_abspath = expand_abspath(prefix);

	// search for current prefix in prefix list
	if (strset_contains(&set, prefix_abspath))
		goto exit;

	strset_add(&set, prefix_abspath);
	rv = save_list(&set);

exit:
	mmstr_free(prefix_abspath);
	strset_deinit(&set);
	return rv;
}
