/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "cmdline.h"
#include "common.h"
#include "context.h"
#include "mmpack-list.h"
#include "package-utils.h"

#include <mmargparse.h>
#include <mmerrno.h>
#include <mmlib.h>
#include <mmsysio.h>
#include <string.h>


struct cb_data {
	const char * pkg_name_pattern;
	int only_available;
	int found;
};


/**
 * print_pkg_if_match() - print package if name match pattern
 * @pkg:        package whose name must be tested
 * @pattern:    pattern to search in name (can be NULL)
 * @settings:   setting of the context
 *
 * Tests whether @pkg has a name which contains the string @pattern if it
 * is not NULL. If @pattern is NULL, the match is considered to be true
 * (This allows to use NULL to disable filter on name).
 *
 * If there is a match, the package is printed on standard output along
 * with its status and version.
 *
 * Return: 1 in case of match, 0 otherwise.
 */
static
int print_pkg_if_match(const struct mmpkg* pkg, const char* pattern)
{
	const char* state;
	struct from_repo * from;

	// If pattern is provided and the name does not match do nothing
	if (pattern && (strstr(pkg->name, pattern) == NULL))
		return 0;

	if (pkg->state == MMPACK_PKG_INSTALLED)
		state = "[installed]";
	else
		state = "[available]";

	printf("%s %s (%s) ", state, pkg->name, pkg->version);

	if (pkg->from_repo) {
		printf("from repositories:");
		for (from = pkg->from_repo; from != NULL; from = from->next) {
			mm_check(from->repo != NULL);
			printf(" %s%c",
			       from->repo->name,
			       from->next ? ',' : '\n');
		}
	} else {
		printf("from repositories: unknown\n");
	}

	return 1;
}


static
int binindex_cb_all(struct mmpkg* pkg, void * void_data)
{
	struct cb_data * data = (struct cb_data*) void_data;

	// Exclude package not in repo if only available requested
	if (data->only_available && !pkg->from_repo)
		return 0;

	data->found |= print_pkg_if_match(pkg, data->pkg_name_pattern);
	return 0;
}


static
int list_all(struct mmpack_ctx* ctx, int argc, const char* argv[])
{
	struct cb_data data = {
		.only_available = 0,
		.pkg_name_pattern = (argc > 1) ? argv[1] : NULL,
		.found = 0,
	};

	binindex_foreach(&ctx->binindex, binindex_cb_all, &data);
	return data.found;
}


static
int list_available(struct mmpack_ctx* ctx, int argc, const char* argv[])
{
	struct cb_data data = {
		.only_available = 1,
		.pkg_name_pattern = (argc > 1) ? argv[1] : NULL,
		.found = 0,
	};

	binindex_foreach(&ctx->binindex, binindex_cb_all, &data);
	return data.found;
}


static
int list_installed(struct mmpack_ctx* ctx, int argc, const char* argv[])
{
	struct it_iterator iter;
	struct it_entry* entry;
	const char* pattern = (argc > 1) ? argv[1] : NULL;
	const struct mmpkg* pkg;
	int found = 0;

	// Loop over the entries of the installed package list
	entry = it_iter_first(&iter, &ctx->installed.idx);
	for (; entry != NULL; entry = it_iter_next(&iter)) {
		pkg = entry->value;
		found |= print_pkg_if_match(pkg, pattern);
	}

	return found;
}


static
int list_extras(struct mmpack_ctx* ctx, int argc, const char* argv[])
{
	struct it_iterator iter;
	struct it_entry* entry;
	const char* pattern = (argc > 1) ? argv[1] : NULL;
	const struct mmpkg* pkg;
	int found = 0;

	// Loop over the entries of the installed package list
	entry = it_iter_first(&iter, &ctx->installed.idx);
	for (; entry != NULL; entry = it_iter_next(&iter)) {
		pkg = entry->value;

		// Skip if a repo provides this package
		if (pkg->from_repo)
			continue;

		found |= print_pkg_if_match(pkg, pattern);
	}

	return found;
}


static
int list_upgradeable(struct mmpack_ctx* ctx, int argc, const char* argv[])
{
	struct it_iterator iter;
	struct it_entry* entry;
	struct pkg_parser pp;
	const char* pattern = (argc > 1) ? argv[1] : NULL;
	const struct mmpkg * pkg, * latest;
	int found = 0;

	// Loop over the entries of the installed package list
	entry = it_iter_first(&iter, &ctx->installed.idx);
	for (; entry != NULL; entry = it_iter_next(&iter)) {
		pkg = entry->value;

		if (!binindex_is_pkg_upgradeable(&ctx->binindex, pkg))
			continue;

		pkg_parser_init(&pp);
		pp.name = mmstr_malloc_from_cstr(pkg->name);

		latest = binindex_lookup(&ctx->binindex, pp.name, NULL);

		pkg_parser_deinit(&pp);

		found |= print_pkg_if_match(latest, pattern);
	}

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
	if (mmarg_is_completing())
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
