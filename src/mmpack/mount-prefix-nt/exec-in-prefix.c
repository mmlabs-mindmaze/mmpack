/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdlib.h>
#include <mmlib.h>
#include <mmsysio.h>

#include "path-win32.h"

#include "exec_in_prefix.h"


/**
 * check_running_mmpack() - check mmpack prefix input
 * @path: given input path
 *
 * This is used to prevent running two different prefixes at the same time
 *
 * This only is about prevent two distinct concurrent prefixes on the same
 * windows session. The same user logged twice independently (with password)
 * does not have that restriction.
 *
 * Returns: 0 on success (path can be used), or a non-zero value
 * otherwise
 */
static
int check_running_mmpack(WCHAR * path)
{
	DWORD rv;
	WCHAR prev_path[MAX_PATH];
	WCHAR * norm_prev_path;
	WCHAR norm_path[MAX_PATH];

	/* get and normalize the folder path of MOUNT_TARGET (M:) */
	rv = QueryDosDeviceW(MOUNT_TARGET, prev_path, MAX_PATH);
	if (rv == 0) {
		switch (GetLastError()) {
		case ERROR_FILE_NOT_FOUND:
			return 0;

		default:
			return -1;
		}
	}
	if (memcmp(prev_path, L"\\??\\", 4 * sizeof(WCHAR)) == 0)
		norm_prev_path = &prev_path[wcslen(L"\\??\\")];
	else
		norm_prev_path = &prev_path[0];

	/* normalize input path */
	GetFullPathNameW(path, MAX_PATH, norm_path, NULL);

	return wcscmp(norm_prev_path, norm_path);
}


static
WCHAR* abswpath(const char* path)
{
	char* fpath;
	WCHAR* wpath = NULL;
	int len;

	fpath = _fullpath(NULL, path, 32768);
	len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
	                          fpath, -1,
	                          NULL, 0);
	if (len == -1) {
		fprintf(stderr, "invalid prefix: %s\n", prefix);
		goto exit;
	}

	wpath = malloc(len*sizeof(*wpath));
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
	                    fpath, -1, wpath, len);

exit:
	free(fpath);
	return wpath;
}


static noreturn
void mount_and_exec(const char* prefix, const char* argv[])
{
	int exitcode = EXIT_FAILURE;
	int status;
	mm_pid_t pid;
	WCHAR* path;

	path = abswpath(prefix);
	if (!path)
		goto exit;

	if (check_running_mmpack(path)) {
		/* TODO: find a way to create a new authenticated session here */
		fprintf(stderr, "Cannot mount target with different prefixes and "
		                "on the same session.");
		goto exit;
	}

	// Create temporary drive M: pointing to prefix_path
	if (!DefineDosDeviceW(DDD_NO_BROADCAST_SYSTEM, MOUNT_TARGET, path)) {
		fprintf(stderr, "Failed to create dos device M: mapped"
		                " to %S: %lu\n", path, GetLastError());
		goto exit;
	}


	if (mm_spawn(&pid, argv[0], 0, NULL, 0, argv, NULL)
	   || mm_wait_process(pid, &status)
	   || !(status & MM_WSTATUS_EXITED))
		goto umount;

	exitcode = status & MM_WSTATUS_CODEMASK;

umount:
	// Try remove temporary drive letter M:
	if (!DefineDosDeviceW(DDD_NO_BROADCAST_SYSTEM|DDD_REMOVE_DEFINITION, MOUNT_TARGET, NULL))
		fprintf(stderr, "Warning: Failed to revert dos device: %lu\n", GetLastError());
exit:
	free(path);
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
		return mm_execv(argv[0], 0, NULL, 0, argv, NULL);

	mount_and_exec(prefix, argv);
}

