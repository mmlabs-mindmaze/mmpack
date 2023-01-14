/*
 * @mindmaze_header@
 */

#ifndef SUMSHA_H
#define SUMSHA_H

#include "binpkg.h"
#include "indextable.h"
#include "mmstring.h"
#include "strlist.h"


struct sumsha {
	struct indextable idx;
};


struct sumsha_entry {
	struct typed_hash hash;
	int index;
	struct mmstring path;
};


struct sumsha_iterator {
	struct it_iterator idx_iter;
};


/**
 * sumsha_first() - initialize a sumsha iterator and get first element
 * @iter:       pointer to sumsha_iterator to use for the iteration
 * @sumsha:     sumsha table to iterate.
 *
 * Return: pointer to first sumsha_entry element, NULL if the table is empty.
 */
static inline
struct sumsha_entry* sumsha_first(struct sumsha_iterator* iter,
                                  const struct sumsha* sumsha)
{
	struct it_entry* e;

	e = it_iter_first(&iter->idx_iter, &sumsha->idx);
	return e ? e->value : NULL;
}


/**
 * sumsha_next() - get next sumsha element of iteration
 * @iter:       pointer to initialized sumsha_iterator
 *
 * Return: pointer to next sumsha_entry element, NULL if end of table.
 */
static inline
struct sumsha_entry* sumsha_next(struct sumsha_iterator* iter)
{
	struct it_entry* e;

	e = it_iter_next(&iter->idx_iter);
	return e ? e->value : NULL;
}


/**
 * sumsha_get() - lookup typed hash from path in sumsha
 * @sumsha:     sumsha table to search
 * @path:       path of the entry in the sumsha to lookup
 *
 * Return: pointer to struct typed_hash mapped to @path if found in @sumsha,
 * NULL otherwise.
 */
static inline
struct typed_hash* sumsha_get(const struct sumsha* sumsha, const mmstr* path)
{
	struct it_entry* entry = indextable_lookup(&sumsha->idx, path);
	return entry ? &((struct sumsha_entry*)entry->value)->hash : NULL;
}


mmstr* sha256sums_path(const mmstr* rootpath, const struct binpkg* pkg);
void sumsha_init(struct sumsha* sumsha);
int sumsha_load(struct sumsha* sumsha, const char* sumsha_path);
void sumsha_deinit(struct sumsha* sumsha);
int read_sumsha_filelist(const char* sumsha_path, struct strlist* filelist);


#endif /* SUMSHA_H */
