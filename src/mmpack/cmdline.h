/*
 * @mindmaze_header@
 */

#ifndef CMDLINE_H
#define CMDLINE_H

#include <mmargparse.h>

#include "context.h"

enum pkg_comp_type {
	AVAILABLE_PKGS,
	ONLY_INSTALLED,
};

typedef int (* subcmd_proc)(struct mmpack_ctx * ctx, int argc,
                            const char* argv[]);

struct subcmd {
	const char* name;
	subcmd_proc cb;
};


/**
 * strcut cmdline_constraints - structure containing all the possible
 *                              constraints imposed by the user in the command
 *                              line.
 * @pkg_name: package name
 * @pkg_version: package version
 * @repo_name: name of the repository in which the package should be searched
 * @sumsha: package sumsha
 */
struct cmdline_constraints {
	mmstr const * pkg_name;
	mmstr const * pkg_version;
	mmstr const * repo_name;
	mmstr const * pkg_sumsha;
};


/**
 * struct subcmd_parser - subcmd and option parser configuration
 * @num_opt:    number of element in @optv.
 * @optv:       array of option supported. same as in struct mmarg_parser
 * @args_doc:   lines of synopsis of program usage (excluding program name).
 *              This can support multiple line (like for different case of
 *              invocation). Can be NULL.
 * @doc:        document of the program. same as in struct mmarg_parser
 * @execname:   name of executable. You are invited to set it to argv[0]. If
 *              NULL, "PROGRAM" will be used instead for synopsis
 * @num_subcmd: number of element in @subcmds array
 * @subcmds:    pointer to array of subcmd listing the possible command
 * @defcmd:     pointer to string interpreted as sub command if none is
 *              identified in arguments. If NULL, a missing subcommand will
 *              result in error of parsing.
 */
struct subcmd_parser {
	int num_opt;
	const struct mmarg_opt* optv;
	const char* doc;
	const char* args_doc;
	const char* execname;
	int num_subcmd;
	const struct subcmd* subcmds;
	const char* defcmd;
};

const struct subcmd* subcmd_parse(const struct subcmd_parser* parser,
                                  int* p_argc, const char*** p_argv);
void cmdline_constraints_init(struct cmdline_constraints ** cc);
void cmdline_constraints_deinit(struct cmdline_constraints ** cc);
struct cmdline_constraints* parse_cmdline(const char * pkg_req);
int complete_pkgname(struct mmpack_ctx * ctx, const char* arg,
                     enum pkg_comp_type type);

#endif /* CMDLINE_H */
