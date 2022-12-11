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
#include "strlist.h"
#include "utils.h"


#define PREFIX_LIST_SUBPATH     "mmpack/known_prefixes"

static
mmstr* alloc_list_path_str(void)
{
	return get_xdg_subpath(MM_CACHE_HOME, PREFIX_LIST_SUBPATH);
}


static
int save_list(struct strlist* list)
{
	struct buffer buff = {0};
	struct strlist_elt *elt;
	mmstr* list_path;
	int rv;

	for (elt = list->head; elt; elt = elt->next) {
		buffer_push(&buff, elt->str.buf, elt->str.len);
		buffer_push(&buff, "\n", 1);
	}

	list_path = alloc_list_path_str();
	rv = save_file_atomically(list_path, &buff);
	mmstr_free(list_path);

	buffer_deinit(&buff);

	return rv;
}


static
int filter_list(struct strlist* list)
{
	STATIC_CONST_MMSTR(hashset_subpath, HASHSET_RELPATH);
	struct strlist_elt *elt, *prev;
	mmstr* hpath = NULL;
	const mmstr* prefix;
	bool update = false;
	int rv = 0;

	for (prev = NULL, elt = list->head; elt; prev = elt, elt = elt->next) {
		prefix = elt->str.buf;
		hpath = mmstr_join_path_realloc(hpath, hashset_subpath, prefix);

		if (mm_check_access(hpath, R_OK) == 0)
			continue;

		// drop element since the prefix cannot be found
		strlist_drop_after(list, prev);
		elt = prev->next;
		update = true;
	}
	mmstr_free(hpath);

	if (update)
		rv = save_list(list);

	return rv;
}


static
void list_load(struct strlist* list)
{
	struct strchunk content, prefix;
	mmstr* list_path = NULL;
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
		prefix = strchunk_getline(&content);
		strlist_add_strchunk(list, prefix);
	}

	mm_unmap(map);

exit:
	mmstr_free(list_path);
	mm_error_set_flags(flags, MM_ERROR_NOLOG);
}


LOCAL_SYMBOL
struct strlist load_other_prefixes(const char* current_prefix)
{
	struct strlist list = {NULL};
	char* prefix_abspath;
	struct strlist_elt *elt, *prev;

	list_load(&list);
	filter_list(&list);

	// remove current prefix from prefix list
	prefix_abspath = expand_abspath(current_prefix);
	for (prev = NULL, elt = list.head; elt; prev = elt, elt = elt->next) {
		if (mmstrequal(elt->str.buf, prefix_abspath)) {
			strlist_drop_after(&list, prev);
			break;
		}
	}
	free(prefix_abspath);

	return list;
}


LOCAL_SYMBOL
int update_prefix_list_with_current_prefix(const char* current_prefix)
{
	struct strlist list = {NULL};
	char* prefix_abspath;
	struct strlist_elt *elt;
	int rv = 0;

	list_load(&list);
	prefix_abspath = expand_abspath(current_prefix);

	// search for current prefix in prefix list
	for (elt = list.head; elt; elt = elt->next) {
		if (mmstrequal(elt->str.buf, prefix_abspath))
			goto exit;
	}

	strlist_add(&list, prefix_abspath);
	rv = save_list(&list);

exit:
	free(prefix_abspath);
	strlist_deinit(&list);
	return rv;
}
