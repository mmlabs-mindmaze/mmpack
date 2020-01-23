/*
 * @mindmaze_header@
 */

#ifndef MMPACK_REPO_H
#define MMPACK_REPO_H

#include "context.h"

#define REPO_SYNOPSIS "repo [add|list|remove|rename] <name> <url>"

int mmpack_repo(struct mmpack_ctx * ctx, int argc, char const ** argv);

#endif /* MMPACK_REPO_H */

