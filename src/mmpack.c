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
#include <mmerrno.h>
#include <mmsysio.h>

#include "mmpack-common.h"
#include "mmpack-update.h"


static
void usage(char const * progname)
{
	printf("usage:\n %s <comand> [options]\n", progname);
}

int main(int argc, char ** argv)
{
	int rv;
	char * command;
	struct mmpack_ctx ctx;

	if (argc < 2) {
		usage(argv[0]);
		return -1;
	}
	command = argv[1];

	rv = mmpack_ctx_init(&ctx);
	if (rv != 0)
		return mm_raise_from_errno("failed to init mmpack");


	if (STR_EQUAL(command, strlen(command), "update")) {
		rv = mmpack_update_all(&ctx);
	} else {
		usage(argv[0]);
		rv = -1;
	}

	mmpack_ctx_deinit(&ctx);

	return rv;
}
