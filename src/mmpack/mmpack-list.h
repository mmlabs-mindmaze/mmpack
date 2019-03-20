/*
 * @mindmaze_header@
 */

#ifndef MMPACK_LIST_H
#define MMPACK_LIST_H

#include "context.h"

#define LIST_SYNOPSIS \
	"list [all|available|upgradeable|installed|extras] [*pattern*]"

int mmpack_list(struct mmpack_ctx * ctx, int argc, const char* argv[]);

#endif /* MMPACK_LIST_H */

