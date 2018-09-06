/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "mmpack-install.h"

#include <mmerrno.h>
#include <mmlib.h>
#include <mmsysio.h>
#include <string.h>
#include "context.h"
#include "package-utils.h"
#include "pkg-fs-utils.h"



static
void fill_pkgreq_from_cmdarg(struct pkg_request *req, const char* arg)
{
	const char* v;

	// Find the first occurence of '='
	v = strchr(arg, '=');
	if (v != NULL) {
		// The package name is before the '=' character
		req->name = mmstr_malloc_copy(arg, v - arg);
		req->version = mmstr_malloc_from_cstr(v+1);
	} else {
		req->name = mmstr_malloc_from_cstr(arg);
		req->version = NULL;
	}
}


LOCAL_SYMBOL
int mmpack_install(struct mmpack_ctx * ctx, int argc, const char* argv[])
{
	struct pkg_request* reqlist = NULL;
	struct action_stack* act_stack = NULL;
	int i, nreq, rv = -1;
	const char** req_args;

	if (argc < 2) {
		fprintf(stderr, "missing package list argument in command line\n"
		                "Usage:\n\tmmpack "INSTALL_SYNOPSIS"\n");
		return -1;
	}
	nreq = argc - 1;
	req_args = argv + 1;

	if (mmpack_ctx_init_pkglist(ctx)) {
		fprintf(stderr, "Failed to load package lists\n");
		goto exit;
	}

	// Fill package requested to be installed from cmd arguments
	reqlist = mm_malloca(nreq * sizeof(*reqlist));
	memset(reqlist, 0, nreq * sizeof(*reqlist));
	for (i = 0; i < nreq; i++) {
		fill_pkgreq_from_cmdarg(&reqlist[i], req_args[i]);
		reqlist[i].next = (i == nreq-1) ? NULL : &reqlist[i+1];
	}

	// Determine the stack of actions to perform
	act_stack = mmpkg_get_install_list(ctx, reqlist);
	if (!act_stack)
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
