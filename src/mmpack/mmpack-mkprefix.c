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
#include "repo.h"

#include "mmpack-mkprefix.h"

#define PREFIX_SITE_PY "lib/python3/site-packages/site.py"


static int force_mkprefix = 0;
static int clean_repolist = 0;
static int do_upgrade = 0;
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

static const struct mm_arg_opt cmdline_optv[] = {
	{"f|force", MM_OPT_NOVAL|MM_OPT_INT, "1", {.iptr = &force_mkprefix},
	 "Force setting up prefix folder even if it was already setup"},
	{"c|clean-repolist", MM_OPT_NOVAL|MM_OPT_INT, "1", {.iptr = &clean_repolist},
	 "Do not import repository list from global config"},
	{"u|upgrade", MM_OPT_NOVAL|MM_OPT_INT, "1", {.iptr = &do_upgrade},
	 "Upgrade prefix files instead setting up a new prefix"},
	{"url", MM_OPT_NEEDSTR, NULL, {.sptr = &repo_url},
	 "Specify @URL as the address of package repository"},
	{"name", MM_OPT_NEEDSTR, NULL, {.sptr = &repo_name},
	 "Specify @NAME as a nickname for the url specified"},
};


static const char site_py_content[] = {
#include "site.py.inc"
};


static
int copy_site_py(const mmstr* prefix, int force_create)
{
	STATIC_CONST_MMSTR(site_py_relpath, PREFIX_SITE_PY);
	ssize_t rsz;
	int fd, oflag;
	mmstr* site_py_path = NULL;

	oflag = O_WRONLY|O_CREAT| (force_create ? O_TRUNC : O_EXCL);

	// Create site.py in prefix python site package folder
	fd = open_file_in_prefix(prefix, site_py_relpath, oflag);
	if (fd < 0)
		return -1;

	rsz = mm_write(fd, site_py_content, sizeof(site_py_content));
	mm_close(fd);
	if (rsz < 0)
		return -1;

	// Generate site.py bytecode
	site_py_path = mmstr_join_path_realloc(NULL, prefix, site_py_relpath);
	char* argv[] = {
		"python3",
		"-m",
		"compileall",
		"-q",
		site_py_path,
		NULL
	};
	execute_cmd(argv);
	mmstr_free(site_py_path);

	return 0;
}


static
int create_initial_empty_files(const mmstr* prefix, int force_create)
{
	int fd, oflag;
	const mmstr * instlist_relpath, * log_relpath, * manually_inst_relpath;

	instlist_relpath = mmstr_alloca_from_cstr(INSTALLED_INDEX_RELPATH);
	log_relpath = mmstr_alloca_from_cstr(LOG_RELPATH);
	manually_inst_relpath = mmstr_alloca_from_cstr(MANUALLY_INST_RELPATH);

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

	// Create initial empty manually installed packages set
	fd = open_file_in_prefix(prefix, manually_inst_relpath, oflag);
	mm_close(fd);
	if (fd < 0)
		return -1;

	return copy_site_py(prefix, force_create);
}


static
int upgrade_prefix(const mmstr* prefix)
{
	if (copy_site_py(prefix, 1))
		return -1;

	printf("Upgraded mmpack prefix: %s\n", prefix);
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
	int arg_index;
	const mmstr* prefix;
	struct mm_arg_parser parser = {
		.flags = mm_arg_is_completing() ? MM_ARG_PARSER_COMPLETION : 0,
		.doc = mkprefix_doc,
		.args_doc = MKPREFIX_SYNOPSIS,
		.optv = cmdline_optv,
		.num_opt = MM_NELEM(cmdline_optv),
		.execname = "mmpack",
	};
	struct repolist* repo_list = &ctx->settings.repo_list;
	struct repo* repo;

	arg_index = mm_arg_parse(&parser, argc, (char**)argv);
	if (mm_arg_is_completing()) {
		if (arg_index+1 < argc)
			return 0;

		return mm_arg_complete_path(argv[arg_index],
		                            MM_DT_DIR, NULL, NULL);
	}

	prefix = ctx->prefix;
	if (arg_index+1 == argc)
		prefix = mmstr_alloca_from_cstr(argv[arg_index]);

	if (argc > arg_index+1) {
		fprintf(stderr, "Bad usage of mkprefix command.\n");
		fprintf(stderr,
		        "Run \"mmpack mkprefix --help\" to see usage\n");
		return -1;
	}

	if (!prefix) {
		fprintf(stderr, "Un-specified prefix path to create.\n");
		return -1;
	}

	if (clean_repolist)
		repolist_reset(repo_list);

	// If url is set, replace the repo list with one whose the url and name
	// are supplied in arguments. If unset, the repo list will be kept
	// untouched, hence will be the one read from user global configuration
	if (repo_url) {
		repolist_reset(repo_list);
		if (!(repo = repolist_add(repo_list, repo_name)))
			return -1;

		repo->url = mmstr_malloc_from_cstr(repo_url);
		repo->enabled = 1;
	}

	if (do_upgrade)
		return upgrade_prefix(prefix);

	if (create_initial_empty_files(prefix, force_mkprefix)
	    || create_initial_index_files(prefix, repo_list)
	    || settings_serialize(prefix, &ctx->settings, force_mkprefix)) {
		fprintf(stderr, "Failed to create mmpack prefix: %s\n", prefix);
		return -1;
	}

	printf("Created mmpack prefix: %s\n", prefix);

	return 0;
}
