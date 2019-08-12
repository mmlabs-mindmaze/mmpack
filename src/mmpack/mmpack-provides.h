/*
 * @mindmaze_header@
 */

#ifndef MMPACK_PROVIDES_H
#define MMPACK_PROVIDES_H

#include "context.h"

#define PROVIDES_SYNOPSIS \
	"provides [-u|--update] <pattern>"

int mmpack_provides(struct mmpack_ctx * ctx, int argc, char const ** argv);

#endif /* MMPACK_PROVIDES_H */

