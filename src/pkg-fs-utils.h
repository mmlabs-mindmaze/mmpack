/*
 * @mindmaze_header@
 */
#ifndef PKG_FS_UTILS_H
#define PKG_FS_UTILS_H

#include "context.h"
#include "action-solver.h"

int apply_action_stack(struct mmpack_ctx* ctx, struct action_stack* stack);

#endif
