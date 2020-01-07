/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmargparse.h>
#include <mmsysio.h>

#include "cmdline.h"
#include "context.h"
#include "indextable.h"
#include "package-utils.h"
#include "utils.h"


static
void subcmd_complete_arg(const struct subcmd_parser* parser, const char* arg)
{
	const char* cmd;
	int i, len;

	len = strlen(arg);

	/* Find command to dispatch */
	for (i = 0; i < parser->num_subcmd; i++) {
		cmd = parser->subcmds[i].name;
		if (strncmp(cmd, arg, len) == 0)
			printf("%s\n", cmd);
	}
}


/**
 * subcmd_parse() - parse cmdline options and a sub command
 * @parser:     sub command parser
 * @p_argc:     pointer to argument count (used as input and output)
 * @p_argv:     pointer to argument array (used as input and output)
 *
 * This function extents the functionality provided by mmarg_parse() from
 * mmlib. It parses the command line option and try to match the first
 * non-option argument to one of the subcmd listed in @parser->subcmds. If
 * no sub command if found on command line, the string pointed by
 * @parser->defcmd, if not NULL, will be interpreted as the provided sub
 * command.
 *
 * If mmarg_is_completing() reports that the shell completion is requested,
 * the function will complete the options and possibly the sub command if
 * last argument in array is being parsed. In such a case, the function will
 * exit the program. If the last argument as not been parsed by the
 * function, it will behave normally (and defer the completion to next
 * stages of command line parsing)
 *
 * The command line arguments array and count are respectively provided in
 * the pointed value by @p_argv and @p_argc. If the function succeeds, the
 * pointed value are updated such as *@p_argv starts with the subcommand
 * element (ie, (*@p_argv)[0] points to the subcommand).
 *
 * Return: pointer to the struct subcmd matching the sub command identified
 * in command line. NULL if an error has been encountered or no valid
 * subcommand has been found.
 */
LOCAL_SYMBOL
const struct subcmd* subcmd_parse(const struct subcmd_parser* parser,
                                  int* p_argc, const char*** p_argv)
{
	struct mmarg_parser argparser = {
		.flags = mmarg_is_completing() ? MMARG_PARSER_COMPLETION : 0,
		.num_opt = parser->num_opt,
		.optv = parser->optv,
		.doc = parser->doc,
		.args_doc = parser->args_doc,
		.execname = parser->execname,
	};
	int arg_index, i;
	int argc = *p_argc;
	const char* argcmd;
	const char** argv = *p_argv;
	const struct subcmd* subcmd = NULL;

	/* Parse command line options */
	arg_index = mmarg_parse(&argparser, argc, (char**)argv);
	if (arg_index < 0)
		return NULL;

	/* Run completion if last argument and completion requested */
	if (mmarg_is_completing() && (arg_index == argc-1)) {
		subcmd_complete_arg(parser, argv[argc-1]);
		mmarg_parse_complete(&argparser, argv[argc-1]);
		exit(EXIT_SUCCESS);
	}

	/* Check command is supplied, if not use default */
	argcmd = (arg_index+1 > argc) ? parser->defcmd : argv[arg_index];
	if (!argcmd)
		goto exit;

	/* Get command function to execute */
	for (i = 0; i < parser->num_subcmd; i++) {
		if (strcmp(parser->subcmds[i].name, argcmd) == 0)
			subcmd = &parser->subcmds[i];
	}

	if (subcmd == NULL) {
		fprintf(stderr, "Invalid command: %s."
		        " Run \"%s --help\" to see Usage\n",
		        argcmd, parser->execname);
		return NULL;
	}

exit:
	*p_argv = argv + arg_index;
	*p_argc = argc - arg_index;

	return subcmd;
}


static
int is_file(char const * path)
{
	struct mm_stat st;

	if (mm_stat(path, &st, 0) != 0)
		return 0;

	return S_ISREG(st.mode);
}


/**
 * pkg_parser_init  -  init a structure struct pkg_parser
 * @pp: pointer to the structure to initialize
 */
LOCAL_SYMBOL
void pkg_parser_init(struct pkg_parser * pp)
{
	*pp = (struct pkg_parser) {0};
}


/**
 * pkg_parser_deinit  -  deinit a structure struct pkg_parser
 * @pp: the structure to deinitialize
 */
LOCAL_SYMBOL
void pkg_parser_deinit(struct pkg_parser * pp)
{
	constraints_deinit(&pp->cons);
	mmstr_free(pp->name);
	pp->name = NULL;
}


/**
 * parse_pkgreq() -  fills the request asked by the user.
 * @ctx: context associated with prefix
 * @pkg_req: an entry matching "pkg_name[=pkg_version]".
 * @pp: the request to fill
 *
 * pkg_name, on the entry, is potentially a path toward the package (in this
 * case no option is possible).
 *
 * Returns: 0 in case the commandline is successfully read, -1 otherwise.
 */
LOCAL_SYMBOL
int parse_pkgreq(struct mmpack_ctx * ctx, const char* pkg_req,
                 struct pkg_parser * pp)
{
	int len;
	struct mmpkg * pkg;
	mmstr * tmp, * arg_full;
	char * equal;

	// case where pkg_name is actually a path toward the package
	if (is_file(pkg_req)) {
		tmp = mmstr_alloca_from_cstr(pkg_req);
		len = mmstrlen(ctx->cwd) + 1 + mmstrlen(tmp);
		arg_full = mmstr_malloca(len);
		mmstr_join_path(arg_full, ctx->cwd, tmp);

		pkg = add_pkgfile_to_binindex(&ctx->binindex, arg_full);
		mmstr_freea(arg_full);
		if (pkg == NULL) {
			printf("Bad commandline argument or syntax\n");
			return -1;
		}

		pp->pkg = pkg;
		return 0;
	}

	/* Parsing of pkg_req */
	equal = strchr_or_end(pkg_req, '=');
	pp->name = mmstr_copy_realloc(pp->name, pkg_req, equal - pkg_req);
	if (*equal == '=')
		pp->cons.version =
			mmstrcpy_cstr_realloc(pp->cons.version, equal + 1);

	return 0;
}


/**
 * parse_pkg() -  returns the package wanted.
 * @ctx:          context associated with prefix
 * @pkg_arg:      an entry matching "pkg_name[=pkg_version]".
 *
 * pkg_name, on the entry, is potentially a path toward the package (in this
 * case no option is possible).
 *
 * Return: the package having pkg_name as name and pkg_version as version. In
 * the case where pkg_version is NULL (the entry was "pkg_name" without the
 * "=pkg_version"), the package returned is the latest one. If no such package
 * is found, NULL is returned.
 */
LOCAL_SYMBOL
struct mmpkg const* parse_pkg(struct mmpack_ctx * ctx, const char* pkg_arg)
{
	struct pkg_parser pp;
	struct mmpkg const* pkg;
	struct constraints * cons;

	pkg_parser_init(&pp);

	if (parse_pkgreq(ctx, pkg_arg, &pp)) {
		pkg = NULL;
		goto exit;
	}

	cons = &pp.cons;

	if (!(pkg = binindex_lookup(&ctx->binindex, pp.name, cons)))
		info("No package %s (%s)\n", pp.name,
		     cons->version ? cons->version : "any version");

exit:
	pkg_parser_deinit(&pp);
	return pkg;
}


struct cb_data {
	mmstr const * sumsha;
	struct mmpkg * pkg;
};


static
int cb_binindex(struct mmpkg * pkg, void * void_data)
{
	struct cb_data * data = (struct cb_data*) void_data;

	if (mmstrcmp(pkg->sumsha, data->sumsha) == 0) {
		data->pkg = pkg;
		return 0;
	}

	return -1;
}


/**
 * find_package_by_sumsha() -  find the package associated with the sumsha given
 *                             in argument.
 *
 * @binindex:      the binary package index
 * @sumsha_req:    the sumsha searched
 *
 * Return: the package having the sumsha @sumsha_req, NULL if not found.
 */
LOCAL_SYMBOL
struct mmpkg const* find_package_by_sumsha(struct mmpack_ctx * ctx,
                                           const char* sumsha_req)
{
	struct cb_data data;

	data.sumsha = mmstr_malloc_from_cstr(sumsha_req);
	data.pkg = NULL;
	binindex_foreach(&ctx->binindex, cb_binindex, &data);
	mmstr_free(data.sumsha);

	if (data.pkg == NULL)
		info("No package with sumsha: %s\n", sumsha_req);

	return data.pkg;
}


/**
 * complete_pkgname() - complete name of package
 * @ctx:        context associated with prefix
 * @arg:        incomplete argument supplied on command line
 * @type:       if ONLY_INSTALLED, completion must be performed on the name of
 *              installed packages. If AVAILABLE_PKGS, the completion must
 *              be performed on the name of available packages.
 *
 * Return: 0 in case of success, -1 otherwise.
 */
LOCAL_SYMBOL
int complete_pkgname(struct mmpack_ctx * ctx, const char* arg,
                     enum pkg_comp_type type)
{
	struct indextable* table;
	struct it_iterator iter;
	struct it_entry* entry;
	int arglen = strlen(arg);

	// During completion, chances are not negligeable that error arises
	// (incomplete name, start searching in non-existing folder). Those
	// error must not be logged. Actually standard error should normally
	// be redirected to /dev/null for completion.
	if (mmpack_ctx_use_prefix(ctx, CTX_SKIP_REDIRECT_LOG))
		return -1;

	// indextable of install_state or binary index store data radically
	// different... However both index their data by package name:
	// installed_state index the packages that are installed in the prefix,
	// binary index, all the different versions available of the same
	// package name.
	if (type == ONLY_INSTALLED)
		table = &ctx->installed.idx;
	else
		table = &ctx->binindex.pkgname_idx;

	// Loop over the entries of the index table and print if a beginning
	// of key match argument
	entry = it_iter_first(&iter, table);
	while (entry) {
		if (strncmp(arg, entry->key, arglen) == 0)
			printf("%s\n", entry->key);

		entry = it_iter_next(&iter);
	}

	return 0;
}
