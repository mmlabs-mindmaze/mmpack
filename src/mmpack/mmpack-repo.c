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
#include <stdio.h>
#include <string.h>

#include "cmdline.h"
#include "common.h"
#include "context.h"
#include "mmpack-repo.h"
#include "mmstring.h"
#include "package-utils.h"
#include "settings.h"
#include "utils.h"


STATIC_CONST_MMSTR(repo_relpath, REPO_INDEX_RELPATH);


static
int repo_add(struct mmpack_ctx* ctx, int argc, char const ** argv)
{
	if (argc != 2) {
		printf("usage: mmpack repo add <name> <url>\n");
		return -1;
	}

	if (repolist_add(&ctx->settings.repo_list, argv[0], argv[1]) != 0)
		return -1;

	if (create_empty_binindex_file(ctx->prefix, argv[0]) == -1) {
		info("binindex file was not created\n");
		return -1;
	}

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
int remove_binindex_file(const mmstr* prefix, char const * name)
{
	mmstr * avllist_relpath;
	int rv = 0;
	int len;

	len = mmstrlen(prefix) + mmstrlen(repo_relpath) + strlen(name) + 2;
	avllist_relpath = mmstr_malloc(len);

	mmstr_join_path(avllist_relpath, prefix, repo_relpath);

	// Append the name of the repo
	mmstrcat_cstr(mmstrcat_cstr(avllist_relpath, "."), name);

	if (mm_unlink(avllist_relpath) != 0) {
		rv = -1;
	}

	mmstr_free(avllist_relpath);
	return rv;
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

	remove_binindex_file(ctx->prefix, argv[0]);

	return settings_serialize(ctx->prefix, &ctx->settings, 1);
}


static
int rename_binindex_file(const mmstr* prefix, char const * old_name,
                         char const * new_name)
{
	mmstr * old, * new;
	int old_len, new_len;
	int rv = 0;

	// create the path of the old and new names
	old_len = mmstrlen(prefix) + mmstrlen(repo_relpath) +
	          strlen(old_name) + 2;
	new_len = mmstrlen(prefix) + mmstrlen(repo_relpath) +
	          strlen(new_name) + 2;

	old = mmstr_malloca(old_len);
	new = mmstr_malloca(new_len);

	mmstr_join_path(old, prefix, repo_relpath);
	mmstrcat_cstr(mmstrcat_cstr(old, "."), old_name);

	mmstr_join_path(new, prefix, repo_relpath);
	mmstrcat_cstr(mmstrcat_cstr(new, "."), new_name);

	if (rename(old, new) != 0) {
		rv = -1;
	}

	mmstr_freea(old);
	mmstr_freea(new);
	return rv;
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

	// check that no repository possesses already this name
	if (repolist_lookup(&ctx->settings.repo_list, argv[1])) {
		error("repository \"%s\" already exists\n", argv[1]);
		return -1;
	}

	while (elt != NULL) {
		if (name_len == mmstrlen(elt->name)
		    && strncmp(elt->name, name, name_len) == 0) {

			mmstr_free(elt->name);
			elt->name = mmstr_malloc_from_cstr(argv[1]);
			rename_binindex_file(ctx->prefix, argv[0], argv[1]);
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
