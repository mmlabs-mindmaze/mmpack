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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "cmdline.h"
#include "common.h"
#include "context.h"
#include "mmstring.h"
#if defined (_WIN32)
#include "path-win32.h"
#endif

#include "mmpack-run.h"

static int no_prefix_mount = 0;

static char run_doc[] =
	"\"mmpack run\" execute a program in the mmpack prefix. If no program "
	"is supplied, the default shell is executed.";


static const struct mm_arg_opt cmdline_optv[] = {
	{"n|no-prefix-mount", MM_OPT_NOVAL|MM_OPT_INT, "1",
	 {.iptr = &no_prefix_mount}, "Do not perform prefix mount"},
};


#if !defined (_WIN32)

static
char* get_mount_prefix_bin(void)
{
	return LIBEXECDIR "/mmpack/mount-mmpack-prefix" EXEEXT;
}

#else // _WIN32

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

#endif /* if !defined (_WIN32) */


static
char* expand_abspath(const char* prefix)
{
	char* res;

	// TODO Implement a proper version of realpath in mmlib
#if _WIN32
	res = _fullpath(NULL, prefix, 32768);
#else
	res = realpath(prefix, NULL);
#endif
	if (!res) {
		mm_raise_from_errno("Cannot expand %s", prefix);
		return NULL;
	}

	return res;
}


static
int prepend_env(const char* restrict nameenv, const char* restrict fmt, ...)
{
	char* envstr;
	va_list args;
	int rv, len;

	// Compute needed size for env
	va_start(args, fmt);
	len = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	envstr = xx_malloc(len);

	// Actual env string formatting
	va_start(args, fmt);
	vsprintf(envstr, fmt, args);
	va_end(args);

	rv = mm_setenv(nameenv, envstr, MM_ENV_PREPEND);

	free(envstr);

	return rv;
}


/**
 * set_run_env()
 * @ctx:        mmpack context
 *
 * Return: 0 in case of success, -1 otherwise.
 */
static
int setup_run_env(struct mmpack_ctx* ctx)
{
	char* full_prefix;
	const char* target;
	int rv = -1;

	full_prefix = expand_abspath(ctx->prefix);
	if (!full_prefix)
		return -1;

	target = no_prefix_mount ? full_prefix : MOUNT_TARGET;

	// Add prefix path to environment variables
	if (prepend_env("PATH", "%s/bin", target)
	    || prepend_env("CPATH", "%s/include", target)
	    || prepend_env("LIBRARY_PATH", "%s/lib", target)
	    || prepend_env("PKG_CONFIG_PATH", "%s/lib/pkgconfig", target)
	    || prepend_env("MANPATH", "%s/share/man", target)
	    || prepend_env("PYTHONPATH", "%s/lib/python3/site-packages", target)
	    || mm_setenv("MMPACK_PREFIX", full_prefix, MM_ENV_OVERWRITE)
	    || mm_setenv("MMPACK_ACTIVE_PREFIX", full_prefix, MM_ENV_OVERWRITE))
		goto exit;

#if defined (_WIN32)
	// Convert environment path list that has been set/enriched here and
	// which are meant to be used in POSIX development environment (in
	// MSYS2 or Cygwin)
	conv_env_pathlist_win32_to_posix("CPATH");
	conv_env_pathlist_win32_to_posix("LIBRARY_PATH");
	conv_env_pathlist_win32_to_posix("MANPATH");
	conv_env_pathlist_win32_to_posix("ACLOCAL_PATH");
#endif
	rv = 0;

exit:
	free(full_prefix);
	return rv;
}


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
	int arg_index, nargs;
	int is_completing = mm_arg_is_completing();
	const char** args;
	char** new_argv;
	const char* default_shell_argv[] = {
		mm_getenv("SHELL", "sh"),
		NULL,
	};
	struct mm_arg_parser parser = {
		.flags = is_completing ? MM_ARG_PARSER_COMPLETION : 0,
		.doc = run_doc,
		.args_doc = RUN_SYNOPSIS,
		.optv = cmdline_optv,
		.num_opt = MM_NELEM(cmdline_optv),
		.execname = "mmpack",
	};

	arg_index = mm_arg_parse(&parser, argc, (char**)argv);

	// If completion is running, we need to offload completion of command
	// argument to the shell. However the prefix in use must be reported.
	// Hence we report a first string identifying the action to perform and
	// then prefix path is reported.
	if (is_completing) {
		printf("execute_run_completion\n");
		printf("%s\n", ctx->prefix);
		return 0;
	}

	// Copy command argument if supplied or launch default shell
	if (argc > arg_index) {
		args = argv + arg_index;
		nargs = argc - arg_index;
	} else {
		args = default_shell_argv;
		nargs = MM_NELEM(default_shell_argv) - 1;
	}

	if (ctx->prefix == NULL) {
		fprintf(stderr, "Prefix not set\n");
		return -1;
	}

	// new_argv contains: 1 command, 1 prefix, nargs arguments from
	// argv and one terminating NULL => length = nargs+3
	new_argv = alloca((nargs+3) * sizeof(*new_argv));
	new_argv[0] = get_mount_prefix_bin();
	new_argv[1] = ctx->prefix;

	// args is terminated by NULL (either from argv or default_argv) and
	// this is not counted in nargs, thus if we copy nargs+1 element
	// from args, the NULL termination will be added to new_argv
	memcpy(new_argv+2, args, (nargs+1) * sizeof(*args));

	if (no_prefix_mount)
		new_argv = (char**)args;

	if (setup_run_env(ctx))
		return -1;

	return mm_execv(new_argv[0], 0, NULL, 0, new_argv, NULL);
}
