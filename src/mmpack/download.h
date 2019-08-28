/*
 * @mindmaze_header@
 */
#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include "context.h"
#include "mmstring.h"

int download_from_repo(struct mmpack_ctx * ctx,
                       const mmstr* repo, const mmstr* repo_relpath,
                       const mmstr* prefix, const mmstr* prefix_relpath);

#endif /* DOWNLOAD_H */

