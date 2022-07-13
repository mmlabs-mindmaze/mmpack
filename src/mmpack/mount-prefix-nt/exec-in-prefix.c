/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmlib.h>
#include <mmsysio.h>

#include "path-win32.h"

#include "exec-in-prefix.h"


static char* mount_prefix_bin = NULL;


MM_DESTRUCTOR(mount_prefix_bin_str)
{
	free(mount_prefix_bin);
}

#define REL_MOUNT_PREFIX_BIN \
	BIN_TO_LIBEXECDIR "/mmpack/mount-mmpack-prefix" EXEEXT

static
char* get_mount_prefix_bin(void)
{
	if (!mount_prefix_bin)
		mount_prefix_bin = get_relocated_path(REL_MOUNT_PREFIX_BIN);

	return mount_prefix_bin;
}


LOCAL_SYMBOL
int exec_in_prefix(const char* prefix, const char* argv[], int no_prefix_mount)
{
	char** new_argv;
	int nargs;

	// Count number of element in argv
	for (nargs = 0; argv[nargs] != NULL; nargs++);

	// Convert environment path list that has been set/enriched here and
	// which are meant to be used in POSIX development environment (in
	// MSYS2 or Cygwin)
	conv_env_pathlist_win32_to_posix("CPATH");
	conv_env_pathlist_win32_to_posix("LIBRARY_PATH");
	conv_env_pathlist_win32_to_posix("MANPATH");
	conv_env_pathlist_win32_to_posix("ACLOCAL_PATH");

	// new_argv contains: 1 command, 1 prefix, nargs arguments from
	// argv and one terminating NULL => length = nargs+3
	new_argv = alloca((nargs+3) * sizeof(*new_argv));
	new_argv[0] = get_mount_prefix_bin();
	new_argv[1] = (char*)prefix;

	// args is terminated by NULL (either from argv or default_argv) and
	// this is not counted in nargs, thus if we copy nargs+1 element
	// from args, the NULL termination will be added to new_argv
	memcpy(new_argv+2, argv, (nargs+1) * sizeof(*argv));

	if (no_prefix_mount)
		new_argv = (char**)argv;

	return mm_execv(new_argv[0], 0, NULL, 0, new_argv, NULL);
}
