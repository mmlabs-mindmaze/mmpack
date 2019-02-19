/*
 * @mindmaze_header@
 */

#ifndef MMPACK_UPGRADE_H
#define MMPACK_UPGRADE_H

#include "context.h"

#define UPGRADE_SYNOPSIS "upgrade [upgrade-opts] [<pkg> [...]]"

int mmpack_upgrade(struct mmpack_ctx * ctx, int argc, char const ** argv);

#endif /* MMPACK_UPDATE_H */

