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

#include "common.h"
#include "mmpack-mkprefix.h"
#include "mmpack-update.h"

static const char mmpack_doc[] =
	"TODO write a proper tool description";

static const char arguments_docs[] =
	"[options] mkprefix\n"
	"[options] update\n";

static struct mmpack_opts cmdline_opts;

static const struct mmarg_opt cmdline_optv[] = {
	{"p|prefix", MMOPT_NEEDSTR, NULL, {.sptr = &cmdline_opts.prefix},
	 "Use @PATH as install prefix."},
};

int main(int argc, char* argv[])
{
	int rv, arg_index;
	const char* cmd;
	struct mmpack_ctx ctx;
	struct mmarg_parser parser = {
		.doc = mmpack_doc,
		.args_doc = arguments_docs,
		.optv = cmdline_optv,
		.num_opt = MM_NELEM(cmdline_optv),
		.execname = argv[0],
	};

	/* Parse commmand line options and check command is supplied */
	arg_index = mmarg_parse(&parser, argc, argv);
	if (arg_index+1 > argc) {
		fprintf(stderr, "Invalid number of argument."
		                " Run \"%s --help\" to see Usage\n", argv[0]);
		rv = -1;
		goto exit;
	}
	cmd = argv[arg_index];

	/* Initialize context according to command line options */
	rv = mmpack_ctx_init(&ctx, &cmdline_opts);
	if (rv != 0)
		goto exit;

	/* Dispatch command */
	if (STR_EQUAL(cmd, strlen(cmd), "mkprefix")) {
		rv = mmpack_mkprefix(&ctx);
	} else if (STR_EQUAL(cmd, strlen(cmd), "update")) {
		rv = mmpack_update_all(&ctx);
	} else {
		fprintf(stderr, "Invalid command: %s."
		                " Run \"%s --help\" to see Usage\n", cmd, argv[0]);
		rv = -1;
	}

exit:
	mmpack_ctx_deinit(&ctx);

	return (rv == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
