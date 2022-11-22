/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "mmpack-check-integrity.h"

#include <mmargparse.h>
#include <mmerrno.h>
#include <mmlib.h>
#include <mmpredefs.h>
#include <mmsysio.h>
#include <stdint.h>
#include <string.h>

#include "cmdline.h"
#include "context.h"
#include "mmstring.h"
#include "package-utils.h"
#include "pkg-fs-utils.h"
#include "utils.h"


static
int check_pkg_integrity(const struct binpkg* pkg, struct mmpack_ctx* ctx)
{
	int rv;

	info("Checking %s (%s) ... ", pkg->name, pkg->version);
	rv = check_installed_pkg(pkg);
	info("%s\n", (rv == 0) ? "OK" : "Failed");

	return rv;
}


/**
 * mmpack_check_integrity() - main function for the check integrity command
 * @ctx: mmpack context
 * @argc: number of arguments
 * @argv: array of arguments
 *
 * check package integrity, or all packages if none given
 *
 * Return: 0 on success, -1 otherwise
 */
LOCAL_SYMBOL
int mmpack_check_integrity(struct mmpack_ctx * ctx, int argc,
                           char const* argv[])
{
	struct inststate_iter iter;
	const struct binpkg* pkg;
	mmstr* name;
	int rv;

	if (mm_arg_is_completing()) {
		// Complete only first command argument and if not empty
		if (argc != 2 || argv[1][0] == '\0')
			return 0;

		return complete_pkgname(ctx, argv[1], ONLY_INSTALLED);
	}

	if (argc > 2
	    || (argc == 2
	        && (STR_EQUAL(argv[1], strlen(argv[1]), "--help")
	            || STR_EQUAL(argv[1], strlen(argv[1]), "-h")))) {
		fprintf(stderr,
		        "Usage:\n\tmmpack "CHECK_INTEGRITY_SYNOPSIS "\n");
		return argc > 2;
	}

	// Load prefix configuration and caches and move to prefix directory
	if (mmpack_ctx_use_prefix(ctx, 0)
	    || mm_chdir(ctx->prefix))
		return -1;

	rv = 0;
	if (argc == 2) {
		name = mmstr_malloca_from_cstr(argv[1]);
		pkg = install_state_get_pkg(&ctx->installed, name);
		if (pkg) {
			rv = check_pkg_integrity(pkg, ctx);
		} else {
			rv = -1;
			printf("Package \"%s\" not found\n", name);
		}

		mmstr_freea(name);
	} else {
		// Loop over installed packages
		pkg = inststate_first(&iter, &ctx->installed);
		for (; pkg != NULL; pkg = inststate_next(&iter))
			rv = (check_pkg_integrity(pkg, ctx) < 0) ? -1 : rv;
	}

	// Restore previous current directory
	mm_chdir(ctx->cwd);

	return rv;
}
