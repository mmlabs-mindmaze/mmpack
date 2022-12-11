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
int prefix_init_load(struct prefix* prefix, struct strchunk path)
{
	prefix->path = mmstr_copy_realloc(NULL, path.buf, path.len);
	return hashset_init_from_file(&prefix->set, NULL);
}


static
void prefix_deinit(struct prefix* prefix)
{
	mmstr_free(prefix->path);
	hashset_deinit(&prefix->set);
}


static
struct prefix_list* list_load(const char* prefix_abspath,
                              struct buffer* list_out,
                              struct strchunk sc_in,
			      int* update)
{
	int need_update = 0, has_current_prefix = 0, num_prefix = 0;
	struct strchunk line;
	struct buffer list_buf = {0};
	struct buffer list = {0};
	struct prefix prefix;

	buffer_push(&list_buf, &list, sizeof(list));

	while (sc_in.len) {
		line = strchunk_getline(&sc_in);
		if (strchunk_equal(line, prefix_abspath)) {
			has_current_prefix = 1;
			continue;
		}

		if (prefix_init_load(&prefix, line))
			continue;

		num_prefix++;
		buffer_push(&list, &prefix, sizeof(prefix));
		push_path(list_out, line);
	}

	if (!has_current_prefix) {
		line.buf = prefix_abspath;
		line.len = strlen(prefix_abspath);
		push_path(list_out, line);
		need_update = 1;
	}

	*update = need_update;
	return NULL;
}


LOCAL_SYMBOL
struct prefix_list* prefix_list_load(const char* current_prefix)
{
	int flags, update = 0;
	struct buffer buff = {0};
	mmstr* list_path = NULL;
	char* prefix_abspath;
	void* map = NULL;
	size_t len = 0;
	struct prefix_list* list;

	flags = mm_error_set_flags(MM_ERROR_SET, MM_ERROR_NOLOG);

	list_path = get_xdg_subpath(MM_CACHE_HOME, PREFIX_LIST_SUBPATH);
	prefix_abspath = expand_abspath(current_prefix);

	// Load file, but missing file or issue to open remained silent
	map_file_in_prefix(NULL, list_path, &map, &len);
	list = list_load(prefix_abspath, &buff,
	                 (struct strchunk) {.buf = map, .len = len},
	                 &update);
	mm_unmap(map);

	if (update)
		save_file_atomically(list_path, &buff);

	buffer_deinit(&buff);
	free(prefix_abspath);
	mmstr_free(list_path);

	mm_error_set_flags(flags, MM_ERROR_NOLOG);

	return list;
}


LOCAL_SYMBOL
void prefix_list_destroy(struct prefix_list* list)
{
	int i;

	for (i = 0; i < list->num; i++)
		prefix_deinit(&list->prefixes[i]);

	free(list);
}
