/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "common.h"
#include "context.h"
#include "mmpack-list.h"
#include "package-utils.h"

#include <mmerrno.h>
#include <mmlib.h>
#include <mmsysio.h>
#include <string.h>


enum {
	LIST_ALL,
	LIST_AVAILABLE,
	LIST_UPGRADABLE,
	LIST_INSTALLED,
	LIST_EXTRAS,
};

struct cb_data {
	struct mmpack_ctx * ctx;
	const char * pkg_name_pattern;
	struct mmpkg* pkg;
	int subcommand;
	int found;
	int silent;
};


static
int binindex_cb_all(struct mmpkg* pkg, void * void_data)
{
	char * state;
	struct cb_data * data = (struct cb_data *) void_data;

	if (strstr(pkg->name, data->pkg_name_pattern) != 0) {
		switch (data->subcommand) {
		case LIST_AVAILABLE:
			if (pkg->filename == NULL)  /* XXX could do better ... */
				return 0;
			state = "[available]";
			break;
		case LIST_ALL:
			if (pkg->state == MMPACK_PKG_INSTALLED)
				state = "[installed]";
			else
				state = "[available]";
			break;
		case LIST_INSTALLED:
			if (pkg->state != MMPACK_PKG_INSTALLED)
				return 0;
			state = "[installed]";
			break;
		default:
			/* Should not happen. Keep to silence warnings. */
			return 0;
		}

		data->found = 1;
		if (!data->silent)
			printf("%s %s (%s)\n", state, pkg->name, pkg->version);
	}

	return 0;
}


static
int binindex_cb_extras(struct mmpkg* pkg, void * void_data)
{
	struct cb_data * data = (struct cb_data *) void_data;

	if (strstr(pkg->name, data->pkg_name_pattern) != 0) {
		if (pkg->state != MMPACK_PKG_INSTALLED
		    || pkg->filename != NULL)
			return 0;

		data->found = 1;
		printf("%s %s (%s)\n", "[installed]", pkg->name, pkg->version);
	}

	return 0;
}


static
int binindex_cb_upgradable(struct mmpkg* pkg, void * void_data)
{
	struct cb_data * data = (struct cb_data *) void_data;
	struct mmpkg const * latest;

	if (strstr(pkg->name, data->pkg_name_pattern) != 0) {
		if (pkg->state != MMPACK_PKG_INSTALLED)
			return 0;

		latest = binindex_get_latest_pkg(&data->ctx->binindex, pkg->name,
		                                 mmstr_alloca_from_cstr("any"));
		/* always found: at least self is present */
		if (pkg_version_compare(pkg->version, latest->version) < 0) {
			printf("%s %s (%s -> %s)\n", "[available]",
			       latest->name, pkg->version, latest->version);
			data->found = 1;
		}
	}

	return 0;
}


static
int mmpack_list_parse_options(int argc, const char* argv[], struct cb_data * data)
{
	*data = (struct cb_data) {
		.subcommand = LIST_ALL,
		.pkg_name_pattern = "",
	};

	if (argc >= 2) {
		size_t len = strlen(argv[1]);

		if (STR_EQUAL(argv[1], len, "all"))
			data->subcommand = LIST_ALL;
		else if (STR_EQUAL(argv[1], len, "available"))
			data->subcommand = LIST_AVAILABLE;
		else if (STR_EQUAL(argv[1], len, "extras"))
			data->subcommand = LIST_EXTRAS;
		else if (STR_EQUAL(argv[1], len, "installed"))
			data->subcommand = LIST_INSTALLED;
		else if (STR_EQUAL(argv[1], len, "upgradable"))
			data->subcommand = LIST_UPGRADABLE;
		else
			return -1;

		if (argc == 3)
			data->pkg_name_pattern = argv[2];
		else if (argc > 3)
			return -1;
	}

	return 0;
}


LOCAL_SYMBOL
int mmpack_list(struct mmpack_ctx * ctx, int argc, const char* argv[])
{
	struct cb_data data;

	if (mmpack_list_parse_options(argc, argv, &data) < 0) {
		fprintf(stderr, "invalid list subcommand\n"
				"Usage:\n\tmmpack "LIST_SYNOPSIS"\n");
		return -1;
	}

	/* Load prefix configuration and caches */
	if (mmpack_ctx_use_prefix(ctx, 0))
		return -1;
	data.ctx = ctx;

	switch (data.subcommand) {
	case LIST_EXTRAS:
		binindex_foreach(&ctx->binindex, binindex_cb_extras, &data);
		break;
	case LIST_UPGRADABLE:
		binindex_foreach(&ctx->binindex, binindex_cb_upgradable, &data);
		break;
	case LIST_AVAILABLE:
	case LIST_INSTALLED:
	case LIST_ALL:
		binindex_foreach(&ctx->binindex, binindex_cb_all, &data);
		break;
	default:
		/* should not happen.
		 * Default case has been set to LIST_ALL when parsing options. */
		abort();
		break;
	}

	if (!data.found) {
		if (strlen(data.pkg_name_pattern))
			printf("No package found matching pattern: \"%s\"\n",
		           data.pkg_name_pattern);
		else
			printf("No package found\n");
	}

	return 0;
}
