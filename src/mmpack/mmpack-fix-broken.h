/*
 * @mindmaze_header@
 */

#ifndef MMPACK_FIX_BROKEN_H
#define MMPACK_FIX_BROKEN_H

#include "context.h"

#define FIX_BROKEN_SYNOPSIS \
	"fix-broken [<pkg> [...]]"

int mmpack_fix_broken(struct mmpack_ctx * ctx, int argc, const char ** argv);

#endif /* MMPACK_FIX_BROKEN_H */

