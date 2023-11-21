/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmargparse.h>
#include <stdbool.h>
#include <stdio.h>

#include "action-solver.h"
#include "cmdline.h"
#include "common.h"
#include "context.h"
#include "mmpack-upgrade.h"
#include "pkg-fs-utils.h"


static int is_yes_assumed = 0;

static char upgrade_doc[] =
	"\"mmpack upgrade\" upgrades given packages, or all possible packages "
	"if no package is given";


static const struct mm_arg_opt cmdline_optv[] = {
	{"y|assume-yes", MM_OPT_NOVAL|MM_OPT_INT, "1",
	 {.iptr = &is_yes_assumed},
	 "assume \"yes\" as answer to all prompts and run non-interactively"},
};


static
struct pkg_request* get_full_upgradeable_reqlist(struct mmpack_ctx* ctx)
{
	struct pkg_request * req, * reqlist;
	struct it_iterator iter;
	struct it_entry * entry;
	const struct binpkg* pkg;

	reqlist = NULL;

	entry = it_iter_first(&iter, &ctx->installed.idx);
	for (; entry != NULL; entry = it_iter_next(&iter)) {
		pkg = entry->value;
		if (!binindex_is_pkg_upgradeable(&ctx->binindex, pkg))
			continue;

		req = xx_malloc(sizeof(*req));
		req->name = pkg->name;
		req->version = NULL;
		req->next = reqlist;
		reqlist = req;
	}

	return reqlist;
}


static
void clean_reqlist(struct pkg_request* reqlist)
{
	struct pkg_request* next;

	while (reqlist) {
		next = reqlist->next;
		free(reqlist);
		reqlist = next;
	}
}


/**
 * get_upgradeable_reqlist() - gets the packages among the one asked that
 *                             are effectively upgradeable (they are not already
 *                             at their latest version possible)
 * @ctx: mmpack context
 * @nreq: number of packages asked to be upgraded
 * @req_args: names of the packages asked to be upgraded
 * @reqlist: pointer which receives the resulting pkg_request list (list of the
 *           packages that will effectively be upgraded).
 *
 * Return: 0 on success, -1 otherwise.
 */
static
int get_upgradeable_reqlist(struct mmpack_ctx* ctx, int nreq,
                            char const ** req_args,
                            struct pkg_request ** reqlist)
{
	int i;
	struct pkg_request * req;
	mmstr * pkg_name;
	const struct binpkg* pkg;

	*reqlist = NULL;

	for (i = 0; i < nreq; i++) {
		pkg_name = mmstr_malloc_from_cstr(req_args[i]);
		pkg = install_state_get_pkg(&ctx->installed, pkg_name);

		if (!pkg) {
			clean_reqlist(*reqlist);
			*reqlist = NULL;
			printf("No package \"%s\" installed\n", pkg_name);
			printf("Abort\n");
			return -1;
		}

		mmstr_free(pkg_name);

		if (!binindex_is_pkg_upgradeable(&ctx->binindex, pkg)) {
			printf("Package \"%s\" is already at its"
			       "latest possible version (%s).\n",
			       pkg->name, pkg->version);
			continue;
		}

		req = xx_malloc(sizeof(*req));
		req->name = pkg->name;
		req->version = NULL;
		req->next = *reqlist;
		*reqlist = req;
	}

	return 0;
}


static
int mmpack_upgrade_reqlist(struct mmpack_ctx * ctx, bool skip_confirm,
                           struct pkg_request* reqlist)
{

	struct action_stack* act_stack = NULL;
	int rv = -1;

	// Determine the stack of actions to perform
	act_stack = mmpkg_get_upgrade_list(ctx, reqlist);
	if (!act_stack)
		goto exit;

	if (!skip_confirm) {
		rv = confirm_action_stack_if_needed(0, act_stack);
		if (rv != 0)
			goto exit;
	}

	rv = apply_action_stack(ctx, act_stack);

exit:
	mmpack_action_stack_destroy(act_stack);
	return rv;

}


/**
 * mmpack_upgrade_from_repos() - upgrade package from repositories
 * @ctx: mmpack context
 * @skip_confirm: if true, just perform upgrade without asking confirmation
 * @nreq: number of package that must be explicitly upgraded. If 0 all
 *        upgreable packages will be upgraded.
 * @req_args: list of required package to be upgraded. ignored if @nreq is 0.
 *
 * installs available upgrades of all packages currently installed in the
 * current prefix if @nreq is 0. Otherwise only listed packages will be
 * upgraded. New packages will be installed if required to satisfy
 * dependencies.
 *
 * NOTE: this function assumes that @ctx has been initialized with package list
 * (mmpack_ctx_use_prefix(ctx, 0) or mmpack_ctx_init_pkglist(ctx)).
 *
 * Return: 0 on success, -1 otherwise
 */
int mmpack_upgrade_from_repos(struct mmpack_ctx* ctx, bool skip_confirm,
                              int nreq, const char** req_args)
{
	int rv;
	struct pkg_request* reqlist;

	if (nreq == 0) {
		reqlist = get_full_upgradeable_reqlist(ctx);
	} else {
		if (get_upgradeable_reqlist(ctx, nreq, req_args, &reqlist) != 0)
			return -1;
	}

	rv = 0;
	if (reqlist != NULL) {
		rv = mmpack_upgrade_reqlist(ctx, skip_confirm, reqlist);
		clean_reqlist(reqlist);
	}

	return rv;
}


/**
 * mmpack_upgrade() - main function for the upgrade command
 * @ctx: mmpack context
 * @argc: number of arguments
 * @argv: array of arguments
 *
 * installs available upgrades of all packages currently installed in the
 * current prefix. New packages will be installed if required to satisfy
 * dependencies.
 *
 * Return: 0 on success, -1 otherwise
 */
LOCAL_SYMBOL
int mmpack_upgrade(struct mmpack_ctx * ctx, int argc, char const ** argv)
{
	int nreq, arg_index;
	const char** req_args;
	struct mm_arg_parser parser = {
		.flags = mm_arg_is_completing() ? MM_ARG_PARSER_COMPLETION : 0,
		.doc = upgrade_doc,
		.args_doc = UPGRADE_SYNOPSIS,
		.optv = cmdline_optv,
		.num_opt = MM_NELEM(cmdline_optv),
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

	return mmpack_upgrade_from_repos(ctx, is_yes_assumed, nreq, req_args);

}
