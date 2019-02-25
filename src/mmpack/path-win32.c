/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmerrno.h>
#include <mmlib.h>
#include <mmsysio.h>

#include "mmstring.h"
#include "path-win32.h"
#include "utils.h"


/**
 * conv_pathlist_win32_to_posix() - convert win32 path list into posix path list
 * @win32_pathlist:     list of windows path separated by semicolons
 *
 * Return: a mmstr pointer holding the transformed path list. When done with
 * it, the returned value must be cleansed with mmstr_free().
 */
static
mmstr* conv_pathlist_win32_to_posix(const char* win32_pathlist)
{
	// TODO: Replace call to cygpath with an function call that will
	// perform conversion in mmpack process
	char* argv[] = {"cygpath.exe", "-p", (char*)win32_pathlist, NULL};
	struct buffer cmd_output;
	mmstr* outstr = NULL;
	int len;

	buffer_init(&cmd_output);
	if (execute_cmd_capture_output(argv, &cmd_output))
		goto exit;

	outstr = mmstr_malloc_copy(cmd_output.base, cmd_output.size);

	// Strip trailing '\n' if any
	for (len = mmstrlen(outstr); len > 0; len--) {
		if (outstr[len-1] != '\n')
			break;
	}
	mmstr_setlen(outstr, len);

exit:
	buffer_deinit(&cmd_output);
	return outstr;
}


/**
 * conv_env_pathlist_win32_to_posix() - posixify env var path list
 * @envname:    name of environement variable to posixify
 *
 * Convert a environment variable named @envname whose value is assumed to be a
 * list of windows paths separated by semicolons  into a POSIX format, ie POSIX
 * path separated by colons.
 */
LOCAL_SYMBOL
void conv_env_pathlist_win32_to_posix(const char* envname)
{
	const char* envvalue;
	mmstr* posix_value;

	envvalue = mm_getenv(envname, NULL);
	if (!envvalue)
		return;

	posix_value = conv_pathlist_win32_to_posix(envvalue);
	mm_setenv(envname, posix_value, MM_ENV_OVERWRITE);
	mmstr_free(posix_value);
}
