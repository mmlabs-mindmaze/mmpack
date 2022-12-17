/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "mmpack-check-sysdep.h"

#include "context.h"
#include "strset.h"
#include "sysdeps.h"


/**
 * mmpack_check_sysdep() - Check system deps specified on cmdline are fulfilled
 * @ctx: mmpack context
 * @argc: number of arguments
 * @argv: array of arguments
 *
 * Return: 0 on success, -1 otherwise
 */
LOCAL_SYMBOL
int mmpack_check_sysdep(struct mmpack_ctx * ctx, int argc, char const* argv[])
{
	(void)ctx;
	int i, rv;
	struct strset sysdeps;

	if (argc < 2) /* missing package argument */
		return -1;

	/* convert argument to strset struct */
	strset_init(&sysdeps, STRSET_FOREIGN_STRINGS);
	for (i = 1; i < argc; i++)
		strset_add(&sysdeps, argv[i]);

	rv = check_sysdeps_installed(&sysdeps);
	strset_deinit(&sysdeps);

	return rv;
}
