/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
#include <config.h>
#endif

static int recursive = 1;


static
int analyze_remove(struct mmpkg * pkg, void * void_data)
{
	struct list_pkgs * rdep_list = NULL;


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
	if (argc != 1) {
		fprintf(stderr, "Bad usage of rdepends command.\n"
		        "Usage:\n\tmmpack "AUTOREMOVE_SYNOPSIS "\n");
		return -1;
	}

	// Load prefix configuration and caches
	if (mmpack_ctx_use_prefix(ctx, 0))
		return -1;

	binindex_foreach(&ctx->binindex, analyze_remove, NULL);

	return 0;
}
