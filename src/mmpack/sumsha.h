/*
 * @mindmaze_header@
 */

#ifndef SUMSHA_H
#define SUMSHA_H

#include "binpkg.h"
#include "mmstring.h"
#include "strlist.h"


mmstr* sha256sums_path(const struct binpkg* pkg);
int read_sha256sums(const mmstr* sha256sums_path,
                    struct strlist* filelist, struct strlist* hashlist);


#endif /* SUMSHA_H */
