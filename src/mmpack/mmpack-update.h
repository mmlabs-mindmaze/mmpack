/*
 * @mindmaze_header@
 */

#ifndef MMPACK_UPDATE_H
#define MMPACK_UPDATE_H

#include "context.h"

#define UPDATE_SYNOPSIS "update"

int mmpack_update_all(struct mmpack_ctx * ctx, int argc, char const ** argv);
int mmpack_update_repos(struct mmpack_ctx* ctx);

#endif /* MMPACK_UPDATE_H */
