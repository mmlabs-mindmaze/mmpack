/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmargparse.h>
#include <stdio.h>

#include "action-solver.h"
#include "common.h"
#include "context.h"
#include "mmpack-upgrade.h"
#include "pkg-fs-utils.h"


static int is_yes_assumed = 0;

static char upgrade_doc[] =
	"\"mmpack upgrade\" upgrades given packages, or all possible packages "
	"if no package is given";


static const struct mmarg_opt cmdline_optv[] = {
	{"y|assume-yes", MMOPT_NOVAL|MMOPT_INT, "1", {.iptr = &is_yes_assumed},
	 "assume \"yes\" as answer to all prompts and run non-interactively"},
};


static
struct pkg_request* get_full_upgradable_reqlist(struct mmpack_ctx* ctx)
{
	STATIC_CONST_MMSTR(any_version, "any")
	struct pkg_request *req, *reqlist;
	struct it_iterator iter;
	struct it_entry *entry;
	const struct mmpkg* pkg;
	const struct mmpkg* latest;

	reqlist = NULL;

	entry = it_iter_first(&iter, &ctx->installed.idx);
	for (; entry != NULL; entry = it_iter_next(&iter)) {
		pkg = entry->value;
		latest = binindex_get_latest_pkg(&ctx->binindex, pkg->name,
		                                 any_version);
		if (pkg_version_compare(pkg->version, latest->version) >= 0)
			continue;

		req = mm_malloc(sizeof(*req));
		req->name = pkg->name;
		req->version = NULL;
		req->next = reqlist;
		reqlist = req;
	}

	return reqlist;
}


static
struct pkg_request* get_upgradable_reqlist(struct mmpack_ctx* ctx, int nreq,
                                           char const ** req_args)
{
	STATIC_CONST_MMSTR(any_version, "any")
	int i;
	struct pkg_request *req, *reqlist;
	mmstr * pkg_name;
	const struct mmpkg* pkg;
	const struct mmpkg* latest;

	reqlist = NULL;

	for (i = 0 ; i < nreq ; i++) {
		pkg_name = mmstr_malloc_from_cstr(req_args[i]);
		pkg = install_state_get_pkg(&ctx->installed, pkg_name);
		mmstr_free(pkg_name);
		latest = binindex_get_latest_pkg(&ctx->binindex, pkg->name, any_version);

		if (pkg_version_compare(pkg->version, latest->version) >= 0) {
			printf("Package \"%s\" is already at its latest possible version (%s).\n",
					pkg->name, pkg->version);
			continue;
		}

		req = mm_malloc(sizeof(*req));
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


static
int mmpack_upgrade_reqlist(struct mmpack_ctx * ctx, struct pkg_request* reqlist)
{

	struct action_stack* act_stack = NULL;
	int rv = -1;

	// Determine the stack of actions to perform
	act_stack = mmpkg_get_upgrade_list(ctx, reqlist);
	if (!act_stack)
		goto exit;

	if (!is_yes_assumed) {
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
	int nreq, arg_index, rv;
	const char** req_args;
	struct pkg_request* reqlist;
	struct mmarg_parser parser = {
		.doc = upgrade_doc,
		.args_doc = UPGRADE_SYNOPSIS,
		.optv = cmdline_optv,
		.num_opt = MM_NELEM(cmdline_optv),
		.execname = "mmpack",
	};

	arg_index = mmarg_parse(&parser, argc, (char**)argv);
	nreq = argc - arg_index;
	req_args = argv + arg_index;

	/* Load prefix configuration and caches */
	if (mmpack_ctx_use_prefix(ctx, 0))
		return -1;

	if (nreq == 0)
		reqlist = get_upgradable_reqlist(ctx, nreq, req_args);
	else
		reqlist = get_full_upgradable_reqlist(ctx);

	rv = 0;
	if (reqlist != NULL) {
		rv = mmpack_upgrade_reqlist(ctx, reqlist);
		clean_reqlist(reqlist);
	}

	return rv;
}
