/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmargparse.h>

#include "cmdline.h"
#include "context.h"
#include "indextable.h"
#include "package-utils.h"


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


/**
 * cmdline_constraints_init  -  init a structure struct cmdline_constraints
 *
 * @cc: the structure to initialize
 */
LOCAL_SYMBOL
void cmdline_constraints_init(struct cmdline_constraints ** cc)
{
	*cc = xx_malloc(sizeof(cmdline_constraints));
	*cc = (struct cmdline_constraints) {0};	
}


/**
 * cmdline_constraints_deinit  -  deinit a structure struct cmdline_constraints
 *
 * @cc: the structure to deinitialize
 */
LOCAL_SYMBOL
void cmdline_constraints_deinit(struct cmdline_constraints ** cc)
{
	  mmstr_free(*cc->pkg_name);
	  mmstr_free(*cc->pkg_version);
	  mmstr_free(*cc->pkg_sumsha);
	  free(*cc);
}


/**
 * parse_cmdline() -  returns the structure of constraints imposed by the user.
 *
 * @pkg_req: an entry matching either "pkg_name=sumsha:sumsha", 
 *           "pkg_name=pkg_version" or "pkg_name"
 *
 * Return: the structure of constraints imposed by the user.
 */
LOCAL_SYMBOL
struct cmdline_constraints * parse_cmdline(const char* pkg_req)
{
	const char * first_sep;
	const char * second_sep;
	struct cmdline_constraints * r;

       	cmdline_constraints_init(&r);

	/* Find the first occurrence of '=' */
	first_sep = strchr(pkg_req, '=');
	second_sep = strchr(pkg_req, ":");
	if (first_sep == NULL && second_sep == NULL)
		r->pkg_name = mmstr_malloc_from_cstr(pkg_req);
	else { 
		/* The package name is before the '=' character */
		r->pkg_name = mmstr_malloc_copy(pkg_req, first_sep - pkg_req);
		if (second_sep == NULL) {
			r->pkg_version = mmstr_malloc_from_cstr(first_sep + 1);
		}



	return ret;
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
