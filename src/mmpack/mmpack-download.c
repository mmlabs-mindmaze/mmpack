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
	struct mm_arg_parser parser = {
		.flags = mm_arg_is_completing() ? MM_ARG_PARSER_COMPLETION : 0,
		.doc = download_doc,
		.args_doc = DOWNLOAD_SYNOPSIS,
		.execname = "mmpack",
	};

	arg_index = mm_arg_parse(&parser, argc, (char**)argv);
	if (mm_arg_is_completing())
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

	if ((pkg = parse_pkg(ctx, argv[arg_index])) == NULL)
		return -1;

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
