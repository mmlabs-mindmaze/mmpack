/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "mmpack-download.h"

#include <mmargparse.h>
#include <mmerrno.h>
#include <mmlib.h>
#include <mmsysio.h>
#include <string.h>

#include "cmdline.h"
#include "context.h"
#include "package-utils.h"
#include "pkg-fs-utils.h"
#include "utils.h"


static char download_doc[] =
	"\"mmpack download\" downloads and downloads given packages file "
	"the current prefix cache folder.";


/**
 * mmpack_download() - main function for the download command
 * @ctx: mmpack context
 * @argc: number of arguments
 * @argv: array of arguments
 *
 * downloads given packages into the current prefix.
 *
 * Return: 0 on success, -1 otherwise
 */
LOCAL_SYMBOL
int mmpack_download(struct mmpack_ctx * ctx, int argc, const char* argv[])
{
	int arg_index, rv = -1;
	mmstr * basename;
	struct mmpkg const * pkg;
	struct cmdline_constraints const * cc;
	struct mmarg_parser parser = {
		.flags = mmarg_is_completing() ? MMARG_PARSER_COMPLETION : 0,
		.doc = download_doc,
		.args_doc = DOWNLOAD_SYNOPSIS,
		.execname = "mmpack",
	};

	arg_index = mmarg_parse(&parser, argc, (char**)argv);
	if (mmarg_is_completing())
		return complete_pkgname(ctx, argv[argc-1], AVAILABLE_PKGS);

	if ((arg_index + 1) != argc) {
		fprintf(stderr,
		        "missing package list argument in command line\n"
		        "Run \"mmpack download --help\" to see usage\n");
		return -1;
	}

	/* Load prefix configuration and caches */
	if (mmpack_ctx_use_prefix(ctx, 0))
		return -1;

	if (!(cc = parse_cmdline(argv[arg_index])))
		return -1;

	pkg = binindex_lookup(&ctx->binindex, cc);

	if (pkg->from_repo != NULL) {
		basename = mmstr_malloc(mmstrlen(pkg->from_repo->filename));
		mmstr_basename(basename, pkg->from_repo->filename);
		rv = download_package(ctx, pkg, basename);
		mmstr_free(basename);
	} else
		error("package %s is not present in known repositories\n",
		      pkg->name);

	return rv;
}
