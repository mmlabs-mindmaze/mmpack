/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
#include <config.h>
#endif

#include "cmdline.h"
#include "action-solver.h"
#include "mmpack-autoremove.h"
#include "mmpack-rdepends.h"
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


struct cb_data {
	struct mmpack_ctx * ctx;
	int * needed;
	struct pkg_request * to_remove;
	int cpt;
};


static
void mark_needed(struct mmpkg * pkg, struct cb_data * data)
{
	struct install_state is = data->ctx->installed;
	struct it_entry * entry;
	struct mmpkg_dep * deps;
	struct mmpkg * pkg_dep;	

	data->needed[pkg->name_id] = 1;
	for (deps = pkg->mpkdeps; deps; deps = deps->next) {
		entry = indextable_lookup(&is.idx, deps->name);
		pkg_dep = entry->value;
		if (data->needed[pkg_dep->name_id] == 0)
			mark_needed(pkg_dep, data);
	}	
}


static
int find_needed(struct mmpkg * pkg, void * void_data)
{	
	struct cb_data * data = (struct cb_data *) void_data;

	if (data->needed[pkg->name_id] == 0
	    && strset_contains(&data->ctx->manually_inst, pkg->name))
		mark_needed(pkg, data);	
	
	return 0;
}

	
static
int find_to_remove(struct mmpkg * pkg, void * void_data)
{
	struct cb_data * data = (struct cb_data*) void_data;

	if (data->needed[pkg->name_id] == 1)
		return 0;

	// add the package name to to_remove list
	data->cpt++;
	data->to_remove = xx_realloc(data->to_remove,
	                             data->cpt * sizeof(*data->to_remove));
	data->to_remove[data->cpt - 1].name = mmstr_malloc_from_cstr(pkg->name);
	data->to_remove[data->cpt - 1].next = NULL;
	if (data->cpt - 2 != -1)
		data->to_remove[data->cpt - 2].next =
			&data->to_remove[data->cpt - 1];

	return 0;
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
	struct cb_data data;
	struct action_stack * act_stack = NULL;
	int i, arg_index, rv = -1;
	int size;

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

	data.ctx = ctx;
	data.to_remove = NULL;
	data.cpt = 0;
	size = ctx->binindex.num_pkgname;
	data.needed = xx_malloc(sizeof(*data.needed) * size);
	memset(data.needed, 0, sizeof(*data.needed) * size);

	// Determine if there are some packages to remove
	installed_pkgs_foreach(ctx, find_needed, &data);
	installed_pkgs_foreach(ctx, find_to_remove, &data);

	// Determine the stack of actions to perform
	act_stack = mmpkg_get_remove_list(ctx, data.to_remove);
	if (!act_stack)
		goto exit;

	if (!is_yes_assumed) {
		rv = confirm_action_stack_if_needed(data.cpt, act_stack);
		if (rv != 0)
			goto exit;
	}

	rv = apply_action_stack(ctx, act_stack);

exit:
	mmpack_action_stack_destroy(act_stack);
	for (i = 0; i < data.cpt && data.to_remove; i++) {
		mmstr_free(data.to_remove[i].name);
	}

	free(data.needed);
	free(data.to_remove);
	return rv;
}
