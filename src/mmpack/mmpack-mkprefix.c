/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmargparse.h>
#include <mmerrno.h>
#include <mmlib.h>
#include <mmsysio.h>

#include "context.h"
#include "mmstring.h"
#include "utils.h"
#include "package-utils.h"

#include "mmpack-mkprefix.h"

static int force_mkprefix = 0;
static const char* repo_url = NULL;
static const char* repo_name = NULL;

static char mkprefix_doc[] =
	"mmpack mkprefix allows you to create a new prefix in folder "
	"specified by <prefix-path> which will pull packages from the "
	"repository whose URL is optionally set by --url. If not present, "
	"the URL is inherited by the global user configuration of mmpack. "
	"By default the command will prevent to create a prefix in a "
	"folder that has been already setup. You can add a short NAME for the "
	"URL. If you do not enter a NAME, your url will have \"default\" as "
	"short name. If NAME is set but not URL an error is returned.";

static const struct mmarg_opt cmdline_optv[] = {
	{"f|force", MMOPT_NOVAL|MMOPT_INT, "1", {.iptr = &force_mkprefix},
	 "Force setting up prefix folder even if it was already setup"},
	{"url", MMOPT_NEEDSTR, NULL, {.sptr = &repo_url},
	 "Specify @URL as the address of package repository"},
	{"name", MMOPT_NEEDSTR, NULL, {.sptr = &repo_name},
	 "Specify @NAME as a nickname for the url specified"},
};


static
int create_initial_empty_files(const mmstr* prefix, int force_create)
{
	int fd, oflag;
	const mmstr * instlist_relpath, * log_relpath;

	instlist_relpath = mmstr_alloca_from_cstr(INSTALLED_INDEX_RELPATH);
	log_relpath = mmstr_alloca_from_cstr(LOG_RELPATH);

	oflag = O_WRONLY|O_CREAT| (force_create ? O_TRUNC : O_EXCL);

	// Create initial empty log file
	fd = open_file_in_prefix(prefix, log_relpath, oflag);
	mm_close(fd);
	if (fd < 0)
		return -1;

	// Create initial empty installed package list
	fd = open_file_in_prefix(prefix, instlist_relpath, oflag);
	mm_close(fd);
	if (fd < 0)
		return -1;

	return 0;
}


/**
 * mmpack_mkprefix() - main function for the command to create prefixes
 * @ctx: mmpack context
 * @argc: number of arguments
 * @argv: array of arguments
 *
 * Allows you to create a new prefix in given folder. Packages will be pulled
 * from the repository whose URL is given, or the global prefix URL if none
 * given.
 *
 * Return: 0 on success, -1 otherwise
 */
LOCAL_SYMBOL
int mmpack_mkprefix(struct mmpack_ctx * ctx, int argc, const char* argv[])
{
	int arg_index, res = 0;
	const mmstr* prefix;
	struct mmarg_parser parser = {
		.flags = mmarg_is_completing() ? MMARG_PARSER_COMPLETION : 0,
		.doc = mkprefix_doc,
		.args_doc = MKPREFIX_SYNOPSIS,
		.optv = cmdline_optv,
		.num_opt = MM_NELEM(cmdline_optv),
		.execname = "mmpack",
	};
	struct repolist* repo_list = &ctx->settings.repo_list;
	struct repolist_elt * repo;

	arg_index = mmarg_parse(&parser, argc, (char**)argv);
	if (mmarg_is_completing()) {
		if (arg_index+1 < argc)
			return 0;

		return mmarg_complete_path(argv[arg_index],
		                           MM_DT_DIR, NULL, NULL);
	}

	prefix = ctx->prefix;
	if (arg_index+1 == argc)
		prefix = mmstr_alloca_from_cstr(argv[arg_index]);

	if (argc > arg_index+1) {
		fprintf(stderr, "Bad usage of mkprefix command.\n");
		res = 1;
	}

	if (!prefix) {
		fprintf(stderr, "Un-specified prefix path to create.\n");
		res = 1;
	}

	// If url is set, replace the repo list with one whose the url and name
	// are supplied in arguments. If unset, the repo list will be kept
	// untouched, hence will be the one read from user global configuration
	if (!res && repo_url) {
		repolist_reset(repo_list);
		if (!(repo = repolist_add(repo_list, repo_name)))
			res = -1;

		repo->url = mmstr_malloc_from_cstr(repo_url);
		repo->enabled = 1;
	}

	if (!res && (create_initial_empty_files(prefix, force_mkprefix)
	             || create_initial_binindex_files(prefix, repo_list)
	             || settings_serialize(prefix, &ctx->settings,
	                                   force_mkprefix))) {
		fprintf(stderr, "Failed to create mmpack prefix: %s\n", prefix);
		res = -1;
	}

	if (!res)
		printf("Created mmpack prefix: %s\n", prefix);
	else
		fprintf(stderr,
		        "Run \"mmpack mkprefix --help\" to see usage\n");

	return res;
}
