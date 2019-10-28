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

#include "cmdline.h"
#include "common.h"
#include "mmpack-check-integrity.h"
#include "mmpack-download.h"
#include "mmpack-fix-broken.h"
#include "mmpack-install.h"
#include "mmpack-list.h"
#include "mmpack-mkprefix.h"
#include "mmpack-provides.h"
#include "mmpack-remove.h"
#include "mmpack-repo.h"
#include "mmpack-run.h"
#include "mmpack-search.h"
#include "mmpack-show.h"
#include "mmpack-source.h"
#include "mmpack-update.h"
#include "mmpack-upgrade.h"

static
const struct subcmd mmpack_subcmds[] = {
	{"check-integrity", mmpack_check_integrity},
	{"download", mmpack_download},
	{"fix-broken", mmpack_fix_broken},
	{"info", mmpack_show},
	{"install", mmpack_install},
	{"list", mmpack_list},
	{"mkprefix", mmpack_mkprefix},
	{"provides", mmpack_provides},
	{"remove", mmpack_remove},
	{"repo", mmpack_repo},
	{"run", mmpack_run},
	{"search", mmpack_search},
	{"show", mmpack_show},
	{"source", mmpack_source},
	{"uninstall", mmpack_remove},
	{"update", mmpack_update_all},
	{"upgrade", mmpack_upgrade},
};

static const char mmpack_doc[] =
	"mmpack is a cross-platform package manager."
	"\n\n"
	"It is designed to work without any need for root access, and to allow "
	"multiple coexisting project versions within project prefixes (akin to "
	"python's virtualenv sandboxes)"
	"\n\n"
	"mmpack is the entry point for many package management commands (update, "
	"install, remove...)."
;

static const char arguments_docs[] =
	"[options] "CHECK_INTEGRITY_SYNOPSIS "\n"
	"[options] "DOWNLOAD_SYNOPSIS "\n"
	"[options] "FIX_BROKEN_SYNOPSIS "\n"
	"[options] "INSTALL_SYNOPSIS "\n"
	"[options] "LIST_SYNOPSIS "\n"
	"[options] "MKPREFIX_SYNOPSIS "\n"
	"[options] "REMOVE_SYNOPSIS "\n"
	"[options] "REPO_SYNOPSIS "\n"
	"[options] "RUN_SYNOPSIS "\n"
	"[options] "SEARCH_SYNOPSIS "\n"
	"[options] "SHOW_SYNOPSIS "\n"
	"[options] "SOURCE_SYNOPSIS "\n"
	"[options] "UPDATE_SYNOPSIS "\n"
	"[options] "UPGRADE_SYNOPSIS "\n"
;

static struct mmpack_opts cmdline_opts;

static const struct mmarg_opt cmdline_optv[] = {
	{"p|prefix", MMOPT_NEEDDIR, NULL, {.sptr = &cmdline_opts.prefix},
	 "Use @PATH as install prefix."},
	{"version", MMOPT_NOVAL, "set", {.sptr = &cmdline_opts.version},
	 "Display mmpack version"},
};


static
void init_stdout(void)
{
#if defined (_WIN32)
	// Make stream unbuffered
	setbuf(stdout, NULL);
#endif
}


int main(int argc, char* argv[])
{
	int rv;
	const struct subcmd* subcmd;
	struct mmpack_ctx ctx = {0};
	struct subcmd_parser parser = {
		.doc = mmpack_doc,
		.args_doc = arguments_docs,
		.optv = cmdline_optv,
		.num_opt = MM_NELEM(cmdline_optv),
		.execname = "mmpack",
		.num_subcmd = MM_NELEM(mmpack_subcmds),
		.subcmds = mmpack_subcmds,
	};
	int cmd_argc = argc;
	const char** cmd_argv = (const char**)argv;

	/* Parse command line options and subcmd. cmd_argv and cmd_argc are
	 * updated so that cmd_argv[0] point to sub command */
	subcmd = subcmd_parse(&parser, &cmd_argc, &cmd_argv);
	if (!subcmd)
		return EXIT_FAILURE;

	/* handle non-command options */
	if (cmdline_opts.version) {
		if (!mmarg_is_completing())
			printf("%s\n", PACKAGE_STRING);

		return EXIT_SUCCESS;
	}

	init_stdout();

	/* Initialize context according to command line options */
	if (mmpack_ctx_init(&ctx, &cmdline_opts))
		return EXIT_FAILURE;

	/* Run identified sub command with the remaining arguments */
	rv = subcmd->cb(&ctx, cmd_argc, cmd_argv);

	mmpack_ctx_deinit(&ctx);
	return (rv == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
