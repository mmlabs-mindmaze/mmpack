/*
 * @mindmaze_header@
 */

#ifndef MMPACK_SEARCH_H
#define MMPACK_SEARCH_H

#include "context.h"

#define SEARCH_SYNOPSIS \
	"search <pkg-name>"

int mmpack_search(struct mmpack_ctx * ctx, int argc, const char* argv[]);

#endif /* MMPACK_SEARCH_H */

