/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmerrno.h>
#include <mmsysio.h>

#include "buffer.h"
#include "mmstring.h"
#include "prefix-list.h"
#include "utils.h"


#define PREFIX_LIST_SUBPATH     "mmpack/known_prefixes"


static
void push_path(struct buffer* buff, struct strchunk path)
{
	buffer_push(buff, path.buf, path.len);
	buffer_push(buff, "\n", 1);
}


static
struct prefix_list* list_load(const char* prefix_abspath,
                              struct buffer* list_out,
                              struct strlist* list,
                              struct strchunk sc_in,
			      int* update)
{
	STATIC_CONST_MMSTR(hashset_subpath, HASHSET_RELPATH);
	int need_update = 0, has_current_prefix = 0;
	struct strchunk line;
	mmstr *path = NULL, *hpath = NULL;

	while (sc_in.len) {
		line = strchunk_getline(&sc_in);
		path = mmstr_copy_realloc(path, line.buf, line.len);
		hpath = mmstr_join_path_realloc(hpath, hashset_subpath, path);

		if (strchunk_equal(line, prefix_abspath)) {
			has_current_prefix = 1;
			push_path(list_out, line);
		}

		if (mm_check_access(hpath, R_OK) != 0)
			continue;

		strlist_add(list, path);
		push_path(list_out, line);
	}

	if (!has_current_prefix) {
		line.buf = prefix_abspath;
		line.len = strlen(prefix_abspath);
		push_path(list_out, line);
		need_update = 1;
	}

	mmstr_free(path);
	mmstr_free(hpath);
	*update = need_update;
	return NULL;
}


LOCAL_SYMBOL
void prefix_list_load(const char* current_prefix, struct strlist* list)
{
	int flags, update = 0;
	struct buffer buff = {0};
	mmstr* list_path = NULL;
	char* prefix_abspath;
	void* map = NULL;
	size_t len = 0;

	flags = mm_error_set_flags(MM_ERROR_SET, MM_ERROR_NOLOG);

	list_path = get_xdg_subpath(MM_CACHE_HOME, PREFIX_LIST_SUBPATH);
	prefix_abspath = expand_abspath(current_prefix);

	// Load file, but missing file or issue to open remained silent
	map_file_in_prefix(NULL, list_path, &map, &len);
	struct strchunk content = {.buf = map, .len = len};
	list_load(prefix_abspath, &buff, list, content, &update);
	mm_unmap(map);

	if (update)
		save_file_atomically(list_path, &buff);

	buffer_deinit(&buff);
	free(prefix_abspath);
	mmstr_free(list_path);

	mm_error_set_flags(flags, MM_ERROR_NOLOG);
}
