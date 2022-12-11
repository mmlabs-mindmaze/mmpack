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
#define NUM_ATTEMPT             10


static
int save_file_atomically(mmstr* path, const struct buffer* buff)
{
	int i, rv = -1;
	mmstr* tmp_path = NULL;
	
	tmp_path = mmstrcpy_cstr_realloc(NULL, path);

	for (i = 0; i < NUM_ATTEMPT; i++) {
		tmp_path = mmstr_append_realloc(tmp_path, "~", 1);
		rv = save_file(tmp_path, buff, O_EXCL);
		if (rv) {
			if (i >= NUM_ATTEMPT-1
			    || mm_get_lasterror_number() != EEXIST)
				goto exit;
		}
	}
	rv = mm_rename(tmp_path, path);

exit:
	mmstr_free(tmp_path);
	return rv;
}



LOCAL_SYMBOL
int register_in_prefix_list(const char* prefix_path)
{
	int flags, rv = 0;
	struct buffer buff = {0};
	mmstr* list_path = NULL;
	struct strchunk sc, line;
	char* prefix_abspath;

	list_path = get_xdg_subpath(MM_CACHE_HOME, PREFIX_LIST_SUBPATH);
	prefix_abspath = expand_abspath(prefix_path);

	// Load file, but missing file or issue to open remained silent
	flags = mm_error_set_flags(MM_ERROR_SET, MM_ERROR_IGNORE);
	load_file(list_path, &buff);
	mm_error_set_flags(flags, MM_ERROR_IGNORE);

	// Test prefix_abspath is in file content
	sc.buf = buff.base;
	sc.len = buff.size;
	while (sc.len) {
		line = strchunk_getline(&sc);
		if (strchunk_equal(line, prefix_abspath))
			goto exit;
	}

	// Recreate a new file with prefix path appended to it
	buffer_push(&buff, prefix_abspath, strlen(prefix_abspath));
	buffer_push(&buff, "\n", 1);
	rv = save_file_atomically(list_path, &buff);

exit:
	buffer_deinit(&buff);
	free(prefix_abspath);
	mmstr_free(list_path);
	return rv;
}
