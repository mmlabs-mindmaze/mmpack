/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mmargparse.h>
#include <mmsysio.h>

#include "mmpack-update.h"

#define STR_EQUAL(str, const_str) \
	memcmp(str, const_str, sizeof(const_str) - 1)

static
void usage(char const * progname)
{
	printf("usage:\n %s <comand> [options]\n", progname);
}

int main(int argc, char ** argv)
{
	char * command;

	if (argc < 2) {
		usage(argv[0]);
		return -1;
	}
	command = argv[1];

	if (STR_EQUAL(command, "update")) {
		return mmpack_update_all();
	} else {
		usage(argv[0]);
		return -1;
	}

	return 0;
}
