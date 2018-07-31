/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <check.h>
#include <stdlib.h>
#include <mmpredefs.h>

#include "indextable.h"
#include "testcases.h"

#define KEY_NMAX        65000
#define KEYLEN_MIN      8
#define KEYLEN_MAX      64

struct keyval {
	union {
		void* ptr;
		long lval;
	};
	const mmstr* key;
};


/**
 * randint() - generate a rendom number within limits
 * @min:        minimal value that can be returned
 * @max:        maximal value that can be returned
 *
 * Return: a integer between min and max inclusive
 */
static
int randint(int min, int max)
{
	return min + random() % (max - min + 1);
}


/**
 * randperm() - generate a random permutation of an array
 * @perms:      buffer that will receive the permutations
 * @len:        length of @buffer
 */
static
void randperm(int* perms, int len)
{
	int i, p;

	for (i = 0; i < len; i++) {
		p = randint(0, i);
		perms[i] = perms[p];
		perms[p] = i;
	}
}


/**************************************************************************
 *                                                                        *
 *                    setup and cleanup test environment                  *
 *                                                                        *
 **************************************************************************/

static char* string_data;
static struct keyval* keyvals;

#define CHARSET         "-.0000111112223456789" \
                        "aaabbbbbcdeeeeeeeeeeefghiiiiijklllllmnopqrstuvwxyz"
#define CHARSET_LEN     (sizeof(CHARSET)-1)


static
void fill_random_key(mmstr* str)
{
	static const char charset[CHARSET_LEN] = CHARSET;
	int i, len;

	len = mmstr_maxlen(str);
	for (i = 0; i < len; i++)
		str[i] = charset[randint(0, CHARSET_LEN-1)];

	mmstr_setlen(str, len);
}


static
void testdata_setup(void)
{
	int i, keylen;
	char* strdata;
	mmstr* key;

	// Allocate the big data string and the array of key-value pairs
	string_data = malloc(KEY_NMAX * MMSTR_NEEDED_SIZE(KEYLEN_MAX));
	keyvals = malloc(sizeof(*keyvals)*KEY_NMAX);

	// Generate random pair of key-value. Each key is a different
	// piece of the big random string
	strdata = string_data;
	for (i = 0; i < KEY_NMAX; i++) {
		keylen = randint(KEYLEN_MIN, KEYLEN_MAX);
		key = mmstr_init(strdata, keylen);
		fill_random_key(key);

		keyvals[i].lval = random();
		keyvals[i].key = key;

		strdata += MMSTR_NEEDED_SIZE(keylen);
	}
}


static
void testdata_cleanup(void)
{
	free(string_data);
	free(keyvals);
	string_data = NULL;
	keyvals = NULL;
}


/**************************************************************************
 *                                                                        *
 *                        indextable test functions                       *
 *                                                                        *
 **************************************************************************/
struct {
	int num_buckets;
	int num_entries;
} indextable_cases[] = {
	{.num_buckets = 3, .num_entries = 10},
	{.num_buckets = 8, .num_entries = 10},
	{.num_buckets = 12, .num_entries = 10},
	{.num_buckets = 32, .num_entries = 10},
	{.num_buckets = 32, .num_entries = 32},
	{.num_buckets = 32, .num_entries = 100},
	{.num_buckets = 64, .num_entries = 10},
	{.num_buckets = 64, .num_entries = 100},
	{.num_buckets = 64, .num_entries = 1000},
	{.num_buckets = -1, .num_entries = KEY_NMAX-10},
	{.num_buckets = 64, .num_entries = KEY_NMAX-10},
	{.num_buckets = 256, .num_entries = KEY_NMAX-10},
	{.num_buckets = KEY_NMAX-10, .num_entries = KEY_NMAX-10},
};
#define NUM_HASHTABLE_CASES     MM_NELEM(indextable_cases)


START_TEST(empty_table)
{
	struct indextable table;
	int num_buckets = indextable_cases[_i].num_buckets;
	struct it_entry* entry;
	int i;

	ck_assert(indextable_init(&table, num_buckets, -1) != -1);

	for (i = 0; i < 10; i++) {
		entry = indextable_lookup(&table, keyvals[i].key);
		ck_assert(entry == NULL);
	}

	indextable_deinit(&table);
}
END_TEST


START_TEST(populate_table)
{
	struct indextable table;
	int num_buckets = indextable_cases[_i].num_buckets;
	int num_entries = indextable_cases[_i].num_entries;
	void *exp_val;
	int i, j;
	const mmstr* key;
	struct it_entry* entry;
	int perms[KEY_NMAX];

	randperm(perms, num_entries);
	ck_assert(indextable_init(&table, num_buckets, -1) != -1);

	for (i = 0; i < num_entries; i++) {
		key = keyvals[i].key;
		exp_val = keyvals[i].ptr;

		entry = indextable_lookup_create(&table, key);
		ck_assert(entry != NULL);
		ck_assert(entry->value == NULL);

		entry->value = exp_val;
	}

	for (i = 0; i < num_entries; i++) {
		j = perms[i];
		key = keyvals[j].key;
		exp_val = keyvals[j].ptr;

		entry = indextable_lookup(&table, key);
		ck_assert_msg(entry != NULL,
		              "entry not found for %i (perms[%i])", j, i);
		ck_assert_msg(entry->value == exp_val,
		              "values mismatch for %i (perms[%i])", j, i);
	}

	indextable_deinit(&table);
}
END_TEST


START_TEST(walk_and_remove)
{
	struct indextable idx;
	struct it_iterator iter;
	struct it_entry* entry;
	mmstr* keys[3];
	int vals[3] = {5, 7, 11};
	mmstr* key_to_remove;
	int* ptr;
	int i;

	keys[0] = mmstr_alloca_from_cstr("first key");
	keys[1] = mmstr_alloca_from_cstr("another key");
	keys[2] = mmstr_alloca_from_cstr("this is the last");

	// Init and populate table
	ck_assert(indextable_init(&idx, -1, -1) != -1);
	for (i = 0; i < MM_NELEM(keys); i++)
		indextable_insert(&idx, keys[i])->value = &vals[i];

	key_to_remove = mmstr_alloca_from_cstr("another key");
	indextable_remove(&idx, key_to_remove);

	// Multiply by 3 pointed value of all entries remaining in table
	entry= it_iter_first(&iter, &idx);
	while (entry != NULL) {
		ptr = entry->value;
		*ptr *= 3;
		entry = it_iter_next(&iter);
	}

	ck_assert_int_eq(vals[0], 5*3);
	ck_assert_int_eq(vals[1], 7);
	ck_assert_int_eq(vals[2], 11*3);

	indextable_deinit(&idx);
}
END_TEST


/**************************************************************************
 *                                                                        *
 *                         test suite creation                            *
 *                                                                        *
 **************************************************************************/
TCase* create_indextable_tcase(void)
{
	TCase * tc;

	tc = tcase_create("indextable");
	tcase_add_unchecked_fixture(tc, testdata_setup, testdata_cleanup);

	tcase_add_loop_test(tc, empty_table, 0, NUM_HASHTABLE_CASES);
	tcase_add_loop_test(tc, populate_table, 0, NUM_HASHTABLE_CASES);
	tcase_add_test(tc, walk_and_remove);

	return tc;
}
