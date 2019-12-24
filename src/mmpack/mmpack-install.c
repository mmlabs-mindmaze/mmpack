/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "mmpack-install.h"

#include <mmargparse.h>
#include <mmerrno.h>
#include <mmlib.h>
#include <mmsysio.h>
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

static const struct mmarg_opt cmdline_optv[] = {
	{"y|assume-yes", MMOPT_NOVAL|MMOPT_INT, "1", {.iptr = &is_yes_assumed},
	 "assume \"yes\" as answer to all prompts and run non-interactively"},
};


static
int is_file(char const * path)
{
	struct mm_stat st;

	if (mm_stat(path, &st, 0) != 0)
		return 0;

	return S_ISREG(st.mode);
}


static
int fill_pkgreq_from_cmdarg(struct mmpack_ctx * ctx, struct pkg_request * req,
                            const char* arg)
{
	int len;
	struct mmpkg * pkg;
	mmstr * tmp, * arg_full;

	if (is_file(arg)) {
		tmp = mmstr_alloca_from_cstr(arg);
		len = mmstrlen(ctx->cwd) + 1 + mmstrlen(tmp);
		arg_full = mmstr_malloca(len);
		mmstr_join_path(arg_full, ctx->cwd, tmp);

		pkg = add_pkgfile_to_binindex(&ctx->binindex, arg_full);
		mmstr_freea(arg_full);
		if (pkg == NULL)
			return -1;

		req->pkg = pkg;
		req->name = NULL;
		req->version = NULL;

		return 0;
	}

	return 1;
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
	struct pkg_request * tmp = NULL;
	struct action_stack* act_stack = NULL;
	int i, nreq, arg_index, ret, rv = -1;
	const char** req_args;
	struct mmarg_parser parser = {
		.flags = mmarg_is_completing() ? MMARG_PARSER_COMPLETION : 0,
		.doc = install_doc,
		.args_doc = INSTALL_SYNOPSIS,
		.optv = cmdline_optv,
		.num_opt = MM_NELEM(cmdline_optv),
		.execname = "mmpack",
	};

	arg_index = mmarg_parse(&parser, argc, (char**)argv);
	if (mmarg_is_completing())
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
		if ((ret = fill_pkgreq_from_cmdarg(ctx, &reqlist[i], req_args[i]))
		    < 0)
			goto exit;
		else if (ret == 1) {
			tmp = parse_pkgreq(req_args[i]);
			reqlist[i] = *tmp;
		}

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
	pkg_request_deinit(&reqlist);
	mm_freea(reqlist);
	return rv;
}
