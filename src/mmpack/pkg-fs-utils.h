/*
 * @mindmaze_header@
 */
#ifndef PKG_FS_UTILS_H
#define PKG_FS_UTILS_H

#include "action-solver.h"
#include "buffer.h"
#include "context.h"
#include "mmstring.h"

int is_mmpack_metadata(mmstr const * path);
int check_installed_pkg(const struct binpkg* pkg);
int apply_action_stack(struct mmpack_ctx* ctx, struct action_stack* stack);
int pkg_load_pkginfo(const char* mpk_filename, struct buffer * buffer);
int download_package(struct mmpack_ctx * ctx, struct binpkg const * pkg,
                     mmstr const * pathname);

#endif /* ifndef PKG_FS_UTILS_H */
