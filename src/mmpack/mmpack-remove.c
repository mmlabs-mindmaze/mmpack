/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmargparse.h>
#include <stdio.h>
#include <string.h>

#include "cmdline.h"
#include "context.h"
#include "mmpack-remove.h"
#include "mmstring.h"
#include "package-utils.h"
#include "pkg-fs-utils.h"
#include "xx-alloc.h"


static int is_yes_assumed = 0;

static char remove_doc[] =
	"\"mmpack remove\" removes given packages and the packages depending upon "
	"them from the current prefix. First mmpack* adds all given packages and "
	"the packages depending upon them to the current transaction. If any "
	"additional package is required, it will ask for user confirmation "
	"before proceeding (default answer is negative). "
	"Otherwise it will proceed to the removal.";

static const struct mm_arg_opt cmdline_optv[] = {
	{"y|assume-yes", MM_OPT_NOVAL|MM_OPT_INT, "1",
	 {.iptr = &is_yes_assumed},
	 "assume \"yes\" as answer to all prompts and run non-interactively"},
};

static
int warn_uninstalled_package(const struct mmpack_ctx* ctx,
                             const char * _name)
{
	mmstr * name = mmstr_alloca_from_cstr(_name);
	if (install_state_get_pkg(&ctx->installed, name) == NULL) {
		printf("%s is not installed, thus will not be removed\n", name);
		return -1;
	}

	return 0;
}


/**
 * mmpack_remove() - main function for the remove command
 * @ctx: mmpack context
 * @argc: number of arguments
 * @argv: array of arguments
 *
 * removes given packages and the packages depending upon them from the
 * current prefix.
 *
 * Return: 0 on success, -1 otherwise
 */
LOCAL_SYMBOL
int mmpack_remove(struct mmpack_ctx * ctx, int argc, const char* argv[])
{
	struct pkg_request* reqlist = NULL;
	struct action_stack* act_stack = NULL;
	int i, nreq, arg_index, rv = -1;
	const char** req_args;

	struct mm_arg_parser parser = {
		.flags = mm_arg_is_completing() ? MM_ARG_PARSER_COMPLETION : 0,
		.doc = remove_doc,
		.args_doc = REMOVE_SYNOPSIS,
		.optv = cmdline_optv,
		.num_opt = MM_NELEM(cmdline_optv),
		.execname = "mmpack",
	};

	arg_index = mm_arg_parse(&parser, argc, (char**)argv);
	if (mm_arg_is_completing())
		return complete_pkgname(ctx, argv[argc-1], ONLY_INSTALLED);

	if (arg_index+1 > argc) {
		fprintf(stderr,
		        "missing package list argument in command line\n"
		        "Run \"mmpack remove --help\" to see usage\n");
		return -1;
	}

	nreq = argc - arg_index;
	req_args = argv + arg_index;

	// Load prefix configuration and caches
	if (mmpack_ctx_use_prefix(ctx, 0))
		goto exit;

	// Fill package requested to be removed from cmd arguments
	reqlist = xx_malloca(nreq * sizeof(*reqlist));
	memset(reqlist, 0, nreq * sizeof(*reqlist));
	for (i = 0; i < nreq; i++) {
		if (warn_uninstalled_package(ctx, req_args[i]) == 0) {
			reqlist[i].name = mmstr_malloc_from_cstr(req_args[i]);
			reqlist[i].next = (i == nreq-1) ? NULL : &reqlist[i+1];
		}
	}

	if (reqlist[0].name == NULL)
		goto exit;

	// Determine the stack of actions to perform
	act_stack = mmpkg_get_remove_list(ctx, reqlist);
	if (!act_stack)
		goto exit;

	if (!is_yes_assumed) {
		rv = confirm_action_stack_if_needed(nreq, act_stack);
		if (rv != 0)
			goto exit;
	}

	rv = apply_action_stack(ctx, act_stack);

exit:
	mmpack_action_stack_destroy(act_stack);
	for (i = 0; i < nreq && reqlist; i++) {
		mmstr_free(reqlist[i].name);
		mmstr_free(reqlist[i].version);
	}

	mm_freea(reqlist);
	return rv;
}
