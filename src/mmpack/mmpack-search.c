/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "mmpack-search.h"

#include <mmargparse.h>
#include <mmerrno.h>
#include <mmpredefs.h>

#include "context.h"
#include "mmpack-list.h"


/**
 * mmpack_search() - main function for the search command
 * @ctx: mmpack context
 * @argc: number of arguments
 * @argv: array of arguments
 *
 * searches given pattern within all the available package names.
 *
 * Return: 0 on success, -1 otherwise
 */
LOCAL_SYMBOL
int mmpack_search(struct mmpack_ctx * ctx, int argc, const char* argv[])
{
	if (mm_arg_is_completing())
		return 0;

	if (argc != 2
	    || STR_EQUAL(argv[1], strlen(argv[1]), "--help")
	    || STR_EQUAL(argv[1], strlen(argv[1]), "-h")) {
		fprintf(stderr, "Usage:\n\tmmpack "SEARCH_SYNOPSIS "\n");
		return argc != 2 ? -1 : 0;
	}

	const char* list_argv[] = {"list", "-g", "all", argv[1]};
	return mmpack_list(ctx, MM_NELEM(list_argv), list_argv);
}
