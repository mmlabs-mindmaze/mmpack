/*
 * @mindmaze_header@
 */

#ifndef MMPACK_SHOW_H
#define MMPACK_SHOW_H

#include "context.h"

#define SHOW_SYNOPSIS \
	"show <pkg-name>"

int mmpack_show(struct mmpack_ctx * ctx, int argc, const char* argv[]);

#endif /* MMPACK_SHOW_H */

