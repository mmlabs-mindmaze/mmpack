/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <stdio.h>
#include <string.h>

#include "context.h"
#include "mmpack-remove.h"
#include "mmstring.h"
#include "package-utils.h"
#include "pkg-fs-utils.h"


static
void warn_uninstalled_package(const struct mmpack_ctx* ctx,
                              const struct pkg_request* reqlist)
{
	const struct pkg_request* req;

	for (req = reqlist; req; req = req->next) {
		if (install_state_get_pkg(&ctx->installed, req->name) != NULL)
			continue;

		printf("%s is not installed, thus will not be removed\n",
		       req->name);
	}
}


LOCAL_SYMBOL
int mmpack_remove(struct mmpack_ctx * ctx, int argc, const char* argv[])
{
	struct pkg_request* reqlist = NULL;
	struct action_stack* act_stack = NULL;
	int i, nreq, rv = -1;
	const char** req_args;

	if (argc < 2) {
		fprintf(stderr, "missing package list argument in command line\n"
		                "Usage:\n\tmmpack "REMOVE_SYNOPSIS"\n");
		return -1;
	}
	nreq = argc - 1;
	req_args = argv + 1;

	if (mmpack_ctx_init_pkglist(ctx)) {
		fprintf(stderr, "Failed to load package lists\n");
		goto exit;
	}

	// Fill package requested to be removed from cmd arguments
	reqlist = mm_malloca(nreq * sizeof(*reqlist));
	memset(reqlist, 0, nreq * sizeof(*reqlist));
	for (i = 0; i < nreq; i++) {
		reqlist[i].name = mmstr_malloc_from_cstr(req_args[i]);
		reqlist[i].next = (i == nreq-1) ? NULL : &reqlist[i+1];
	}

	// Determine the stack of actions to perform
	warn_uninstalled_package(ctx, reqlist);
	act_stack = mmpkg_get_remove_list(ctx, reqlist);
	if (!act_stack)
		goto exit;

	rv = confirm_action_stack_if_needed(nreq, act_stack);
	if (rv != 0)
		goto exit;

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
