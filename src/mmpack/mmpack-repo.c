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
#include <string.h>

#include "cmdline.h"
#include "common.h"
#include "context.h"
#include "mmpack-repo.h"
#include "mmstring.h"
#include "utils.h"


static
int repo_add(struct mmpack_ctx* ctx, int argc, char const ** argv)
{
	if (argc != 2) {
		printf("usage: mmpack repo add <name> <url>\n");
		return -1;
	}

	if (repolist_lookup(&ctx->settings.repo_list, argv[0])) {
		printf("repository \"%s\" already exists\n", argv[0]);
		return -1;
	}

	repolist_add(&ctx->settings.repo_list, argv[0], argv[1]);

	return settings_serialize(ctx->prefix, &ctx->settings, 1);
}


static
int repo_list(struct mmpack_ctx* ctx, int argc, char const ** argv)
{
	struct repolist_elt * repo;

	(void) argv; /* silence unused warnings */
	if (argc > 0) {
		printf("usage: mmpack repo list\n");
		return -1;
	}

	for (repo = ctx->settings.repo_list.head;
	     repo != NULL;
	     repo = repo->next)
		printf("%s\t%s\n", repo->name, repo->url);

	return 0;
}


static
int repo_remove(struct mmpack_ctx* ctx, int argc, char const ** argv)
{
	if (argc != 1) {
		printf("usage: mmpack repo remove <name>\n");
		return -1;
	}

	if (repolist_remove(&ctx->settings.repo_list, argv[0]) != 0) {
		printf("No such repository: \"%s\"\n", argv[0]);
		return -1;
	}

	return settings_serialize(ctx->prefix, &ctx->settings, 1);
}


static
int repo_rename(struct mmpack_ctx* ctx, int argc, char const ** argv)
{
	if (argc != 2) {
		printf("usage: mmpack repo rename <old> <new>\n");
		return -1;
	}

	char const * name = argv[0];
	int name_len = strlen(name);
	struct repolist_elt * elt = ctx->settings.repo_list.head;

	while (elt != NULL) {
		if (name_len == mmstrlen(elt->name)
		    && strncmp(elt->name, name, name_len) == 0) {

			mmstr_free(elt->name);
			elt->name = mmstr_malloc_from_cstr(argv[1]);
			return settings_serialize(ctx->prefix,
			                          &ctx->settings, 1);
		}

		elt = elt->next;
	}

	printf("No such repository: \"%s\"\n", name);
	return -1;
}


static const struct subcmd repo_subcmds[] = {
	{"add", repo_add},
	{"list", repo_list},
	{"remove", repo_remove},
	{"rename", repo_rename},
};


/**
 * mmpack_repo() - main function for the command to manage repositories
 * @ctx: mmpack context
 * @argc: number of arguments
 * @argv: array of arguments
 *
 * TODOC
 *
 * Return: 0 on success, -1 otherwise
 */
LOCAL_SYMBOL
int mmpack_repo(struct mmpack_ctx * ctx, int argc, char const ** argv)
{
	const struct subcmd* subcmd;

	struct subcmd_parser cmd_parser = {
		.execname = "mmpack",
		.args_doc = REPO_SYNOPSIS,
		.num_subcmd = MM_NELEM(repo_subcmds),
		.subcmds = repo_subcmds,
		.defcmd = "list",
	};

	subcmd = subcmd_parse(&cmd_parser, &argc, &argv);
	if (!subcmd)
		return -1;

	/* If completing, nothing should be further displayed */
	if (mmarg_is_completing())
		return 0;

	/* Load prefix configuration and caches */
	if (mmpack_ctx_use_prefix(ctx, 0))
		return -1;

	return subcmd->cb(ctx, argc - 1, argv + 1);
}
