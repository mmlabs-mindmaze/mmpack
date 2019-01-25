/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmerrno.h>
#include <mmlib.h>
#include <mmsysio.h>
#include <string.h>

#include "common.h"
#include "context.h"
#include "mmstring.h"

#include "mmpack-run.h"

#define MOUNT_PREFIX_BIN        LIBEXECDIR"/mount-mmpack-prefix"EXEEXT

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
	if (mm_setenv("PATH", MOUNT_TARGET"/bin", MM_ENV_PREPEND) == 0
	    && mm_setenv("CPATH", MOUNT_TARGET"/include", MM_ENV_PREPEND) == 0
	    && mm_setenv("LIBRARY_PATH", MOUNT_TARGET"/lib", MM_ENV_PREPEND) == 0
	    && mm_setenv("MANPATH", MOUNT_TARGET"/share/man", MM_ENV_PREPEND) == 0
	    /* XXX: check PYTHONPATH value once a pure-python mmpack package has been created */
	    && mm_setenv("PYTHONPATH", MOUNT_TARGET"/lib/python3/site-packages", MM_ENV_PREPEND) == 0)
		return mm_execv(new_argv[0], 0, NULL, 0, new_argv, NULL);
	else
		return -1;
}
