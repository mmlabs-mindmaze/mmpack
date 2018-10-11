/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmargparse.h>
#include <mmerrno.h>
#include <mmsysio.h>

#include "context.h"
#include "mmstring.h"
#include "utils.h"

#include "mmpack-mkprefix.h"

static int force_mkprefix = 0;
static const char* repo_url = NULL;

static char mkprefix_doc[] =
	"mmpack mkprefix allows you to create a new prefix in folder "
	"specified by <prefix-path> which will pull packages from the "
	"repository whose URL is optionally set by --url. If not present, "
	"the URL is inherited by the global user configuration of mmpack. "
	"By default the command will prevent to create a prefix in a "
	"folder that has been already setup.";

static const struct mmarg_opt cmdline_optv[] = {
	{"f|force", MMOPT_NOVAL|MMOPT_INT, "1", {.iptr = &force_mkprefix},
	 "Force setting up prefix folder even if it was already setup"},
	{"url", MMOPT_NEEDSTR, NULL, {.sptr = &repo_url},
	 "Specify @URL as the address of package repository"},
};


static
int create_initial_empty_files(const mmstr* prefix, int force_create)
{
	int fd, oflag;
	const mmstr *instlist_relpath, *repocache_relpath;

	instlist_relpath = mmstr_alloca_from_cstr(INSTALLED_INDEX_RELPATH);
	repocache_relpath = mmstr_alloca_from_cstr(REPO_INDEX_RELPATH);

	oflag = O_WRONLY|O_CREAT| (force_create ? O_TRUNC : O_EXCL);

	// Create initial empty installed package list
	fd = open_file_in_prefix(prefix, instlist_relpath, oflag);
	mm_close(fd);
	if (fd < 0)
		return -1;

	// Create initial empty cache repo package list
	fd = open_file_in_prefix(prefix, repocache_relpath, oflag);
	mm_close(fd);
	if (fd < 0)
		return -1;

	return 0;
}


static
int create_initial_prefix_cfg(const mmstr* prefix, const char* url,
                              int force_create)
{
	const mmstr* cfg_relpath = mmstr_alloca_from_cstr(CFG_RELPATH);
	char line[256];
	int fd, len, oflag;

	oflag = O_WRONLY|O_CREAT|(force_create ? O_TRUNC : O_EXCL);
	fd = open_file_in_prefix(prefix, cfg_relpath, oflag);
	if (fd < 0)
		return -1;

	// Optionally write URL (if null, it will inherit from user config)
	if (url) {
		len = sprintf(line, "repository: %s", url);
		mm_write(fd, line, len);
	}

	mm_close(fd);
	return 0;
}


LOCAL_SYMBOL
int mmpack_mkprefix(struct mmpack_ctx * ctx, int argc, const char* argv[])
{
	int arg_index;
	const mmstr* prefix;
	struct mmarg_parser parser = {
		.doc = mkprefix_doc,
		.args_doc = MKPREFIX_SYNOPSIS,
		.optv = cmdline_optv,
		.num_opt = MM_NELEM(cmdline_optv),
		.execname = "mmpack",
	};
	(void) ctx;  /* silence unused warning */

	arg_index = mmarg_parse(&parser, argc, (char**)argv);

	if (arg_index+1 != argc) {
		fprintf(stderr, "Bad usage of mkprefix command.\n"
		                "Run \"mmpack mkprefix --help\" to see usage\n");
		return -1;
	}

	prefix = mmstr_alloca_from_cstr(argv[arg_index]);

	if (  create_initial_empty_files(prefix, force_mkprefix)
	   || create_initial_prefix_cfg(prefix, repo_url, force_mkprefix) )
		return -1;

	return 0;
}
