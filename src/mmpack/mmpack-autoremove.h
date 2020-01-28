/*
 * @mindmaze_header@
 */

#ifndef MMPACK_AUTOREMOVE_H
#define MMPACK_AUTOREMOVE_H

#include "context.h"

#define AUTOREMOVE_SYNOPSIS "autoremove [-y|--yes-assumed]"

int mmpack_autoremove(struct mmpack_ctx * ctx, int argc, char const ** argv);

#endif /* MMPACK_AUTOREMOVE_H */
