/*
 * @mindmaze_header@
 */
#ifndef SRCINDEX_H
#define SRCINDEX_H

#include "indextable.h"

struct srcpkg {
	mmstr const * name;
	mmstr const * filename;
	mmstr const * sha256;
	mmstr const * version;
	size_t size;
	int name_id;
};

struct srcindex {
	struct indextable pkgname_idx;
	struct srclist * pkgname_table;
	int num_pkgname;
};

void srcindex_init(struct srcindex* srcindex);
void srcindex_deinit(struct srcindex* srcindex);
int srcindex_populate(struct srcindex * srcindex, char const * index_filename);
int srcindex_get_pkgname_id(struct srcindex* srcindex, const mmstr* name);

#endif /* SRCINDEX_H */
