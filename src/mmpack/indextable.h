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
struct it_entry* indextable_lookup_create(struct indextable* table,
                                          const mmstr* key);
struct it_entry* indextable_lookup_create_default(struct indextable* table,
                                                  const mmstr* key,
                                                  struct it_entry defval);
struct it_entry* indextable_lookup(const struct indextable* table,
                                   const mmstr* key);

struct it_entry* it_iter_first(struct it_iterator* iter,
                               struct indextable const * table);
struct it_entry* it_iter_next(struct it_iterator* iter);


#endif /* ifndef INDEXTABLE_H */
