/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmerrno.h>
#include <mmsysio.h>
#include <stdbool.h>

#include "buffer.h"
#include "mmstring.h"
#include "prefix-list.h"
#include "strset.h"
#include "utils.h"


#define PREFIX_LIST_SUBPATH     "mmpack/known_prefixes"

static
mmstr* alloc_list_path_str(void)
{
	return get_xdg_subpath(MM_CACHE_HOME, PREFIX_LIST_SUBPATH);
}


static
mmstr* mmstr_abspath(const char* path)
{
	char* abspath_cstr;
	mmstr* abspath;

	abspath_cstr = expand_abspath(path);
	abspath = mmstr_malloc_from_cstr(abspath_cstr);
	free(abspath_cstr);

	return abspath;
}


static
int save_list(struct strset* set)
{
	struct buffer buff = {0};
	mmstr* list_path;
	const mmstr* p;
	int rv;
	struct strset_iterator it;

	for (p = strset_iter_first(&it, set); p; p = strset_iter_next(&it)) {
		buffer_push(&buff, p, mmstrlen(p));
		buffer_push(&buff, "\n", 1);
	}

	list_path = alloc_list_path_str();
	rv = save_file_atomically(list_path, &buff);
	mmstr_free(list_path);

	buffer_deinit(&buff);

	return rv;
}


static
int filter_list(struct strset* set)
{
	STATIC_CONST_MMSTR(hashset_subpath, HASHSET_RELPATH);
	mmstr* hpath = NULL;
	const mmstr* p;
	bool update = false;
	int rv = 0;
	struct strset_iterator it;

	for (p = strset_iter_first(&it, set); p; p = strset_iter_next(&it)) {
		hpath = mmstr_join_path_realloc(hpath, hashset_subpath, p);
		if (mm_check_access(hpath, R_OK) == 0)
			continue;

		// drop element since the prefix cannot be found
		strset_remove(set, p);
		update = true;
	}
	mmstr_free(hpath);

	if (update)
		rv = save_list(set);

	return rv;
}


static
void list_load(struct strset* set)
{
	struct strchunk content, line;
	mmstr *list_path = NULL, *prefix = NULL;
	void* map = NULL;
	size_t len = 0;
	int flags;

	flags = mm_error_set_flags(MM_ERROR_SET, MM_ERROR_NOLOG);
	list_path = alloc_list_path_str();

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
	mmstr_free(list_path);
	mm_error_set_flags(flags, MM_ERROR_NOLOG);
}


LOCAL_SYMBOL
int load_other_prefixes(struct strset* set, const char* current_prefix)
{
	int rv = 0;
	char* prefix_abspath;

	list_load(set);
	rv = filter_list(set);

	// remove current prefix from prefix list
	prefix_abspath = mmstr_abspath(current_prefix);
	strset_remove(set, prefix_abspath);
	mmstr_free(prefix_abspath);

	return rv;
}


LOCAL_SYMBOL
int update_prefix_list_with_current_prefix(const char* current_prefix)
{
	struct strset set;
	char* prefix_abspath;
	int rv = 0;

	strset_init(&set, STRSET_HANDLE_STRINGS_MEM);

	list_load(&set);
	prefix_abspath = mmstr_abspath(current_prefix);

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
