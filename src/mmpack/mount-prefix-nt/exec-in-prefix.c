/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <windows.h>

#include <stdlib.h>
#include <stdnoreturn.h>
#include <mmdlfcn.h>
#include <mmlib.h>
#include <mmsysio.h>

#include "common.h"
#include "path-win32.h"
#include "mmpack-mount-prefix.h"

#include "exec-in-prefix.h"


#define MOUNT_DLL       BIN_TO_LIBEXECDIR "/mmpack/mount-mmpack-prefix.dll"

static
WCHAR* path_u16(const char* path)
{
	WCHAR* wpath = NULL;
	int len;

	len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
	                          path, -1, NULL, 0);
	if (len == -1) {
		fprintf(stderr, "invalid prefix: %s\n", path);
		return NULL;
	}

	wpath = malloc(len*sizeof(*wpath));
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
	                    path, -1, wpath, len);

	return wpath;
}


static noreturn
void mount_and_exec(const char* prefix, const char* argv[])
{
	int exitcode = EXIT_FAILURE;
	int status;
	mm_pid_t pid;
	WCHAR* path;
	char* mount_dll_abspath = NULL;
	mm_dynlib_t* mount_lib = NULL;
	const struct mount_mmpack_dispatch* mmpack_mount = NULL;

	path = path_u16(prefix);
	mount_dll_abspath = get_relocated_path(MOUNT_DLL);
	if (!path || !mount_dll_abspath)
		goto exit;

	mount_lib = mm_dlopen(mount_dll_abspath, 0);
	if (!mount_lib)
		goto exit;

	mmpack_mount = mm_dlsym(mount_lib, "dispatch_table");

	if (mmpack_mount->setup(path)
	   || mm_spawn(&pid, argv[0], 0, NULL, 0, (char**)argv, NULL)
	   || mm_wait_process(pid, &status)
	   || !(status & MM_WSTATUS_EXITED))
		goto umount;

	exitcode = status & MM_WSTATUS_CODEMASK;

umount:
	mm_dlclose(mount_lib);

exit:
	free(path);
	free(mount_dll_abspath);
	exit(exitcode);
}


LOCAL_SYMBOL
int exec_in_prefix(const char* prefix, const char* argv[], int no_prefix_mount)
{
	// Convert environment path list that has been set/enriched here and
	// which are meant to be used in POSIX development environment (in
	// MSYS2 or Cygwin)
	conv_env_pathlist_win32_to_posix("CPATH");
	conv_env_pathlist_win32_to_posix("LIBRARY_PATH");
	conv_env_pathlist_win32_to_posix("MANPATH");
	conv_env_pathlist_win32_to_posix("ACLOCAL_PATH");

	if (no_prefix_mount)
		return mm_execv(argv[0], 0, NULL, 0, (char**)argv, NULL);

	mount_and_exec(prefix, argv);
}

