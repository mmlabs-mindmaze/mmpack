/*
 * @mindmaze_header@
 */
#ifndef SRCINDEX_H
#define SRCINDEX_H

#include "indextable.h"
#include "settings.h"

struct srcpkg {
	mmstr * name;
	mmstr * filename;
	mmstr * sha256;
	mmstr * version;
	size_t size;
	int name_id;
	struct repolist_elt * repo;
};

struct srcindex {
	struct indextable pkgname_idx;
};

void srcindex_init(struct srcindex* srcindex);
void srcindex_deinit(struct srcindex* srcindex);
int srcindex_populate(struct srcindex * srcindex, char const * index_filename,
                      struct repolist_elt * repo);

#endif /* SRCINDEX_H */
