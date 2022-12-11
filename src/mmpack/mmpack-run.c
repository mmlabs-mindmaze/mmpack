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
#include "exec-in-prefix.h"
#include "mmstring.h"
#include "utils.h"

#include "mmpack-run.h"

static int no_prefix_mount = 0;

static char run_doc[] =
	"\"mmpack run\" execute a program in the mmpack prefix. If no program "
	"is supplied, the default shell is executed.";


static const struct mm_arg_opt cmdline_optv[] = {
	{"n|no-prefix-mount", MM_OPT_NOVAL|MM_OPT_INT, "1",
	 {.iptr = &no_prefix_mount}, "Do not perform prefix mount"},
};


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
	int arg_index;
	int is_completing = mm_arg_is_completing();
	const char** args;
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
	} else {
		args = default_shell_argv;
	}

	if (ctx->prefix == NULL) {
		fprintf(stderr, "Prefix not set\n");
		return -1;
	}

	if (setup_run_env(ctx))
		return -1;

	return exec_in_prefix(ctx->prefix, args, no_prefix_mount);
}
