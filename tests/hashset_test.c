/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <check.h>
#include <mmsysio.h>
#include <mmlib.h>
#include <stdalign.h>

#include "crypto.h"
#include "hashset.h"
#include "testcases.h"
#include "utils.h"
#include "xx-alloc.h"


#define HASHSET_DIR    TESTSDIR"/hashset"

#define NUM_IN_LIST	4096
#define NUM_OUT_LIST	1000
static digest_t* in_list;
static digest_t* out_list;


// Use a linear congruential generator directly instead of random generator
// provided in stdlib: provided generator are not equal across platforms. 
static
uint32_t lcg(void)
{
	static uint64_t val = 0;
	val = (0x5DEECE66D * val + 11) & 0x0000FFFFFFFFFFFF;
	return (uint32_t)val;
}


static
void gen_random_digest(digest_t* digest)
{
	int i;
	uint32_t* u32 = (uint32_t*)digest;

	for (i = 0; i < sizeof(*digest)/sizeof(*u32); i++)
		u32[i] = lcg();
}


static
int is_in_list(digest_t* digest)
{
	int i;

	for (i = 0; i < NUM_IN_LIST; i++)
		if (digest_equal(digest, in_list + i))
			return 1;

	return 0;
}


static
void setup(void)
{
	int i;

	in_list = xx_mm_aligned_alloc(alignof(*in_list),
	                              NUM_IN_LIST*sizeof(*in_list));
	out_list = xx_mm_aligned_alloc(alignof(*out_list),
	                               NUM_OUT_LIST*sizeof(*out_list));

	for (i = 0; i < NUM_IN_LIST; i++)
		gen_random_digest(in_list + i);

	for (i = 0; i < NUM_OUT_LIST; i++) {
		do {
			gen_random_digest(out_list + i);
		} while (is_in_list(out_list + i));
	}

	mm_mkdir(HASHSET_DIR, 0777, MM_RECURSIVE);
}


static
void cleanup(void)
{
	mm_aligned_free(in_list);
	mm_aligned_free(out_list);
	mm_remove(HASHSET_DIR, MM_RECURSIVE|MM_DT_ANY);
}

/**************************************************************************
 *                                                                        *
 *                         hashset test functions                         *
 *                                                                        *
 **************************************************************************/
static const int contains_cases[] = {
	10, 127, 128, 300, 1024, 1025, NUM_IN_LIST
};

START_TEST(contains)
{
	mmstr* path = mmstr_asprintf(NULL, HASHSET_DIR "/%i.hashset", _i);
	struct hashset hashset;
	int num = contains_cases[_i];
	int i;

	ck_assert(create_hashset(path, num, in_list) == 0);

	ck_assert(hashset_init_from_file(&hashset, path) == 0);

	for (i = 0; i < num; i++)
		ck_assert(hashset_contains(&hashset, in_list + i) == 1);

	for (i = 0; i < NUM_OUT_LIST; i++)
		ck_assert(hashset_contains(&hashset, out_list + i) == 0);

	hashset_deinit(&hashset);
	mmstr_free(path);
}
END_TEST


/**************************************************************************
 *                                                                        *
 *                         test suite creation                            *
 *                                                                        *
 **************************************************************************/
TCase* create_hashset_tcase(void)
{
	TCase * tc;

	tc = tcase_create("hashset");
	tcase_add_unchecked_fixture(tc, setup, cleanup);

	tcase_add_loop_test(tc, contains, 0, MM_NELEM(contains_cases));

	return tc;
}
