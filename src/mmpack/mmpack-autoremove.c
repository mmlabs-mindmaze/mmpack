/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
#include <config.h>
#endif

#include "action-solver.h"
#include "mmpack-autoremove.h"
#include "mmpack-rdepends.h"
#include "pkg-fs-utils.h"
#include "xx-alloc.h"


struct cb_data {
	struct mmpack_ctx * ctx;
	struct pkg_request * to_remove;
	int cpt;
};


static
int analyze_remove(struct mmpkg * pkg, void * void_data)
{
	struct cb_data * data = (struct cb_data*) void_data;
	struct list_pkgs * rdep_list = NULL;
	struct list_pkgs * curr;
	int rv = 0;

	// if the package analyzed is manually installed we cannot remove it
	if (strset_contains(&data->ctx->manually_inst, pkg->name))
		return 0;

	if (find_reverse_dependencies(data->ctx->binindex, pkg, NULL,
	                              &rdep_list, 1)) {
		rv = -1;
		goto exit;
	}

	for (curr = rdep_list; curr; curr = curr->next) {
		// if the package analyzed possessed manually installed reverse
		// dependencies, we cannot remove it.
		if (strset_contains(&data->ctx->manually_inst, curr->pkg->name))
			goto exit;
	}

	// add the package name to to_remove list
	data->cpt++;
	data->to_remove = xx_realloc(data->to_remove,
	                             data->cpt * sizeof(*data->to_remove));
	data->to_remove[data->cpt - 1].name = mmstr_malloc_from_cstr(pkg->name);
	data->to_remove[data->cpt - 1].next = NULL;
	if (data->cpt - 2 != -1)
		data->to_remove[data->cpt - 2].next =
			&data->to_remove[data->cpt - 1];

exit:
	list_pkgs_destroy_all_elt(&rdep_list);
	return rv;
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
	int i, rv = -1;

	data.ctx = ctx;
	data.to_remove = NULL;
	data.cpt = 0;

	if (argc == 2
	    && (STR_EQUAL(argv[1], strlen(argv[1]), "--help")
	        || STR_EQUAL(argv[1], strlen(argv[1]), "-h"))) {
		fprintf(stderr, "Usage:\n\tmmpack "AUTOREMOVE_SYNOPSIS "\n");
		return 0;
	}

	if (argc != 1) {
		fprintf(stderr, "Bad usage of rdepends command.\n"
		        "Usage:\n\tmmpack "AUTOREMOVE_SYNOPSIS "\n");
		return -1;
	}

	// Load prefix configuration and caches
	if (mmpack_ctx_use_prefix(ctx, 0))
		return -1;

	// Determine if there are some packages to remove
	installed_pkgs_foreach(ctx, analyze_remove, &data);

	// Determine the stack of actions to perform
	act_stack = mmpkg_get_remove_list(ctx, data.to_remove);
	if (!act_stack)
		goto exit;

	rv = apply_action_stack(ctx, act_stack);

exit:
	mmpack_action_stack_destroy(act_stack);
	for (i = 0; i < data.cpt && data.to_remove; i++) {
		mmstr_free(data.to_remove[i].name);
	}

	free(data.to_remove);
	return rv;
}
