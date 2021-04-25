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

#include "action-solver.h"
#include "cmdline.h"
#include "common.h"
#include "context.h"
#include "mmpack-install.h"
#include "mmpack-rdepends.h"
#include "mmstring.h"
#include "package-utils.h"
#include "utils.h"


static int recursive = 0;
static const char* repo_name = NULL;

static const struct mm_arg_opt cmdline_optv[] = {
	{"repo", MM_OPT_NEEDSTR, NULL, {.sptr = &repo_name},
	 "Specify @REPO_NAME as the address of package repository"},
	{"r|recursive", MM_OPT_NOVAL|MM_OPT_INT, "1", {.iptr = &recursive},
	 "Print recursively the reverse dependencies"},
};


struct list_pkgs {
	struct binpkg const * pkg;
	struct list_pkgs * next;
};


static
void add_elt_list_pkgs(struct list_pkgs ** list, struct binpkg const * pkg)
{
	struct list_pkgs * elt = malloc(sizeof(struct list_pkgs));

	elt->pkg = pkg;

	elt->next = *list;
	*list = elt;
}


static
int search_elt_list_pkgs(struct list_pkgs * list, struct binpkg const * pkg)
{
	struct list_pkgs * curr;

	for (curr = list; curr != NULL; curr = curr->next) {
		if (curr->pkg == pkg)
			return 0;
	}

	return -1;
}


static
void destroy_all_elt(struct list_pkgs ** list)
{
	struct list_pkgs * next;
	struct list_pkgs * curr = *list;

	while (curr) {
		next = curr->next;
		free(curr);
		curr = next;
	}
}


static
void dump_reverse_dependencies(struct list_pkgs * list)
{
	struct list_pkgs * curr;

	for (curr = list; curr != NULL; curr = curr->next) {
		printf("%s (%s)\n", curr->pkg->name, curr->pkg->version);
	}
}


static
int find_reverse_dependencies(struct binindex binindex,
                              struct binpkg const* pkg,
                              const struct repo* repo,
                              struct list_pkgs** rdep_list)
{
	struct rdeps_iter rdep_it;
	struct binpkg * rdep;

	if (!pkg || !binpkg_is_provided_by_repo(pkg, repo))
		return -1;

	// iterate over all the potential reverse dependencies of pkg
	for (rdep = rdeps_iter_first(&rdep_it, pkg, &binindex); rdep != NULL;
	     rdep = rdeps_iter_next(&rdep_it)) {
		// check that the reverse dependency belongs to the
		// repository inspected
		if (!binpkg_is_provided_by_repo(rdep, repo))
			continue;

		//check that the dependency is not already written
		if (search_elt_list_pkgs(*rdep_list, rdep))
			add_elt_list_pkgs(rdep_list, rdep);

		if (recursive)
			find_reverse_dependencies(binindex, rdep, repo,
			                          rdep_list);
	}

	return 0;
}


/**
 * mmpack_rdepends() - main function for the rdepends command
 * @ctx: mmpack context
 * @argc: number of arguments
 * @argv: array of arguments
 *
 * show given package reverse dependencies.
 *
 * Return: 0 on success, -1 otherwise
 */
LOCAL_SYMBOL
int mmpack_rdepends(struct mmpack_ctx * ctx, int argc, const char* argv[])
{
	struct binpkg const* pkg;
	struct pkg_parser pp;
	struct constraints * cons = &pp.cons;
	struct repo* repo = NULL;
	int arg_index, rv = -1;
	struct list_pkgs * rdep_list = NULL;

	struct mm_arg_parser parser = {
		.flags = mm_arg_is_completing() ? MM_ARG_PARSER_COMPLETION : 0,
		.args_doc = RDEPENDS_SYNOPSIS,
		.optv = cmdline_optv,
		.num_opt = MM_NELEM(cmdline_optv),
		.execname = "mmpack",
	};

	arg_index = mm_arg_parse(&parser, argc, (char**)argv);
	if (mm_arg_is_completing()) {
		if (arg_index + 1 < argc)
			return 0;

		return complete_pkgname(ctx, argv[argc - 1], AVAILABLE_PKGS);
	}

	if (arg_index + 1 != argc) {
		fprintf(stderr, "Bad usage of rdepends command.\n"
		        "Usage:\n\tmmpack "RDEPENDS_SYNOPSIS "\n");
		return -1;
	}

	// Load prefix configuration and caches
	if (mmpack_ctx_use_prefix(ctx, 0))
		return -1;

	pkg_parser_init(&pp);
	if (parse_pkgreq(ctx, argv[arg_index], &pp))
		goto exit;

	if (!(pkg = binindex_lookup(&ctx->binindex, pp.name, cons))) {
		printf("No package %s%s\n", pp.name,
		       constraints_is_empty(cons) ?
		       "" : " respecting the constraints");
		goto exit;
	}

	if (repo_name) {
		repo = repolist_lookup(&ctx->settings.repo_list, repo_name);
		if (!repo) {
			printf("No repository %s\n", repo_name);
			goto exit;
		}
	}

	if (find_reverse_dependencies(ctx->binindex, pkg, repo, &rdep_list)) {
		printf("No package found\n");
		goto exit;
	}

	dump_reverse_dependencies(rdep_list);

	rv = 0;

exit:
	pkg_parser_deinit(&pp);
	destroy_all_elt(&rdep_list);
	return rv;
}
