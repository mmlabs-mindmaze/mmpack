/*
 * @mindmaze_header@
 */

#ifndef MMPACK_CHECK_INTEGRITY_H
#define MMPACK_CHECK_INTEGRITY_H

#include "context.h"

#define CHECK_INTEGRITY_SYNOPSIS \
	"check-integrity [<pkg-name>]"

int mmpack_check_integrity(struct mmpack_ctx * ctx, int argc,
                           const char* argv[]);

#endif /* MMPACK_CHECK_INTEGRITY_H */

