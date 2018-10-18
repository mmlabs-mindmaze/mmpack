/*
 * @mindmaze_header@
 */
#ifndef INDEXTABLE_H
#define INDEXTABLE_H

#include <stdint.h>
#include "mmstring.h"

struct it_bucket;

/**
 * struct it_entry: structure for data retrieval in index table
 * @key:        mmstr pointer holding the key
 * @value:      value used as pointer
 * @ivalue:     value used as signed integer
 *
 * This struct 
 */
struct it_entry {
	const mmstr* key;
	union {
		void* value;
		intptr_t ivalue;
	};
};

struct it_iterator {
	struct indextable const * table;
	struct it_bucket* curr;
	int b;
	int e;
};


/**
 * struct indextable: index table of key/value pairs
 * @buckets:            array of buckets
 * @num_buckets:        number of bucket list
 * @buckets_len_max:    length of @buckets
 * @first_unused_bucket: index of the first bucket node that can be used to
 *                       resize a bucket list
 *
 * This data structure is meant to be embedded in more complex structure and
 * provides the internal data for fast key lookup. The allocation of key and
 * value should be performed by the using code.
 */
struct indextable {
	struct it_bucket* buckets;
	int num_buckets;
	int buckets_len_max;
	int first_unused_bucket;
};

int indextable_init(struct indextable* table, int capacity, int num_extra);
int indextable_copy(struct indextable* restrict table,
                    const struct indextable* restrict src);
void indextable_deinit(struct indextable* table);
int indextable_double_size(struct indextable* table);
struct it_entry* indextable_insert(struct indextable* table, const mmstr* key);
int indextable_remove(struct indextable* table, const mmstr* key);
struct it_entry* indextable_lookup_create(struct indextable* table, const mmstr* key);
struct it_entry* indextable_lookup(const struct indextable* table, const mmstr* key);

struct it_entry* it_iter_first(struct it_iterator* iter, struct indextable const * table);
struct it_entry* it_iter_next(struct it_iterator* iter);


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
};

struct strset_iterator {
	struct it_iterator idx_iter;
};


static inline
int strset_init(struct strset* set, enum strset_mgmt mem_handling)
{
	set->mem_handling = mem_handling;

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
}


static inline
int strset_add(struct strset* set, const mmstr* str)
{
	struct it_entry* entry;
	mmstr* key;

	entry = indextable_lookup_create(&set->idx, str);
	if (!entry)
		return -1;

	if (entry->value)
		return 0;

	// Copy string if the set must manage the string memory
	if (set->mem_handling == STRSET_HANDLE_STRINGS_MEM)
		key = mmstrdup(str);
	else
		key = (mmstr*)str;

	entry->key = key;
	entry->value = key;
	return 0;
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


#endif
