/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmlib.h>
#include <mmsysio.h>

#include "context.h"

#include "exec-in-prefix.h"


static
char* get_mount_prefix_bin(void)
{
	return PKGLIBEXECDIR "/mount-mmpack-prefix" EXEEXT;
}


LOCAL_SYMBOL
int exec_in_prefix(const char* prefix, const char* argv[], int no_prefix_mount)
{
	char** new_argv;
	int nargs;

	// Count number of element in argv
	for (nargs = 0; argv[nargs] != NULL; nargs++);

	// new_argv contains: 1 command, 1 prefix, nargs arguments from
	// argv and one terminating NULL => length = nargs+3
	new_argv = alloca((nargs+3) * sizeof(*new_argv));
	new_argv[0] = get_mount_prefix_bin();
	new_argv[1] = (char*)prefix;

	// args is terminated by NULL (either from argv or default_argv) and
	// this is not counted in nargs, thus if we copy nargs+1 element
	// from args, the NULL termination will be added to new_argv
	memcpy(new_argv+2, argv, (nargs+1) * sizeof(*argv));

	if (no_prefix_mount)
		new_argv = (char**)argv;

	return mm_execv(new_argv[0], 0, NULL, 0, new_argv, NULL);
}
