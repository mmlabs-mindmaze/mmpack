/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "mmpack-source.h"

#include <archive.h>
#include <archive_entry.h>
#include <mmargparse.h>
#include <mmerrno.h>
#include <mmsysio.h>
#include <string.h>

#include "cmdline.h"
#include "context.h"
#include "download.h"
#include "mmstring.h"
#include "package-utils.h"
#include "repo.h"
#include "srcindex.h"
#include "utils.h"


static
int extract_tarball(const mmstr* target_dir, const char* filename)
{
	const char* path;
	struct archive_entry * entry;
	struct archive * a;
	int r, type, rv;

	// Initialize an archive stream
	a = archive_read_new();
	archive_read_support_filter_all(a);
	archive_read_support_format_all(a);

	// Open binary package in the archive stream
	if (archive_read_open_filename(a, filename, READ_ARCHIVE_BLOCK)) {
		mm_raise_error(archive_errno(a), "opening mpk %s failed: %s",
		               filename, archive_error_string(a));
		archive_read_free(a);
		return -1;
	}

	// Loop over each entry in the archive and process them
	rv = 0;
	while (rv == 0) {
		r = archive_read_next_header(a, &entry);
		if (r == ARCHIVE_EOF) {
			rv = READ_ARCHIVE_EOF;
			break;
		}

		if (r != ARCHIVE_OK) {
			rv = mm_raise_error(archive_errno(a),
			                    "reading %s failed: %s",
			                    filename, archive_error_string(a));
			break;
		}

		path = archive_entry_pathname_utf8(entry);
		type = archive_entry_filetype(entry);
		switch (type) {
		case AE_IFDIR: rv = mm_mkdir(path, 0777, MM_RECURSIVE); break;
		case AE_IFREG: rv = pkg_unpack_regfile(entry, path, a); break;
		case AE_IFLNK: rv = pkg_unpack_symlink(entry, path); break;
		default:
			rv = mm_raise_error(MM_EBADFMT,
			                    "unexpected file type of %s", path);
		}
	}

	// Cleanup
	archive_read_close(a);
	archive_read_free(a);

	return rv;
}


static
mmstr* get_source_dir(struct mmpack_ctx * ctx, const struct srcpkg* srcpkg)
{
	mmstr* srcdir;
	const char srcdir_template[] = "%s/src/%s-%s";

	srcdir = mmstr_malloc(sizeof(srcdir_template)
	                      + mmstrlen(ctx->prefix)
	                      + mmstrlen(srcpkg->name)
			      + mmstrlen(srcpkg->srcversion));
	sprintf(srcdir, srcdir_template,
	        ctx->prefix, srcpkg->name, srcpkg->srcversion);

	return srcdir;
}


static
int install_pkg_sources(struct mmpack_ctx * ctx, struct binpkg const * pkg)
{
	const struct srcpkg* srcpkg;
	const mmstr* cachedir;
	mmstr *basepath, *srctar, *srcdir;
	int rv = 0;

	srcpkg = srcindex_lookup(&ctx->srcindex,
	                         pkg->source, pkg->version, pkg->srcsha);
	if (!srcpkg) {
		printf("Cannot find source of package %s %s (%s)\n",
		       pkg->source, pkg->version, pkg->srcsha);
		return -1;
	}

	cachedir = mmpack_ctx_get_pkgcachedir(ctx);
	basepath = mmstr_basename(NULL, srcpkg->remote_res->filename);
	srctar = mmstr_join_path_realloc(NULL, cachedir, basepath);

	rv = download_remote_resource(ctx, srcpkg->remote_res, srctar);

	mm_mkdir()


	mmstr_free(srctar);
	mmstr_free(basepath);
	mmstr_free(srcdir);

	return rv;
}


/**
 * mmpack_source() - main function for the source command
 * @ctx: mmpack context
 * @argc: number of arguments
 * @argv: array of arguments
 *
 * download given package sources into current directory.
 *
 * Return: 0 on success, -1 otherwise
 */
LOCAL_SYMBOL
int mmpack_source(struct mmpack_ctx * ctx, int argc, const char* argv[])
{
	struct binpkg const * pkg;

	if (mm_arg_is_completing()) {
		if (argc != 2)
			return 0;

		return complete_pkgname(ctx, argv[1], AVAILABLE_PKGS);
	}

	if (argc != 2
	    || STR_EQUAL(argv[1], strlen(argv[1]), "--help")
	    || STR_EQUAL(argv[1], strlen(argv[1]), "-h")) {
		fprintf(stderr, "missing package argument in command line\n"
		        "Usage:\n\tmmpack "SOURCE_SYNOPSIS "\n");
		return argc != 2 ? -1 : 0;
	}

	/* Load prefix configuration and caches */
	if (mmpack_ctx_use_prefix(ctx, 0))
		return -1;

	if ((pkg = parse_pkg(ctx, argv[1])) == NULL)
		return -1;

	return install_pkg_sources(ctx, pkg);
}
