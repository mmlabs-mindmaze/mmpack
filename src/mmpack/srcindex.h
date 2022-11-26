/*
 * @mindmaze_header@
 */
#ifndef SRCINDEX_H
#define SRCINDEX_H

#include "crypto.h"
#include "indextable.h"
#include "mmstring.h"
#include "repo.h"

struct srcpkg {
	const mmstr* name;
	const mmstr* version;
	struct sha_digest sha256;
	struct remote_resource* remote_res;
};

struct srcindex {
	struct indextable pkgname_idx;
};

void srcindex_init(struct srcindex* srcindex);
void srcindex_deinit(struct srcindex* srcindex);
int srcindex_populate(struct srcindex * srcindex, char const * index_filename,
                      const struct repo* repo);
const struct srcpkg* srcindex_lookup(struct srcindex* srcindex,
                                     const mmstr* srcname,
                                     const mmstr* version,
                                     const struct sha_digest* srchash);

#endif /* SRCINDEX_H */
