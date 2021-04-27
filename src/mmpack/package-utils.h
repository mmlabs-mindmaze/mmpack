/*
 * @mindmaze_header@
 */
#ifndef PACKAGE_UTILS_H
#define PACKAGE_UTILS_H

#include <stdio.h>

#include "binindex.h"
#include "binpkg.h"
#include "constraints.h"
#include "indextable.h"
#include "mmstring.h"
#include "repo.h"
#include "strlist.h"
#include "utils.h"


int pkg_version_compare(char const * v1, char const * v2);


struct install_state {
	struct indextable idx;
	int pkg_num;
};


struct inststate_iter {
	struct it_iterator it_iter;
};


static inline
const struct binpkg* inststate_first(struct inststate_iter* iter,
                                     struct install_state* state)
{
	struct it_entry* entry;

	entry = it_iter_first(&iter->it_iter, &state->idx);
	if (!entry)
		return NULL;

	return entry->value;
}


static inline
const struct binpkg* inststate_next(struct inststate_iter* iter)
{
	struct it_entry* entry;

	entry = it_iter_next(&iter->it_iter);
	if (!entry)
		return NULL;

	return entry->value;
}


int install_state_init(struct install_state* state);
int install_state_copy(struct install_state* restrict dst,
                       const struct install_state* restrict src);
void install_state_deinit(struct install_state* state);
void install_state_fill_lookup_table(const struct install_state* state,
                                     struct binindex* binindex,
                                     struct binpkg** installed);
const struct binpkg* install_state_get_pkg(const struct install_state* state,
                                           const mmstr* name);
void install_state_add_pkg(struct install_state* state,
                           const struct binpkg* pkg);
void install_state_rm_pkgname(struct install_state* state,
                              const mmstr* pkgname);
void install_state_save_to_index(struct install_state* state, FILE* fp);

#endif /* PACKAGE_UTILS_H */
