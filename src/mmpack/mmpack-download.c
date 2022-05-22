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
#include "download.h"
#include "utils.h"


static char download_doc[] =
	"\"mmpack download\" downloads and downloads given packages file "
	"the current prefix cache folder.";


static
int try_delete(const char* filename)
{
	int previous;

	previous = mm_error_set_flags(MM_ERROR_SET, MM_ERROR_IGNORE);
	mm_unlink(filename);
	mm_error_set_flags(previous, MM_ERROR_IGNORE);

	return 0;
}


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
	int arg_index, rv = 0;
	mmstr * basename = NULL;
	mmstr* cachefile = NULL;
	struct binpkg const * pkg;
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

	if (binpkg_is_available(pkg)) {
		basename = mmstr_basename(NULL, pkg->remote_res->filename);
		if (download_remote_resource(ctx, pkg->remote_res, &cachefile)
		    || try_delete(basename)
		    || mm_copy(cachefile, basename, 0, 0666))
			rv = -1;
	} else
		error("package %s is not present in known repositories\n",
		      pkg->name);

	mmstr_free(basename);
	mmstr_free(cachefile);
	return rv;
}
