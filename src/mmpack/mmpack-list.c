/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "buffer.h"
#include "cmdline.h"
#include "common.h"
#include "context.h"
#include "mmpack-list.h"
#include "package-utils.h"

#include <mmargparse.h>
#include <mmerrno.h>
#include <mmlib.h>
#include <mmsysio.h>
#include <stdlib.h>
#include <string.h>


struct listing_opts {
	const char * pattern;
	int only_available;
	int only_repoless;
};


static int show_ghost_packages = 0;


static
void print_pkg(const struct binpkg* pkg, const struct mmpack_ctx* ctx)
{
	const char* state;
	struct remote_resource* res;

	if (mmpack_ctx_is_pkg_installed(ctx, pkg))
		state = "[installed]";
	else
		state = "[available]";

	printf("%s %s (%s) ", state, pkg->name, pkg->version);

	if (pkg->remote_res) {
		printf("from repositories:");
		for (res = pkg->remote_res; res != NULL; res = res->next) {
			mm_check(res->repo != NULL);
			printf(" %s%c",
			       res->repo->name,
			       res->next ? ',' : '\n');
		}
	} else {
		printf("from repositories: unknown\n");
	}
}


static
int binpkg_cmp(const void * v1, const void * v2)
{
	const struct binpkg * pkg1, * pkg2;
	int res;

	pkg1 = *((const struct binpkg**) v1);
	pkg2 = *((const struct binpkg**) v2);

	res = strcmp(pkg1->name, pkg2->name);

	if (res == 0)
		res = pkg_version_compare(pkg1->version, pkg2->version);

	return res;
}


static
int print_pkgs_in_buffer(struct buffer* buff, struct mmpack_ctx* ctx)
{
	const struct binpkg** pkgs = buff->base;
	int num = buff->size / sizeof(*pkgs);
	int i;

	qsort(pkgs, num, sizeof(*pkgs), binpkg_cmp);
	for (i = 0; i < num; i++)
		print_pkg(pkgs[i], ctx);

	return num != 0;
}


/**
 * add_pkg_to_buff_if_match() - add package to buffer if options match
 * @pkg:        package to be tested
 * @opts:       options restricting the listing
 * @buff:       mmpack context
 *
 * Tests whether @pkg fulfill constraints passed in @opts. If there is a
 * match, the package is added to a buffer holding the packages to eventually
 * print.
 */
static
void add_pkg_to_buff_if_match(const struct binpkg* pkg,
                              const struct listing_opts* opts,
                              struct buffer* buff)
{
	// Exclude package not in repo if only available requested
	if (opts->only_available && !binpkg_is_available(pkg))
		return;

	// Exclude package in repo if only pkg without repo
	if (opts->only_repoless && binpkg_is_available(pkg))
		return;

	// If pattern is provided and the name does not match do nothing
	if (opts->pattern && (strstr(pkg->name, opts->pattern) == NULL))
		return;

	// Skip showing ghost packages if not requested
	if (!show_ghost_packages && binpkg_is_ghost(pkg))
		return;

	buffer_push(buff, &pkg, sizeof(pkg));
}


static
int list_binindex_pkgs(struct mmpack_ctx* ctx,
                       const struct listing_opts* opts)
{
	const struct binpkg* pkg;
	struct pkg_iter iter;
	struct buffer buff;
	int found;

	buffer_init(&buff);
	pkg = pkg_iter_first(&iter, &ctx->binindex);
	for (; pkg != NULL; pkg = pkg_iter_next(&iter))
		add_pkg_to_buff_if_match(pkg, opts, &buff);

	found = print_pkgs_in_buffer(&buff, ctx);
	buffer_deinit(&buff);

	return found;
}


static
int list_installed_pkgs(struct mmpack_ctx* ctx,
                        const struct listing_opts* opts)
{
	const struct binpkg* pkg;
	struct inststate_iter iter;
	struct buffer buff;
	int found;

	buffer_init(&buff);
	pkg = inststate_first(&iter, &ctx->installed);
	for (; pkg != NULL; pkg = inststate_next(&iter))
		add_pkg_to_buff_if_match(pkg, opts, &buff);

	found = print_pkgs_in_buffer(&buff, ctx);
	buffer_deinit(&buff);

	return found;
}


static
int list_all(struct mmpack_ctx* ctx, int argc, const char* argv[])
{
	struct listing_opts opts = {.pattern = (argc >= 2) ? argv[1] : NULL};

	return list_binindex_pkgs(ctx, &opts);
}


static
int list_available(struct mmpack_ctx* ctx, int argc, const char* argv[])
{
	struct listing_opts opts = {
		.pattern = (argc >= 2) ? argv[1] : NULL,
		.only_available = 1,
	};

	return list_binindex_pkgs(ctx, &opts);
}


static
int list_installed(struct mmpack_ctx* ctx, int argc, const char* argv[])
{
	struct listing_opts opts = {.pattern = (argc >= 2) ? argv[1] : NULL};

	return list_installed_pkgs(ctx, &opts);
}


static
int list_extras(struct mmpack_ctx* ctx, int argc, const char* argv[])
{
	struct listing_opts opts = {
		.pattern = (argc >= 2) ? argv[1] : NULL,
		.only_repoless = 1,
	};

	return list_installed_pkgs(ctx, &opts);
}


static
int list_upgradeable(struct mmpack_ctx* ctx, int argc, const char* argv[])
{
	struct listing_opts opts = {.pattern = (argc >= 2) ? argv[1] : NULL};
	const struct binpkg * pkg, * latest;
	struct inststate_iter iter;
	struct buffer buff;
	int found;

	buffer_init(&buff);
	pkg = inststate_first(&iter, &ctx->installed);
	for (; pkg != NULL; pkg = inststate_next(&iter)) {
		if (binindex_is_pkg_upgradeable(&ctx->binindex, pkg)) {
			latest = binindex_lookup(&ctx->binindex,
			                         pkg->name, NULL);
			add_pkg_to_buff_if_match(latest, &opts, &buff);
		}
	}

	found = print_pkgs_in_buffer(&buff, ctx);
	buffer_deinit(&buff);

	return found;
}


static
const struct subcmd list_subcmds[] = {
	{"all", list_all},
	{"available", list_available},
	{"extras", list_extras},
	{"installed", list_installed},
	{"upgradeable", list_upgradeable},
};

static const struct mm_arg_opt cmdline_optv[] = {
	{"g|show-ghosts", MM_OPT_NOVAL|MM_OPT_INT, "1",
	 {.iptr = &show_ghost_packages},
	 "Show ghost packages"},
};




/**
 * mmpack_list() - main function for the list command
 * @ctx: mmpack context
 * @argc: number of arguments
 * @argv: array of arguments
 *
 * lists information about the installed or available packages.
 *
 * Return: 0 on success, -1 otherwise
 */
LOCAL_SYMBOL
int mmpack_list(struct mmpack_ctx * ctx, int argc, const char* argv[])
{
	struct subcmd_parser parser = {
		.execname = "mmpack",
		.args_doc = LIST_SYNOPSIS,
		.optv = cmdline_optv,
		.num_opt = MM_NELEM(cmdline_optv),
		.num_subcmd = MM_NELEM(list_subcmds),
		.subcmds = list_subcmds,
		.defcmd = "installed",
	};
	const struct subcmd* subcmd;
	int found;

	subcmd = subcmd_parse(&parser, &argc, &argv);
	if (!subcmd)
		return -1;

	/* If completing, nothing should be further displayed */
	if (mm_arg_is_completing())
		return 0;

	if (argc > 2) {
		fprintf(stderr, "Too many argument."
		        " Run \"mmpack list --help\" to see Usage\n");
		return -1;
	}

	/* Load prefix configuration and caches */
	if (mmpack_ctx_use_prefix(ctx, 0))
		return -1;

	found = subcmd->cb(ctx, argc, argv);
	if (!found) {
		if (argc > 1)
			printf("No package found matching pattern: "
			       "\"%s\"\n", argv[1]);
		else
			printf("No package found\n");
	}

	return 0;
}
