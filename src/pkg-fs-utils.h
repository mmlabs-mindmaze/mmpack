/*
 * @mindmaze_header@
 */
#ifndef PKG_FS_UTILS_H
#define PKG_FS_UTILS_H

#include "action-solver.h"
#include "context.h"
#include "mmstring.h"

int is_mmpack_metadata(mmstr const * path);
int apply_action_stack(struct mmpack_ctx* ctx, struct action_stack* stack);

#endif
