/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmargparse.h>
#include <mmerrno.h>
#include <mmlib.h>
#include <mmsysio.h>
#include <string.h>

#include "common.h"
#include "context.h"
#include "mmstring.h"
#if defined (_WIN32)
#include "path-win32.h"
#endif

#include "mmpack-run.h"

#define MOUNT_PREFIX_BIN LIBEXECDIR "/mmpack/mount-mmpack-prefix"EXEEXT

/**
 * mmpack_run() - main function for the command to run commands
 * @ctx: mmpack context
 * @argc: number of arguments
 * @argv: array of arguments
 *
 * Runs given command within current prefix.
 * If no command was given, it will open an interactive shell within the current
 * prefix instead.
 *
 * Return: 0 on success, -1 otherwise
 */
LOCAL_SYMBOL
int mmpack_run(struct mmpack_ctx * ctx, int argc, const char* argv[])
{
	int nargs;
	const char** args;
	char** new_argv;
	const char* default_shell_argv[] = {
		mm_getenv("SHELL", "sh"),
		NULL,
	};

	// If completion is running, we need to offload completion of command
	// argument to the shell. However the prefix in use must be reported.
	// Hence we report a first string identifying the action to perform and
	// then prefix path is reported.
	if (mmarg_is_completing()) {
		printf("execute_run_completion\n");
		printf("%s\n", ctx->prefix);
		return 0;
	}

	if (argc == 2
	    && (STR_EQUAL(argv[1], strlen(argv[1]), "--help")
	        || STR_EQUAL(argv[1], strlen(argv[1]), "-h"))) {
		fprintf(stderr, "Usage:\n\tmmpack "RUN_SYNOPSIS "\n");
		return 0;
	}

	// Copy command argument if supplied or launch default shell
	if (argv[1] != NULL) {
		args = argv + 1;
		nargs = argc - 1;
	} else {
		args = default_shell_argv;
		nargs = MM_NELEM(default_shell_argv) - 1;
	}

	// new_argv contains: 1 command, 1 prefix, nargs arguments from
	// argv and one terminating NULL => length = nargs+3
	new_argv = alloca((nargs+3) * sizeof(*new_argv));
	new_argv[0] = MOUNT_PREFIX_BIN;
	new_argv[1] = ctx->prefix;

	// args is terminated by NULL (either from argv or default_argv) and
	// this is not counted in nargs, thus if we copy nargs+1 element
	// from args, the NULL termination will be added to new_argv
	memcpy(new_argv+2, args, (nargs+1) * sizeof(*args));

	// Add prefix path to environment variables
	if (mm_setenv("PATH", MOUNT_TARGET "/bin", MM_ENV_PREPEND)
	    || mm_setenv("CPATH", MOUNT_TARGET "/include", MM_ENV_PREPEND)
	    || mm_setenv("LIBRARY_PATH", MOUNT_TARGET "/lib", MM_ENV_PREPEND)
	    || mm_setenv("MANPATH", MOUNT_TARGET "/share/man", MM_ENV_PREPEND)
	    || mm_setenv("MMPACK_PREFIX", ctx->prefix, MM_ENV_OVERWRITE)
	    || mm_setenv("MMPACK_ACTIVE_PREFIX", ctx->prefix, MM_ENV_OVERWRITE)
	    /* XXX: check PYTHONPATH value once a pure-python mmpack package has
	     * been created */
	    || mm_setenv("PYTHONPATH",
	                 MOUNT_TARGET "/lib/python3/site-packages",
	                 MM_ENV_PREPEND) )
		return -1;

#if defined (_WIN32)
	// Convert environment path list that has been set/enriched here and
	// which are meant to be used in POSIX development environment (in
	// MSYS2 or Cygwin)
	conv_env_pathlist_win32_to_posix("CPATH");
	conv_env_pathlist_win32_to_posix("LIBRARY_PATH");
	conv_env_pathlist_win32_to_posix("MANPATH");
#endif

	return mm_execv(new_argv[0], 0, NULL, 0, new_argv, NULL);
}
