/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmerrno.h>
#include <mmlib.h>
#include <mmsysio.h>

#include "buffer.h"
#include "mmstring.h"
#include "path-win32.h"
#include "utils.h"
#include "xx-alloc.h"


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
 * @envname:    name of environment variable to posixify
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


/**
 * get_relocated_path() - get relocated absolute path
 * @rel_path_from_executable: path relative to executable
 *
 * Return: the relocated path. When not needed, the returned string must be
 * freed
 */
LOCAL_SYMBOL
char* get_relocated_path(const char* rel_path_from_executable)
{
	DWORD ret;
	DWORD len = 128;
	WCHAR* wexepath = NULL;
	int u8_len, idx;
	char* abspath_u8;

	// Get absolute path of current executable in UTF-16
	do {
		len *= 2;
		wexepath = xx_realloc(wexepath, len);
		ret = GetModuleFileNameW(NULL, wexepath, len);
	} while (ret == len);

	// Get the length of the previous path converted in UTF-8
	u8_len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
	                             wexepath, -1, NULL, 0, NULL, NULL);

	// Get executable path in UTF-8. Reserve the size to append the rel_path
	abspath_u8 = xx_malloc(u8_len + 1 + strlen(rel_path_from_executable));
	WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wexepath, -1,
	                    abspath_u8, u8_len, NULL, NULL);

	free(wexepath);

	// position u8_len to the beginning of the basename
	for (idx = u8_len; idx > 0; idx--) {
		if (is_path_separator(abspath_u8[idx-1]))
			break;
	}

	// Append the rel_path
	strcpy(abspath_u8 + idx, rel_path_from_executable);

	return abspath_u8;
}
