/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmargparse.h>
#include <mmsysio.h>
#include <mmerrno.h>

#include "cmdline.h"
#include "constraints.h"
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
 * This function extents the functionality provided by mm_arg_parse() from
 * mmlib. It parses the command line option and try to match the first
 * non-option argument to one of the subcmd listed in @parser->subcmds. If
 * no sub command if found on command line, the string pointed by
 * @parser->defcmd, if not NULL, will be interpreted as the provided sub
 * command.
 *
 * If mm_arg_is_completing() reports that the shell completion is requested,
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
	struct mm_arg_parser argparser = {
		.flags = mm_arg_is_completing() ? MM_ARG_PARSER_COMPLETION : 0,
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
	arg_index = mm_arg_parse(&argparser, argc, (char**)argv);
	if (arg_index < 0)
		return NULL;

	/* Run completion if last argument and completion requested */
	if (mm_arg_is_completing() && (arg_index == argc-1)) {
		subcmd_complete_arg(parser, argv[argc-1]);
		mm_arg_parse_complete(&argparser, argv[argc-1]);
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
	int previous, rv;

	// Call mm_stat() without changing error state nor error log
	previous = mm_error_set_flags(MM_ERROR_SET, MM_ERROR_IGNORE);
	rv = mm_stat(path, &st, 0);
	mm_error_set_flags(previous, MM_ERROR_IGNORE);
	if (rv != 0)
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


enum constraints_type {
	UNKNOWN = -1,
	HASH = 0,
	REPO,
	PKG_VERSION,
};


static
const char * constraints_names[] = {
	[HASH] = "hash",
	[REPO] = "repo",
	[PKG_VERSION] = "version",
};


static
enum constraints_type get_constraints_type(const char * key, size_t size_key)
{
	int i;
	size_t size;

	for (i = 0; i < MM_NELEM(constraints_names); i++) {
		size = strlen(constraints_names[i]);

		if (size_key == size &&
		    !memcmp(key, constraints_names[i], size))
			return i;
	}

	return UNKNOWN;
}


static
int constraints_set(struct mmpack_ctx * ctx,
                    struct constraints * cons,
                    const char * arg_req)
{
	enum constraints_type type = PKG_VERSION;
	const char * value = arg_req;
	const char * colon;

	// Try to split the argument in key/value. If no colon, the whole
	// argument is assumed to be version
	colon = strchr_or_end(arg_req, ':');
	if (*colon == ':') {
		value = colon + 1;
		type = get_constraints_type(arg_req, colon - arg_req);
	}

	switch (type) {
	case HASH:
		cons->sumsha = xx_malloc(sizeof(*cons->sumsha));
		digest_from_hexstr(cons->sumsha,
		                     (struct strchunk) {.buf = value, .len = strlen(value)});
		break;
	case REPO:
		cons->repo = repolist_lookup(&ctx->settings.repo_list, value);
		if (!cons->repo) {
			printf("Repository %s not found\n", value);
			return -1;
		}

		break;
	case PKG_VERSION:
		cons->version = mmstrcpy_cstr_realloc(cons->version, value);
		break;
	default:
		printf("Unknown constraint key %.*s\n", (int) (colon - arg_req),
		       arg_req);
		return -1;
	}

	return 0;
}


/**
 * parse_pkgreq() -  fills the request asked by the user.
 * @ctx: context associated with prefix
 * @pkg_req: an entry matching "pkg_name[=key:value]" or "pkg_name[=value]"
 * @pp: the request to fill
 *
 * pkg_name, on the entry, is potentially a path toward the package (in this
 * case no option is possible). "key" can be either equal to "hash", to "repo"
 * or to "version". It is also possible to omit key ("pkg_name[=value]"), in
 * this case the value corresponds to the version of the package.
 *
 * Returns: 0 in case the commandline is successfully read, -1 otherwise.
 */
LOCAL_SYMBOL
int parse_pkgreq(struct mmpack_ctx * ctx, const char* pkg_req,
                 struct pkg_parser * pp)
{
	int len;
	struct binpkg * pkg;
	mmstr * tmp, * arg_full;
	char * equal;

	// case where pkg_name is actually a path toward the package
	if (is_file(pkg_req)) {
		tmp = mmstr_alloca_from_cstr(pkg_req);
		len = mmstrlen(ctx->cwd) + 1 + mmstrlen(tmp);
		arg_full = mmstr_malloca(len);
		mmstr_join_path(arg_full, ctx->cwd, tmp);

		pkg = binindex_add_pkgfile(&ctx->binindex, arg_full);
		mmstr_freea(arg_full);
		if (pkg == NULL) {
			printf("Package not found or malformed package\n");
			return -1;
		}

		pp->pkg = pkg;
		return 0;
	}

	/* Parsing of pkg_req */
	equal = strchr_or_end(pkg_req, '=');
	pp->name = mmstr_copy_realloc(pp->name, pkg_req, equal - pkg_req);
	if (*equal++ == '=') {
		if (constraints_set(ctx, &pp->cons, equal))
			return -1;
	}

	return 0;
}


/**
 * parse_pkg() -  returns the package wanted.
 * @ctx:          context associated with prefix
 * @pkg_arg:      an entry matching "pkg_name[=key:value]" or "pkg_name[=value]"
 *
 * pkg_name, on the entry, is potentially a path toward the package (in this
 * case no option is possible). "key" can be either equal to "hash", to "repo"
 * or to "version". It is also possible to omit key ("pkg_name[=value]"), in
 * this case the value corresponds to the version of the package.
 *
 * Return: the package having pkg_name as name and meeting the constraints. In
 * the case where pkg_version is NULL, the package returned is the latest one.
 * If no such package is found, NULL is returned.
 */
LOCAL_SYMBOL
struct binpkg const* parse_pkg(struct mmpack_ctx * ctx, const char* pkg_arg)
{
	struct pkg_parser pp;
	struct binpkg const* pkg;
	struct constraints * cons;

	pkg_parser_init(&pp);

	if (parse_pkgreq(ctx, pkg_arg, &pp)) {
		pkg = NULL;
		goto exit;
	}

	cons = &pp.cons;

	if (!(pkg = binindex_lookup(&ctx->binindex, pp.name, cons)))
		printf("No package %s%s\n", pp.name,
		       constraints_is_empty(cons) ?
		       "" : " respecting the constraints");

exit:
	pkg_parser_deinit(&pp);
	return pkg;
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
