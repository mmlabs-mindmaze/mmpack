/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmargparse.h>

#include "action-solver.h"
#include "cmdline.h"
#include "context.h"
#include "mmpack-check-integrity.h"
#include "mmpack-fix-broken.h"
#include "pkg-fs-utils.h"

static char fix_broken_doc[] =
	"\"mmpack fix-broken\" resets an already-installed package to its original "
	"state. If no argument is given, try to fix all installed packages.";


struct cb_data {
	struct mmpack_ctx * ctx;
	int found;
	int error;
};


/* reinstall a single package */
static
int fix_broken_package(struct mmpack_ctx * ctx, mmstr const * pkg_name,
                       int unattended)
{
	int rv = -1;
	struct mmpkg const * installed_pkg;
	struct action_stack* stack;

	stack = mmpack_action_stack_create();
	installed_pkg = install_state_get_pkg(&ctx->installed, pkg_name);

	if (installed_pkg == NULL) {
		error("Package \"%s\" not found as installed.\n"
		      "fix-broken can only work on installed packages.\n",
		      pkg_name);
		goto exit;
	}

	stack = mmpack_action_stack_push(stack, INSTALL_PKG,
	                                 installed_pkg, NULL);

	if (!unattended) {
		rv = confirm_action_stack_if_needed(0, stack);
		if (rv != 0)
			goto exit;
	}

	rv = apply_action_stack(ctx, stack);

exit:
	mmpack_action_stack_destroy(stack);
	return rv;
}


static
int binindex_cb(struct mmpkg* pkg, void * void_data)
{
	int rv;
	mmstr * sha256sums;
	struct cb_data * data = (struct cb_data*) void_data;

	if (pkg->state == MMPACK_PKG_INSTALLED) {
		sha256sums = get_sha256sums_file(data->ctx->prefix, pkg->name);
		rv = check_pkg(data->ctx->prefix, sha256sums);
		mmstr_free(sha256sums);

		if (rv != 0) {
			data->found = 1;
			info("Trying to fix broken installed package: "
			     "%s (%s) ... \n", pkg->name, pkg->version);
			rv = fix_broken_package(data->ctx, pkg->name, 0);
			if (rv != 0) {
				data->error = 1;
				info("Failed!\n");
				return rv;
			}
		}
	}

	return 0;
}


/**
 * mmpack_fix_broken() - main function for the fix-broken command
 * @ctx: mmpack context
 * @argc: number of arguments
 * @argv: array of arguments
 *
 * Resets an already-installed package to its original state, meaning as if
 * it was just installed.
 *
 * Return: 0 on success, -1 otherwise
 */
LOCAL_SYMBOL
int mmpack_fix_broken(struct mmpack_ctx * ctx, int argc, const char ** argv)
{
	int i, nreq, arg_index, rv = 0;
	const char** req_args;
	mmstr * pkg_name;
	struct cb_data data = {.ctx = ctx};
	struct mm_arg_parser parser = {
		.flags = mm_arg_is_completing() ? MM_ARG_PARSER_COMPLETION : 0,
		.doc = fix_broken_doc,
		.args_doc = FIX_BROKEN_SYNOPSIS,
		.execname = "mmpack",
	};

	arg_index = mm_arg_parse(&parser, argc, (char**)argv);
	nreq = argc - arg_index;
	req_args = argv + arg_index;
	if (mm_arg_is_completing())
		return complete_pkgname(ctx, argv[argc-1], ONLY_INSTALLED);

	/* Load prefix configuration and caches */
	if (mmpack_ctx_use_prefix(ctx, 0))
		return -1;

	/* Fill package requested to be fixed from cmd arguments */
	for (i = 0; i < nreq; i++) {
		pkg_name = mmstr_malloc_from_cstr(req_args[i]);
		rv = fix_broken_package(ctx, pkg_name, 1);
		mmstr_free(pkg_name);

		if (rv != 0)
			break;
	}

	if (nreq == 0) {
		binindex_foreach(&ctx->binindex, binindex_cb, (void*) &data);
		if (data.found) {
			if (data.error) {
				info("Failure! You have held broken packages.\n");
				rv = -1;
			} else {
				info("Success! Fixed all the broken packages.\n");
			}
		}
	}

	return rv;
}
