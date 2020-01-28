/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
#include <config.h>
#endif

#include "cmdline.h"
#include "action-solver.h"
#include "mmpack-autoremove.h"
#include "pkg-fs-utils.h"
#include "xx-alloc.h"

static int is_yes_assumed = 0;

static char autoremove_doc[] =
	"\"mmpack autoremove\" removes all the packages that are not manually "
	"installed and that do not possessed manually installed reverse "
	"dependencies.";

static const struct mm_arg_opt cmdline_optv[] = {
	{"y|assume-yes", MM_OPT_NOVAL|MM_OPT_INT, "1",
	 {.iptr = &is_yes_assumed},
	 "assume \"yes\" as answer to all prompts and run non-interactively"},
};


static
void mark_needed(struct mmpack_ctx * ctx, struct mmpkg const * pkg,
                 int * needed)
{
	struct mmpkg_dep * deps;
	struct mmpkg const * pkg_dep;

	if (needed[pkg->name_id])
		return;

	needed[pkg->name_id] = 1;

	for (deps = pkg->mpkdeps; deps; deps = deps->next) {
		pkg_dep = install_state_get_pkg(&ctx->installed, deps->name);
		if (needed[pkg_dep->name_id] == 0)
			mark_needed(ctx, pkg_dep, needed);
	}
}


struct data_ctx {
	struct pkg_request * to_remove;
	int cpt;
};


static
void find_to_remove(struct mmpack_ctx * ctx, int * needed,
                    struct data_ctx * data)
{
	struct it_iterator iter;
	struct it_entry * entry;
	struct mmpkg * pkg;

	for (entry = it_iter_first(&iter, &(ctx->installed.idx)); entry;
	     entry = it_iter_next(&iter)) {
		pkg = entry->value;

		if (needed[pkg->name_id] == 1)
			return;

		// add the package name to to_remove list
		data->cpt++;
		data->to_remove = xx_realloc(data->to_remove,
		                             data->cpt *
		                             sizeof(*data->to_remove));
		data->to_remove[data->cpt - 1].name = mmstr_malloc_from_cstr(
			pkg->name);
		data->to_remove[data->cpt - 1].next = NULL;
		if (data->cpt - 2 != -1)
			data->to_remove[data->cpt - 2].next =
				&data->to_remove[data->cpt - 1];
	}
}


/**
 * mmpack_autoremove() - main function for the autoremove command
 * @ctx: mmpack context
 * @argc: number of arguments
 * @argv: array of arguments
 *
 * Removes all the packages that are not manually installed such that no reverse
 * dependencies are manually installed.
 *
 * Return: 0 on success, -1 otherwise
 */
LOCAL_SYMBOL
int mmpack_autoremove(struct mmpack_ctx * ctx, int argc, char const ** argv)
{
	struct data_ctx data;
	struct action_stack * act_stack = NULL;
	int i, arg_index, rv = -1;
	int * needed;
	int size;
	mmstr * curr;
	struct mmpkg const * pkg;
	struct strset_iterator iter;

	struct mm_arg_parser parser = {
		.flags = mm_arg_is_completing() ? MM_ARG_PARSER_COMPLETION : 0,
		.doc = autoremove_doc,
		.args_doc = AUTOREMOVE_SYNOPSIS,
		.optv = cmdline_optv,
		.num_opt = MM_NELEM(cmdline_optv),
		.execname = "mmpack",
	};

	arg_index = mm_arg_parse(&parser, argc, (char**)argv);

	if (arg_index != argc) {
		fprintf(stderr,
		        "Bad usage\n\n"
		        "Run \"mmpack autoremove --help\" to see usage\n");
		return -1;
	}

	// Load prefix configuration and caches
	if (mmpack_ctx_use_prefix(ctx, 0))
		return -1;

	size = ctx->binindex.num_pkgname;
	needed = xx_calloc(size, sizeof(*needed));
	data.to_remove = NULL;
	data.cpt = 0;

	// Determine if there are some packages to remove
	for (curr = strset_iter_first(&iter, &ctx->manually_inst); curr;
	     curr = strset_iter_next(&iter)) {
		pkg = install_state_get_pkg(&ctx->installed, curr);
		mark_needed(ctx, pkg, needed);
	}

	find_to_remove(ctx, needed, &data);

	// Determine the stack of actions to perform
	act_stack = mmpkg_get_remove_list(ctx, data.to_remove);
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
	for (i = 0; i < data.cpt && data.to_remove; i++) {
		mmstr_free(data.to_remove[i].name);
	}

	free(needed);
	free(data.to_remove);
	return rv;
}
