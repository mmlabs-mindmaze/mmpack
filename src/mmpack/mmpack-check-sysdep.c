/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "indextable.h"
#include "sysdeps.h"

int main(int argc, char const ** argv)
{
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
