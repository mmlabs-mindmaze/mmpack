/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmargparse.h>
#include <mmsysio.h>

#include "action-solver.h"
#include "cmdline.h"
#include "context.h"
#include "mmpack-check-integrity.h"
#include "mmpack-fix-broken.h"
#include "pkg-fs-utils.h"

static char fix_broken_doc[] =
	"\"mmpack fix-broken\" resets an already-installed package to its original "
	"state. If no argument is given, try to fix all installed packages.";


/* reinstall a single package */
static
int fix_broken_package(struct mmpack_ctx * ctx, mmstr const * pkg_name,
                       int unattended)
{
	int rv = -1;
	struct binpkg const * installed_pkg;
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
int fix_broken_installed_packages(struct mmpack_ctx * ctx)
{
	struct inststate_iter iter;
	const struct binpkg* pkg;
	int rv = 0;

	// Loop over installed packages
	pkg = inststate_first(&iter, &ctx->installed);
	for (; pkg != NULL && rv == 0; pkg = inststate_next(&iter)) {
		if (check_installed_pkg(ctx, pkg) == 0)
			continue;

		info("Trying to fix broken installed package: "
		     "%s (%s) ... \n", pkg->name, pkg->version);
		rv = fix_broken_package(ctx, pkg->name, 0);
		info("%s!\n", rv == 0 ? "Done" : "Failed");
	}

	if (rv < 0) {
		info("Failure! You have held broken packages.\n");
	} else {
		info("Success! Fixed all the broken packages.\n");
	}

	return rv;
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

	// Load prefix configuration and caches and move to prefix directory
	if (mmpack_ctx_use_prefix(ctx, 0)
	    || mm_chdir(ctx->prefix))
		return -1;

	/* Fill package requested to be fixed from cmd arguments */
	for (i = 0; i < nreq; i++) {
		pkg_name = mmstr_malloc_from_cstr(req_args[i]);
		rv = fix_broken_package(ctx, pkg_name, 1);
		mmstr_free(pkg_name);

		if (rv != 0)
			break;
	}

	if (nreq == 0)
		rv = fix_broken_installed_packages(ctx);

	// Restore previous current directory
	mm_chdir(ctx->cwd);

	return rv;
}
