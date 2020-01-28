/*
 * @mindmaze_header@
 */

#ifndef MMPACK_RDEPENDS_H
#define MMPACK_RDEPENDS_H

#include "context.h"

#define RDEPENDS_SYNOPSIS \
	"rdepends [-r|--recursive] [--repo<=repo_name>] <package>[=[key:]<value>]"


struct list_pkgs {
	struct mmpkg const * pkg;
	struct list_pkgs * next;
};

void list_pkgs_add_elt(struct list_pkgs ** list, struct mmpkg const * pkg);
int list_pkgs_search_elt(struct list_pkgs * list, struct mmpkg const * pkg);
void list_pkgs_destroy_all_elt(struct list_pkgs ** list);
int find_reverse_dependencies(struct binindex binindex,
                              struct mmpkg const* pkg,
                              const struct repolist_elt * repo,
                              struct list_pkgs** rdep_list,
                              int rec);

int mmpack_rdepends(struct mmpack_ctx * ctx, int argc, char const ** argv);

#endif /* MMPACK_RDEPENDS_H */
