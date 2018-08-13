/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <mmlib.h>

#include "indextable.h"
#include "mmstring.h"

/**
 * DOC: index table
 *
 * The index table is implemented through an hash table with separate
 * chaining but whose length of chain are kept small (<= 3 entries) most of
 * the time. When the chain is effectively small, the whole data used for
 * lookup will fit in a cache line, thus the lookup will be fast and the
 * impact of collision will be negligeable. If a chain contains more than 3
 * element, walking the list of entries will require accessing a new
 * cacheline per each group of 3 entries in the list.
 *
 * When the table internals are allocated, a number of bucket is reserved
 * (a power of 2). Each bucket can accomodate a chaining list of entries
 * with at least 3 entries.  However an extra amount of entries node are
 * allocated and made avaible for biggest chain to accomodate an extra
 * number of element. The "long" chain (of more than 3 elements) stay
 * nevertheless the minority. When all extra entries are recruited for a
 * big chain, the index table can be resized (double its size) if a new
 * element cannot be added.
 *
 * In effect this implementation allows a very fast key lookup. The most
 * expensive operation is the table resize which can happens if the table's
 * initial capacity has been underestimated.
 */


/* Compute the next power of 2 from preprocessor constants.
 * This is useful for deriving constant at compile time. Do not use it for
 * runtime since there are much more performant alternative in this case.
 */
#define POW2_CEIL(v) (1 + \
(((((((((v) - 1) | (((v) - 1) >> 0x10) | \
      (((v) - 1) | (((v) - 1) >> 0x10) >> 0x08)) | \
     ((((v) - 1) | (((v) - 1) >> 0x10) | \
      (((v) - 1) | (((v) - 1) >> 0x10) >> 0x08)) >> 0x04))) | \
   ((((((v) - 1) | (((v) - 1) >> 0x10) | \
      (((v) - 1) | (((v) - 1) >> 0x10) >> 0x08)) | \
     ((((v) - 1) | (((v) - 1) >> 0x10) | \
      (((v) - 1) | (((v) - 1) >> 0x10) >> 0x08)) >> 0x04))) >> 0x02))) | \
 ((((((((v) - 1) | (((v) - 1) >> 0x10) | \
      (((v) - 1) | (((v) - 1) >> 0x10) >> 0x08)) | \
     ((((v) - 1) | (((v) - 1) >> 0x10) | \
      (((v) - 1) | (((v) - 1) >> 0x10) >> 0x08)) >> 0x04))) | \
   ((((((v) - 1) | (((v) - 1) >> 0x10) | \
      (((v) - 1) | (((v) - 1) >> 0x10) >> 0x08)) | \
     ((((v) - 1) | (((v) - 1) >> 0x10) | \
      (((v) - 1) | (((v) - 1) >> 0x10) >> 0x08)) >> 0x04))) >> 0x02))) >> 0x01))))

#define DEFAULT_CAPACITY        500
#define BUCKET_SIZE             64 // size of cache line
#define MAX_PER_BUCKET          ((BUCKET_SIZE-sizeof(uint32_t))/(sizeof(uint32_t)+sizeof(struct it_entry)))
#define NUM_ENTRIES_NUMBITS     POW2_CEIL(MAX_PER_BUCKET)

/**
 * struct it_bucket: node of a bucket list
 * @hash:        array of hash of the keys of entries in the node
 * @num_entries: number of entries used in the node
 * @next_offset: offset to the next bucket node if not last (0 otherwise)
 * @entries:     array of entries contained in the node
 *
 * This structure is key in the implementation of the index table. It
 * represent a node of the entries (key-value) list that falls in a bucket.
 * A key value pair belong to the bucket if
 *    hash(key) % num_bucket ==  bucket_index
 * The bucket lists are kept small (few elements). Since individual bucket
 * node contains few elements (MAX_PER_BUCKET = 3), most of the time, the
 * bucket will consist of only one node. Since the bucket node structure
 * fits in one cache line and contains all the data needed, we can have
 * achieve fast lookup performance.
 */
struct it_bucket {
	uint32_t hash[MAX_PER_BUCKET];
	uint32_t num_entries:NUM_ENTRIES_NUMBITS;
	int32_t next_offset:32-NUM_ENTRIES_NUMBITS;
	struct it_entry entries[MAX_PER_BUCKET];
};


/**
 * compute_hash() - compute hash value of data
 * @data:       data array to hash
 * @len:        length of @data
 *
 * Dan Bernstein created this algorithm and posted it in a newsgroup. it has
 * been used successfully by many, but despite that the algorithm itself is not
 * very sound when it comes to avalanche and permutation of the internal state.
 * It has proven very good for small character keys, where it can outperform
 * algorithms that result in a more random distribution.
 *
 * Bernstein's hash should be used with caution. It performs very well in
 * practice, for no apparently known reasons (much like how the constant 33
 * does better than more logical constants for no apparent reason), but in
 * theory it is not up to snuff. Always test this function with sample data for
 * every application to ensure that it does not encounter a degenerate case and
 * cause excessive collisions.
 *
 * Return: a 32-bit hash value
 */
static
uint32_t compute_hash(const void* data, int len)
{
	int i;
	uint_fast32_t h;
	const unsigned char* d = data;

	h = 5381;
	for (i = 0; i < len; i++)
		h = h * 33 + d[i];

	return h;
}


static
struct it_bucket* indextable_get_buckethead(struct indextable* table, uint32_t hash)
{
	// num_bucket is power of 2, so hash % num == hash & (num - 1)
	return &table->buckets[hash & (table->num_buckets - 1)];
}


static
int bucket_lookup(struct it_bucket* bucket, uint32_t hash, const mmstr* key)
{
	struct it_entry* entry;
	int i;

	// Search in the entries of the node for a matching key
	for (i = 0; i < bucket->num_entries; i++) {
		entry = &bucket->entries[i];
		if (  (bucket->hash[i] == hash)
		   && mmstrequal(entry->key, key))
			return i;
	}

	return -1;
}


static
struct it_entry* bucketlist_lookup_entry(struct it_bucket* bucket,
                                         uint32_t hash, const mmstr* key)
{
	int index;

	while (1) {
		// Search in the entries of the node for a matching key
		index = bucket_lookup(bucket, hash, key);
		if (index >= 0)
			return &bucket->entries[index];

		// If this is the end of list, search has failed
		if (!bucket->next_offset)
			return NULL;

		// Go to the next node of the bucket list
		bucket += bucket->next_offset;
	}
}


/**
 * get_last_bucket() - go to the last bucket node of the list
 * @bucket:     any node of a bucket list
 *
 * Return: the last node of the bucket list
 */
static
struct it_bucket* get_last_bucket(struct it_bucket* bucket)
{
	while (bucket->next_offset)
		bucket += bucket->next_offset;

	return bucket;
}


/**
 * indextable_bucket_append_node() - add a new node to a bucket list
 * @table:      initialized index table
 * @bucket:     last node of a bucket list
 *
 * Return: the new bucket node in case of success, NULL if there is no
 * available bucket node left in the table (table will have to be grown)
 */
static
struct it_bucket* indextable_bucket_append_node(struct indextable* table,
                                                struct it_bucket* bucket)
{
	struct it_bucket *new_bucket;

	if (table->first_unused_bucket == table->buckets_len_max)
		return NULL;

	// Recruit a new bucket node for bucket list
	new_bucket = &table->buckets[table->first_unused_bucket];

	// Update the head of the available node list
	table->first_unused_bucket += new_bucket->next_offset;

	// Link new node to bucket list
	new_bucket->num_entries = 0;
	new_bucket->next_offset = 0;
	bucket->next_offset = new_bucket - bucket;

	return new_bucket;
}


/**
 * indextable_release_bucketnode() - return a node to the available list
 * @table:      initialized index table
 * @bucket:     node to return to available list
 */
static
void indextable_release_bucketnode(struct indextable* table,
                                   struct it_bucket* bucket)
{
	int index;

	index = bucket - table->buckets;
	bucket->next_offset = table->first_unused_bucket - index;
	table->first_unused_bucket = index;
}


/**
 * indextable_create_entry() - create a new entry for a hash
 * @table:      initialized index table
 * @hash:       hash of the key being added
 *
 * This function is the internal mechanism to "allocate" an new entry. This
 * does not check for the existence of an entry already associated to the
 * underlying key, so this must be checked beforehand if this is a
 * possibilty.
 *
 * Return: the created entry (fields are not initiliazed yet) in case of
 * success, NULL in case of allocation failure.
 */
static
struct it_entry* indextable_create_entry(struct indextable* table,
                                         uint32_t hash)
{
	struct it_bucket *bucket;
	int ind;

	bucket = indextable_get_buckethead(table, hash);
	bucket = get_last_bucket(bucket);

	// Add new bucket node if the last node is full
	if (bucket->num_entries == MAX_PER_BUCKET) {
		bucket = indextable_bucket_append_node(table, bucket);
		if (!bucket)
			return NULL;
	}

	// Create a new entry in the bucket node
	ind = bucket->num_entries++;
	bucket->hash[ind] = hash;

	return &bucket->entries[ind];
}


/**
 * indextable_alloc() - allocate internal memory of index table
 * @table:      index table being initialized
 *
 * This function is typically called during index table initialization or
 * when it grows.
 *
 * Return: 0 in case of success, -1 in case of allocation failure.
 */
static
int indextable_alloc(struct indextable* table)
{
	int nmax = table->buckets_len_max;
	int i;

	// Perform allocation (align on cacheline)
	table->buckets = mm_aligned_alloc(64, nmax*sizeof(*(table->buckets)));
	if (!table->buckets)
		return -1;

	// Initialize the heads of bucket lists as unused elements
	for (i = 0; i < table->num_buckets; i++) {
		table->buckets[i].num_entries = 0;
		table->buckets[i].next_offset = 0;
	}

	// Initialize the list of avaiable bucket node
	table->first_unused_bucket = table->num_buckets;
	for (i = table->first_unused_bucket; i < nmax; i++) {
		table->buckets[i].num_entries = 0;
		table->buckets[i].next_offset = 1;
	}

	return 0;
}


/**
 * indextable_double_size() - double the capacity of the index table
 * @table:      initialized index table
 *
 * Return: 0 in case of success, -1 in case of allocation failure.
 */
LOCAL_SYMBOL
int indextable_double_size(struct indextable* table)
{
	struct indextable new_table;
	int i, j;
	struct it_entry* restrict entry;
	struct it_bucket* restrict bucket;
	uint32_t hash;

	// Alloc a new indextable with the double of capacity of the old one
	new_table.buckets_len_max = table->buckets_len_max * 2;
	new_table.num_buckets = table->num_buckets * 2;
	if (indextable_alloc(&new_table))
		return -1;

	for (i = 0; i < table->num_buckets; i++) {
		// Walk along the whole bucket list of the old indextable to add
		// the entries in the resized indextable
		bucket = &table->buckets[i];
		while (1) {
			// copy each entry of the node into the new indextable
			for (j = 0; j < bucket->num_entries; j++) {
				hash = bucket->hash[j];
				entry = indextable_create_entry(&new_table, hash);
				*entry = bucket->entries[j];
			}

			// check this is not the end of node list
			if (!bucket->next_offset)
				break;

			// Move to the next node in the bucket list
			bucket += bucket->next_offset;
		}
	}

	// We can now replace the internals of the old indextable with the
	// internals of the new one
	mm_aligned_free(table->buckets);
	*table = new_table;

	return 0;
}


/**
 * indextable_init() - initialize internals of index table
 * @table:      pointer to an index table structure
 * @capacity:   initial capacity in the terms of entries for which the hash
 *              table must be initialized. If negative or zero, the table
 *              will be sized with a default capacity.
 * @num_extra:  number of entries that must be reserved for long chain. If
 *              negative, this will reserve the equivalent of 12% of bucket
 *              nodes for long chains.
 *
 * This function allocates and initializes the internals of an index table.
 * @capacity should set close to the number of elements that the table must
 * hold. This does not need to be accurate since the table will
 * automatically grow if too small, but having a good guess will avoid
 * successive data reallocation.
 *
 * Return: 0 in case of success, -1 if allocation failed
 */
LOCAL_SYMBOL
int indextable_init(struct indextable* table, int capacity, int num_extra)
{
	int num_extra_bucket;

	if (capacity <= 0)
		capacity = DEFAULT_CAPACITY;

	// Adjust the number of buckets to be the biggest power of 2 smaller
	// than the capacity (starting with 16 buckets minimum)
	table->num_buckets = 16;
	while (capacity > 2*table->num_buckets)
		table->num_buckets *= 2;

	// If num_extra is unspecified, reserved 12.5% extra bucket node
	if (num_extra < 0)
		num_extra = (table->num_buckets * MAX_PER_BUCKET) / 8;

	// Size bucket nodes array with some extra to accomodate some
	// exceptional variability in bucket list length
	num_extra_bucket = (num_extra + MAX_PER_BUCKET-1) / MAX_PER_BUCKET;
	table->buckets_len_max = table->num_buckets + num_extra_bucket;
	if (indextable_alloc(table)) {
		indextable_deinit(table);
		return -1;
	}

	return 0;
}


/**
 * indextable_deinit() - free up internals of index table
 * @table:         initialized index table
 */
LOCAL_SYMBOL
void indextable_deinit(struct indextable* table)
{
	mm_aligned_free(table->buckets);

	*table = (struct indextable){.buckets = NULL};
}


/**
 * indextable_lookup_create() - search for an entry and create if none
 * @table:      initialized index table structure
 * @key:        string of the key
 *
 * This creates a new entry associated to @key if it was not existing yet
 * in the table @table. Otherwise, if already present, no new entry is created
 * and the existing one is returned. If a new entry is created, its field
 * value is ensured to be initialized to NULL.
 *
 * Return: pointer to the new entry if a new one is created, or pointer to the
 * existing one. In case of memory allocation problem, NULL is returned. The
 * pointer to the entry is valid until the table grows, so for safety, until
 * the next call to indextable_lookup_create() or indextable_insert().
 */
LOCAL_SYMBOL
struct it_entry* indextable_lookup_create(struct indextable* table,
                                          const mmstr* key)
{
	struct it_bucket *bucket;
	struct it_entry* entry;
	uint32_t hash;

	hash = compute_hash(key, mmstrlen(key));

	// Check for an entry with same key first
	bucket = indextable_get_buckethead(table, hash);
	entry = bucketlist_lookup_entry(bucket, hash, key);
	if (entry)
		return entry;

	// Add entry
	entry = indextable_create_entry(table, hash);
	if (!entry) {
		if (indextable_double_size(table))
			return NULL;

		entry = indextable_create_entry(table, hash);
	}

	// Initialize it since it is a new entry
	entry->key = key;
	entry->value = NULL;

	return entry;
}


/**
 * indextable_insert() - Add an entry to an index table
 * @table:      initialized index table structure
 * @key:        string of the key
 *
 * This creates a new entry associated to @key.
 *
 * NOTE: There is no check that the same key has not been already inserted in
 * the table. If you cannot promise that you will not add a duplicate, you must
 * use indextable_lookup_create() instead.
 *
 * Return: pointer to the new entry. In case of memory allocation problem, NULL
 * is returned. The pointer to the entry is valid until the table grows, so for
 * safety, until the next call to indextable_lookup_create(),
 * indextable_insert() or indextable_remove().
 */
LOCAL_SYMBOL
struct it_entry* indextable_insert(struct indextable* table,
                                          const mmstr* key)
{
	struct it_entry* entry;
	uint32_t hash;

	hash = compute_hash(key, mmstrlen(key));
	entry = indextable_create_entry(table, hash);
	if (!entry) {
		if (indextable_double_size(table))
			return NULL;

		entry = indextable_create_entry(table, hash);
	}

	// Initialize it since it is a new entry
	entry->key = key;
	entry->value = NULL;

	return entry;
}


/**
 * indextable_remove() - Remove an entry from an index table
 * @table:      initialized index table structure
 * @key:        string of the key
 *
 * Return: 0 in case of success, -1 if the key has not been found in the
 * table.
 */
LOCAL_SYMBOL
int indextable_remove(struct indextable* table, const mmstr* key)
{
	struct it_bucket *bucket, *last, *prev_last;
	uint32_t hash;
	int index, lastelt_index;

	hash = compute_hash(key, mmstrlen(key));
	bucket = indextable_get_buckethead(table, hash);
	prev_last = NULL;

	// Find the bucket and index of the key
	while (1) {
		index = bucket_lookup(bucket, hash, key);
		if (index >= 0)
			break;

		if (!bucket->next_offset)
			return -1;

		prev_last = bucket;
		bucket += bucket->next_offset;
	}

	// Find the last bucket node of the bucket list
	last = bucket;
	while (last->next_offset) {
		prev_last = last;
		last += last->next_offset;
	}

	// Move the last entry of the bucket to the place of the one that
	// has been removed
	lastelt_index = last->num_entries--;
	bucket->entries[index] = last->entries[lastelt_index];
	bucket->hash[index] = last->hash[lastelt_index];

	// Unlink the bucket node if there are no more entries and is not
	// bucket list head
	if ((last->num_entries == 0) && (prev_last != NULL)) {
		prev_last->next_offset = 0;
		indextable_release_bucketnode(table, last);
	}

	return 0;
}


/**
 * indextable_lookup() - search for an entry in a index table
 * @table:      initialized index table structure
 * @key:        string of the key
 *
 * Return: the pointer to the entry if the an entry associated to @key can
 * be found in the table, NULL if no entry can be found. The pointer to the
 * entry is valid until the table grows, so for safety, until the next call
 * to indextable_lookup_create(), indextable_insert() or
 * indextable_remove().
 */
LOCAL_SYMBOL
struct it_entry* indextable_lookup(struct indextable* table, const mmstr* key)
{
	struct it_bucket* bucket;
	uint32_t hash;

	hash = compute_hash(key, mmstrlen(key));
	bucket = indextable_get_buckethead(table, hash);

	return bucketlist_lookup_entry(bucket, hash, key);
}


/**
 * it_iter_first() - initialize iterator of index table and return first element
 * @iter:       pointer to an iterator structure
 * @table:      initialized index table
 *
 * Return: the pointer to first entry if the table is not empty, NULL
 * otherwise.
 */
LOCAL_SYMBOL
struct it_entry* it_iter_first(struct it_iterator* iter, struct indextable const * table)
{
	*iter = (struct it_iterator) {.table = table, .e = -1, .b = -1};
	return it_iter_next(iter);
}


/**
 * it_iter_first() - get next element in the iteration of index table
 * @iter:       pointer to an initialized iterator structure
 *
 * Return: the pointer to next entry if this is not last of iteration, NULL
 * otherwise.
 */
LOCAL_SYMBOL
struct it_entry* it_iter_next(struct it_iterator* iter)
{
	struct indextable const * table = iter->table;
	struct it_bucket* curr = iter->curr;
	struct it_bucket* buckets;
	int b;

	if (iter->e < 0) {
		if (!curr || curr->next_offset == 0) {
			// Get next non-empty bucket head
			buckets = table->buckets;
			b = iter->b;
			do {
				if (++b >= table->num_buckets)
					return NULL;
			} while (buckets[b].num_entries == 0);

			iter->b = b;
			curr = buckets + b;
		} else {
			curr += curr->next_offset;
		}
		iter->curr = curr;
		iter->e = curr->num_entries-1;
	}

	return &curr->entries[iter->e--];
}
