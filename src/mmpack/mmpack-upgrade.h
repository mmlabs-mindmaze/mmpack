/*
 * @mindmaze_header@
 */

#ifndef MMPACK_UPGRADE_H
#define MMPACK_UPGRADE_H

#include <stdbool.h>

#include "context.h"

#define UPGRADE_SYNOPSIS "upgrade [upgrade-opts] [<pkg> [...]]"

int mmpack_upgrade(struct mmpack_ctx * ctx, int argc, char const ** argv);
int mmpack_upgrade_from_repos(struct mmpack_ctx* ctx, bool skip_confirm,
                              int nreq, const char** req_args);

#endif /* MMPACK_UPDATE_H */

