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
int check_installed_pkg(const struct mmpack_ctx* ctx, const struct binpkg* pkg);
int apply_action_stack(struct mmpack_ctx* ctx, struct action_stack* stack);
int tar_load_file(const char* mpk_filename, const char* archive_path,
                  struct buffer * buffer);
int pkg_load_pkginfo(const char* mpk_filename, struct buffer * buffer);
int download_package(struct mmpack_ctx * ctx, struct binpkg const * pkg,
                     mmstr const * pathname);
int extract_tarball(const mmstr* target_dir, const char* tarfilename);

#endif /* ifndef PKG_FS_UTILS_H */
