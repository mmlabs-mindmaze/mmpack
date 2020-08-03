/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <check.h>

#include "srcindex.h"
#include "testcases.h"

#define TEST_SRCINDEX_DIR SRCDIR"/tests/source-indexes"

static struct srcindex source_index;

static const char * srcindexes[] = {
	TEST_SRCINDEX_DIR"/simple",
};
#define NUM_SRCINDEXES MM_NELEM(srcindexes)

static
void ctx_setup(void)
{
	srcindex_init(&source_index);
}

static
void ctx_teardown(void)
{
	srcindex_deinit(&source_index);
}

START_TEST(test_srcindex_parsing)
{
	int rv;
	struct repolist_elt repo;

	repo.url = mmstr_malloca_from_cstr("fake://url/ressource");
	repo.name = mmstr_malloca_from_cstr("fake_url");

	rv = srcindex_populate(&source_index, srcindexes[_i], &repo);
	ck_assert(rv == 0);
}
END_TEST


TCase* create_srcindex_tcase(void)
{
	TCase * tc;

	tc = tcase_create("srcindex");
	tcase_add_checked_fixture(tc, ctx_setup, ctx_teardown);

	tcase_add_loop_test(tc, test_srcindex_parsing, 0, NUM_SRCINDEXES);

	return tc;
}
