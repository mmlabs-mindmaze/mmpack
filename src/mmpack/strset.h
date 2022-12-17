/*
 * @mindmaze_header@
 */
#ifndef STRSET_H
#define STRSET_H


#include "indextable.h"


/**************************************************************************
 *                                                                        *
 *                          table of unique strings                       *
 *                                                                        *
 **************************************************************************/

enum strset_mgmt {
	STRSET_FOREIGN_STRINGS,
	STRSET_HANDLE_STRINGS_MEM,
};

struct strset {
	struct indextable idx;
	enum strset_mgmt mem_handling;
	int num_item;
};

struct strset_iterator {
	struct it_iterator idx_iter;
};


static inline
int strset_init(struct strset* set, enum strset_mgmt mem_handling)
{
	set->mem_handling = mem_handling;
	set->num_item = 0;

	// Use an non default initial size of the indextable because the
	// the default indextable size (512) is way too big for the typical
	// use of struct strset which is meant to list system dependencies
	// (or maybe file list). A handful number of element should suffice
	// in most cases.
	return indextable_init(&set->idx, 10, -1);
}


static inline
void strset_deinit(struct strset* set)
{
	struct it_entry* entry;
	struct it_iterator iter;

	if (set->mem_handling == STRSET_HANDLE_STRINGS_MEM) {
		entry = it_iter_first(&iter, &set->idx);
		while (entry) {
			mmstr_free(entry->value);
			entry = it_iter_next(&iter);
		}
	}

	indextable_deinit(&set->idx);
	set->num_item = 0;
}


static inline
int strset_add(struct strset* set, const mmstr* str)
{
	struct it_entry* entry;
	mmstr* key;

	entry = indextable_lookup_create(&set->idx, str);

	if (entry->value)
		return 0;

	// Copy string if the set must manage the string memory
	if (set->mem_handling == STRSET_HANDLE_STRINGS_MEM)
		key = mmstrdup(str);
	else
		key = (mmstr*)str;

	entry->key = key;
	entry->value = key;
	set->num_item++;
	return 1;
}


static inline
int strset_remove(struct strset* set, const mmstr* str)
{
	struct it_entry * to_remove;
	mmstr * string = NULL;
	int rv;

	if (set->mem_handling == STRSET_HANDLE_STRINGS_MEM) {
		to_remove = indextable_lookup(&set->idx, str);
		if (to_remove)
			string = to_remove->value;
	}

	rv = indextable_remove(&set->idx, str);
	mmstr_free(string);
	if (rv == 0)
		set->num_item--;

	return rv;
}


static inline
int strset_contains(const struct strset* set, const mmstr* str)
{
	return (indextable_lookup(&set->idx, str) != NULL);
}


static inline
mmstr* strset_iter_first(struct strset_iterator* iter, const struct strset* set)
{
	struct it_entry* entry;

	entry = it_iter_first(&iter->idx_iter, &set->idx);
	if (!entry)
		return NULL;

	return entry->value;
}


static inline
mmstr* strset_iter_next(struct strset_iterator* iter)
{
	struct it_entry* entry;

	entry = it_iter_next(&iter->idx_iter);
	if (!entry)
		return NULL;

	return entry->value;
}


#endif /* ifndef STRSET_H */
