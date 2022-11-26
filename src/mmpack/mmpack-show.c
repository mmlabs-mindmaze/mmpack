/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "mmpack-show.h"

#include <mmargparse.h>
#include <mmerrno.h>
#include <mmlib.h>
#include <mmsysio.h>
#include <string.h>
#include "cmdline.h"
#include "context.h"
#include "crypto.h"
#include "repo.h"
#include "settings.h"
#include "package-utils.h"


static
void print_wrapped_str(const char* str)
{
	struct strchunk in = {.buf = str, .len = strlen(str)};
	mmstr* wrapped = mmstr_malloc(128);

	wrapped = textwrap_string(wrapped, in, 76, "    ", "\n");
	printf("    %s\n", wrapped);
	mmstr_free(wrapped);
}


static
void show_pkg(const struct binpkg* pkg, const struct mmpack_ctx* ctx)
{
	struct remote_resource* from;
	const struct pkgdep* dep;
	const struct strlist_elt* sysdep;
	char hexstr[SHA_HEXLEN+1] = ""; // ensure null termination is set

	printf("%s (%s) %s\n", pkg->name, pkg->version,
	       mmpack_ctx_is_pkg_installed(ctx, pkg) ? "[installed]" : "");

	hexstr_from_digest(hexstr, &pkg->sumsha);
	printf("SUMSHA256: %s\n", hexstr);

	for (from = pkg->remote_res; from != NULL; from = from->next) {
		printf("Repository: %s\n", from->repo ?
		       from->repo->name : "unknown");
		printf("\tPackage file: %s\n", from->filename);
		hexstr_from_digest(hexstr, &from->sha256);
		printf("\tSHA256: %s\n", hexstr);
	}

	printf("Source package: %s\n", pkg->source);
	printf("Ghost: %s\n", binpkg_is_ghost(pkg) ? "yes" : "no");

	printf("Dependencies:\n");

	for (dep = pkg->mpkdeps; dep != NULL; dep = dep->next) {
		printf("\t\t [MMPACK] %s [%s -> %s]\n",
		       dep->name, dep->min_version, dep->max_version);
	}

	for (sysdep = pkg->sysdeps.head; sysdep != NULL; sysdep = sysdep->next)
		printf("\t\t [SYSTEM] %s\n", sysdep->str.buf);

	printf("\nDescription:\n");
	print_wrapped_str(pkg->desc ? pkg->desc : "none");
}


/**
 * mmpack_show() - main function for the show command
 * @ctx: mmpack context
 * @argc: number of arguments
 * @argv: array of arguments
 *
 * show given package metadatas.
 *
 * Return: 0 on success, -1 otherwise
 */
LOCAL_SYMBOL
int mmpack_show(struct mmpack_ctx * ctx, int argc, const char* argv[])
{
	struct pkglist_iter iter;
	const struct binpkg* pkg;
	mmstr* name;

	if (mm_arg_is_completing()) {
		if (argc != 2)
			return 0;

		return complete_pkgname(ctx, argv[1], AVAILABLE_PKGS);
	}

	if (argc != 2
	    || STR_EQUAL(argv[1], strlen(argv[1]), "--help")
	    || STR_EQUAL(argv[1], strlen(argv[1]), "-h")) {
		fprintf(stderr, "missing package argument in command line\n"
		        "Usage:\n\tmmpack "SHOW_SYNOPSIS "\n");
		return argc != 2 ? -1 : 0;
	}

	name = mmstr_malloca_from_cstr(argv[1]);

	// Load prefix configuration and caches
	if (mmpack_ctx_use_prefix(ctx, 0))
		return -1;

	// Start iteration of package having the specified name
	pkg = pkglist_iter_first(&iter, name, &ctx->binindex);
	if (!pkg)
		printf("No package found matching: \"%s\"\n", name);

	for (; pkg != NULL; pkg = pkglist_iter_next(&iter))
		show_pkg(pkg, ctx);

	mmstr_freea(name);
	return 0;
}
