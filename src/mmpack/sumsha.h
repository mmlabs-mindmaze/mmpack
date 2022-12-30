/*
 * @mindmaze_header@
 */

#ifndef SUMSHA_H
#define SUMSHA_H

#include "binpkg.h"
#include "indextable.h"
#include "strlist.h"


struct sumsha {
	struct indextable idx;
};


struct sumsha_entry {
	struct typed_hash hash;
	struct mmstring path;
};


struct sumsha_iterator {
	struct it_iterator idx_iter;
};


static inline
struct sumsha_entry* ssha_it_first(struct sumsha_iterator* iter,
                                   const struct sumsha* sumsha)
{
	struct it_entry* e;

	e = it_iter_first(&iter->idx_iter, &sumsha->idx);
	return e ? e->value : NULL;
}


static inline
struct sumsha_entry* ssha_it_next(struct sumsha_iterator* iter)
{
	struct it_entry* e;

	e = it_iter_next(&iter->idx_iter);
	return e ? e->value : NULL;
}


mmstr* sha256sums_path(const struct binpkg* pkg);
int sumsha_init(struct sumsha* sumsha);
int sumsha_load(struct sumsha* sumsha, const char* sumsha_path);
void sumsha_deinit(struct sumsha* sumsha);
int read_sumsha_filelist(const char* sumsha_path, struct strlist* filelist);


#endif /* SUMSHA_H */
