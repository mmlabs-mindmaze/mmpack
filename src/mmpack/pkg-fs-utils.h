/*
 * @mindmaze_header@
 */
#ifndef PKG_FS_UTILS_H
#define PKG_FS_UTILS_H

#include "action-solver.h"
#include "context.h"
#include "mmstring.h"

int is_mmpack_metadata(mmstr const * path);
int check_installed_pkg(const struct mmpack_ctx* ctx, const struct mmpkg* pkg);
int apply_action_stack(struct mmpack_ctx* ctx, struct action_stack* stack);
int pkg_get_mmpack_info(char const * mpk_filename, struct buffer * buffer);
int download_package(struct mmpack_ctx * ctx, struct mmpkg const * pkg,
                     mmstr const * pathname);

#endif /* ifndef PKG_FS_UTILS_H */
