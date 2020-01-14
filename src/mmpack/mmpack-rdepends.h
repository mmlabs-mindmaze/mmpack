/*
 * @mindmaze_header@
 */

#ifndef MMPACK_RDEPENDS_H
#define MMPACK_RDEPENDS_H

#include "context.h"

#define RDEPENDS_SYNOPSIS \
	"rdepends [-r|--recursive] [--repo<=repo_name>] <package>[=[key:]<value>]"

int mmpack_rdepends(struct mmpack_ctx * ctx, int argc, char const ** argv);

#endif /* MMPACK_RDEPENDS_H */
