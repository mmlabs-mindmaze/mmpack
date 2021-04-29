/*
 * @mindmaze_header@
 */
#ifndef PACKAGE_UTILS_H
#define PACKAGE_UTILS_H

#include <stdio.h>

#include "binindex.h"
#include "binpkg.h"
#include "constraints.h"
#include "install-state.h"
#include "repo.h"
#include "strlist.h"
#include "utils.h"


int pkg_version_compare(char const * v1, char const * v2);


int binindex_populate(struct binindex* binindex, char const * index_filename,
                      const struct repo* repo);
struct binpkg* binindex_add_pkgfile(struct binindex* binindex,
                                    char const * filename);

#endif /* PACKAGE_UTILS_H */
