/*
 * @mindmaze_header@
 */

#ifndef MMPACK_CHECK_SYSDEP_H
#define MMPACK_CHECK_SYSDEP_H

#include "context.h"

#define CHECK_SYSDEP_SYNOPSIS \
	"check-sysdep [<syspkg_spec> [...]]"

int mmpack_check_sysdep(struct mmpack_ctx * ctx, int argc, const char* argv[]);

#endif /* MMPACK_CHECK_INTEGRITY_H */

