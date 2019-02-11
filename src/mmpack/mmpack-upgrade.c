/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <stdio.h>

#include "action-solver.h"
#include "common.h"
#include "context.h"
#include "mmpack-upgrade.h"
#include "pkg-fs-utils.h"


static
struct pkg_request* get_upgradable_reqlist(struct mmpack_ctx* ctx)
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
	struct action_stack* act_stack = NULL;
	int rv = -1;
	struct pkg_request* reqlist;

	if (argc == 2
	    && (STR_EQUAL(argv[1], strlen(argv[1]), "--help")
	        || STR_EQUAL(argv[1], strlen(argv[1]), "-h"))) {
		fprintf(stderr, "Usage:\n\tmmpack "UPGRADE_SYNOPSIS"\n");
		return 0;
	}

	// Load prefix configuration and caches
	if (mmpack_ctx_use_prefix(ctx, 0))
		return -1;

	reqlist = get_upgradable_reqlist(ctx);
	if (!reqlist) {
		printf("Nothing to do.\n");
		return 0;
	}

	// Determine the stack of actions to perform
	act_stack = mmpkg_get_upgrade_list(ctx, reqlist);
	if (!act_stack)
		goto exit;

	rv = confirm_action_stack_if_needed(0, act_stack);
	if (rv != 0)
		goto exit;

	rv = apply_action_stack(ctx, act_stack);

exit:
	mmpack_action_stack_destroy(act_stack);
	clean_reqlist(reqlist);
	return rv;
}
