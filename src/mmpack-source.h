/*
 * @mindmaze_header@
 */

#ifndef MMPACK_SOURCE_H
#define MMPACK_SOURCE_H

#include "context.h"

#define SOURCE_SYNOPSIS \
	"source <pkg-name>"

int mmpack_source(struct mmpack_ctx * ctx, int argc, const char* argv[]);

#endif /* MMPACK_SOURCE_H */

