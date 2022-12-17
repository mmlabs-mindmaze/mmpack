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
#include "strset.h"
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
void mark_needed(struct mmpack_ctx * ctx, struct binpkg const * pkg,
                 int * needed)
{
	struct pkgdep * deps;
	struct binpkg const * pkg_dep;

	if (needed[pkg->name_id])
		return;

	needed[pkg->name_id] = 1;

	for (deps = pkg->mpkdeps; deps != NULL; deps = deps->next) {
		pkg_dep = install_state_get_pkg(&ctx->installed, deps->name);
		if (needed[pkg_dep->name_id] == 0)
			mark_needed(ctx, pkg_dep, needed);
	}
}


static
struct pkg_request* find_to_remove(struct mmpack_ctx * ctx, int * needed)
{
	struct it_iterator iter;
	struct it_entry * entry;
	struct binpkg * pkg;
	struct pkg_request * head = NULL;
	struct pkg_request * elt;

	entry = it_iter_first(&iter, &(ctx->installed.idx));
	for (; entry != NULL; entry = it_iter_next(&iter)) {
		pkg = entry->value;

		if (needed[pkg->name_id] == 1)
			continue;

		// add the package name to to_remove list
		elt = xx_malloc(sizeof(*elt));
		elt->name = pkg->name;
		elt->next = head;
		head = elt;
	}

	return head;
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
	struct pkg_request * to_remove = NULL;
	struct pkg_request * elt_clean = NULL, * next = NULL;
	struct action_stack * act_stack = NULL;
	int arg_index, rv = -1;
	int * needed;
	int size;
	mmstr * curr;
	struct binpkg const * pkg;
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

	// Determine if there are some packages to remove
	for (curr = strset_iter_first(&iter, &ctx->manually_inst); curr;
	     curr = strset_iter_next(&iter)) {
		pkg = install_state_get_pkg(&ctx->installed, curr);
		mark_needed(ctx, pkg, needed);
	}

	to_remove = find_to_remove(ctx, needed);

	// Determine the stack of actions to perform
	act_stack = mmpkg_get_remove_list(ctx, to_remove);
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
	elt_clean = to_remove;
	while (elt_clean) {
		next = elt_clean->next;
		free(elt_clean);
		elt_clean = next;
	}

	free(needed);
	return rv;
}
