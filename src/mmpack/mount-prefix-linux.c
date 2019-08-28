/*
 * @mindmaze_header@
 */

/**
 * DOC:
 *
 * After install, the root user must add the SYS_ADMIN capability to the
 * executable.  To do this, you may execute the following in a shell as root:
 *
 * setcap cap_sys_admin+ep <path_to_installed_mount_prefix_bin>
 */

#define _GNU_SOURCE
#include <errno.h>
#include <glob.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include "common.h"

static
void try_run_packaged_tool(char* argv[])
{
	int saved_error;
	glob_t res;
	size_t i;

	saved_error = errno;

	if (glob("/usr/lib/*/mmpack/mount-mmpack-prefix", 0, NULL, &res)) {
		errno = saved_error;
		return;
	}

	for (i = 0; i < res.gl_pathc; i++) {
		argv[0] = res.gl_pathv[i];
		execvp(argv[0], argv);
	}

	globfree(&res);

	errno = saved_error;
}


int main(int argc, char* argv[])
{
	const char* prefix_path;

	if (argc < 3) {
		fprintf(stderr, "usage %s <prefix> <command>\n", argv[0]);
		return EXIT_FAILURE;
	}

	prefix_path = argv[1];

	// Create a new mount namespace
	if (unshare(CLONE_NEWNS)) {
		try_run_packaged_tool(argv);
		perror("unshare failed");
		return EXIT_FAILURE;
	}

	// Do NOT propagate mount or umount events
	if (mount("none", "/", NULL, MS_REC|MS_PRIVATE, NULL)) {
		perror("Cannot change the propagation of mount events");
		return EXIT_FAILURE;
	}

	// Create mmpack mount point if it does not exists yet
	if (mkdir(MOUNT_TARGET, 0777) != 0 && errno != EEXIST) {
		perror("Could not create "MOUNT_TARGET);
		return EXIT_FAILURE;
	}

	// Do mount prefix path to mmpack mount target
	if (mount(prefix_path, MOUNT_TARGET, NULL, MS_BIND, NULL)) {
		fprintf(stderr, "mount of %s on "MOUNT_TARGET " failed: %s\n",
		        prefix_path, strerror(errno));
		return EXIT_FAILURE;
	}

	// Run command passed on argument.
	execvp(argv[2], argv+2);
	fprintf(stderr, "Failed to run %s: %s\n", argv[2], strerror(errno));
	return EXIT_FAILURE;
}

