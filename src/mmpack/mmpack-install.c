/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "mmpack-install.h"

#include <mmargparse.h>
#include <mmerrno.h>
#include <string.h>

#include "cmdline.h"
#include "context.h"
#include "package-utils.h"
#include "pkg-fs-utils.h"
#include "xx-alloc.h"


static int is_yes_assumed = 0;

static char install_doc[] =
	"\"mmpack install\" downloads and installs given packages and "
	"their dependencies into the current prefix. If mmpack finds "
	"missing systems dependencies, then it will abort the installation "
	"and request said packages.";

static const struct mm_arg_opt cmdline_optv[] = {
	{"y|assume-yes", MM_OPT_NOVAL|MM_OPT_INT, "1", {.iptr = &is_yes_assumed},
	 "assume \"yes\" as answer to all prompts and run non-interactively"},
};


/**
 * pkg_parser_translate_to_pkg_request() - fill pkg_request from pkg_parser
 * @pp: the pkg_parser structure
 * @req: the pkg_request structure to fill
 */
static
void pkg_parser_translate_to_pkg_request(struct mmpack_ctx * ctx,
                                         struct pkg_parser * pp,
                                         struct pkg_request * req)
{
	if (pp->pkg) {
		req->pkg = pp->pkg;
		return;
	} else if (pp->cons.sumsha) {
		req->pkg = binindex_lookup(&ctx->binindex, pp->name, &pp->cons);
		if (req->pkg)
			return;
	}

	req->name = pp->name ? mmstr_malloc_from_cstr(pp->name) : NULL;
	if (pp->cons.version)
		req->version = mmstr_malloc_from_cstr(pp->cons.version);
}


static
int fill_pkgreq_from_cmdarg(struct mmpack_ctx * ctx, struct pkg_request * req,
                            const char* arg)
{
	struct pkg_parser pp;

	pkg_parser_init(&pp);

	if (parse_pkgreq(ctx, arg, &pp))
		return -1;

	pkg_parser_translate_to_pkg_request(ctx, &pp, req);

	pkg_parser_deinit(&pp);
	return 0;
}


/**
 * mmpack_install() - main function for the install command
 * @ctx: mmpack context
 * @argc: number of arguments
 * @argv: array of arguments
 *
 * installs given packages and their dependencies into the current prefix.
 *
 * Return: 0 on success, -1 otherwise
 */
LOCAL_SYMBOL
int mmpack_install(struct mmpack_ctx * ctx, int argc, const char* argv[])
{
	struct pkg_request* reqlist = NULL;
	struct action_stack* act_stack = NULL;
	int i, nreq, arg_index, rv = -1;
	const char** req_args;
	struct mm_arg_parser parser = {
		.flags = mm_arg_is_completing() ? MM_ARG_PARSER_COMPLETION : 0,
		.doc = install_doc,
		.args_doc = INSTALL_SYNOPSIS,
		.optv = cmdline_optv,
		.num_opt = MM_NELEM(cmdline_optv),
		.execname = "mmpack",
	};

	arg_index = mm_arg_parse(&parser, argc, (char**)argv);
	if (mm_arg_is_completing())
		return complete_pkgname(ctx, argv[argc-1], AVAILABLE_PKGS);

	if (arg_index+1 > argc) {
		fprintf(stderr,
		        "missing package list argument in command line\n"
		        "Run \"mmpack install --help\" to see usage\n");
		return -1;
	}

	nreq = argc - arg_index;
	req_args = argv + arg_index;

	// Load prefix configuration and caches
	if (mmpack_ctx_use_prefix(ctx, 0))
		goto exit;

	// Fill package requested to be installed from cmd arguments
	reqlist = xx_malloca(nreq * sizeof(*reqlist));
	memset(reqlist, 0, nreq * sizeof(*reqlist));
	for (i = 0; i < nreq; i++) {
		if (fill_pkgreq_from_cmdarg(ctx, &reqlist[i], req_args[i]) < 0)
			goto exit;

		reqlist[i].next = (i == nreq-1) ? NULL : &reqlist[i+1];
	}

	// Determine the stack of actions to perform
	act_stack = mmpkg_get_install_list(ctx, reqlist);
	if (act_stack == NULL) {
		printf("Abort: failed to compute action course\n");
		goto exit;
	}

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
		/* do not free reqlist package */
	}

	mm_freea(reqlist);
	return rv;
}
