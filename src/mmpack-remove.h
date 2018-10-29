/*
 * @mindmaze_header@
 */

#ifndef MMPACK_REMOVE_H
#define MMPACK_REMOVE_H

#include "context.h"

#define REMOVE_SYNOPSIS \
	"remove [remove-opts] <pkg1> [<pkg2> [<pkg3> [...]]]"

int mmpack_remove(struct mmpack_ctx * ctx, int argc, const char* argv[]);

#endif
