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
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>


#define MOUNT_TARGET    "/run/mmpack"

int main(int argc, char* argv[])
{
	const char* prefix_path;

	if (argc < 1) {
		fprintf(stderr, "Missing prefix path\n");
		return EXIT_FAILURE;
	}
	prefix_path = argv[1];

	if (argc < 2) {
		fprintf(stderr, "Missing command\n");
		return EXIT_FAILURE;
	}

	// Create a new mount namespace
	if (unshare(CLONE_NEWNS)) {
		perror("unshare failed");
		return EXIT_FAILURE;
	}

	// Do NOT propagate mount or umount events
	if (mount("none", "/", NULL, MS_REC|MS_PRIVATE, NULL)) {
		perror("Cannot change the propagation of mount events");
		return EXIT_FAILURE;
	}

	// Do mount prefix path to mmpack mount target
	if (mount(prefix_path, MOUNT_TARGET, NULL, MS_BIND, NULL)) {
		fprintf(stderr, "mount of %s on "MOUNT_TARGET" failed: %s\n",
		                prefix_path, strerror(errno));
		return EXIT_FAILURE;
	}

	// Run command passed on argument.
	execvp(argv[2], argv+2);
	fprintf(stderr, "Failed to run %s: %s\n", argv[2], strerror(errno));
	return EXIT_FAILURE;
}

