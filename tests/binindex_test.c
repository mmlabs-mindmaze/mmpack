/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <check.h>

#include "package-utils.h"
#include "testcases.h"

#define TEST_BININDEX_DIR SRCDIR"/tests/binary-indexes"

static struct binindex binary_index;

static const char * binindexes[] = {
	TEST_BININDEX_DIR"/simplest.yaml",
	TEST_BININDEX_DIR"/simple.yaml",
	TEST_BININDEX_DIR"/circular.yaml",
	TEST_BININDEX_DIR"/dependency-issue.yaml",
};
#define NUM_BININDEXES MM_NELEM(binindexes)

static
void ctx_setup(void)
{
	binindex_init(&binary_index);
}

static
void ctx_teardown(void)
{
	binindex_deinit(&binary_index);
}

START_TEST(test_binindex_parsing)
{
	int rv;

	rv = binindex_populate(&binary_index, binindexes[_i], NULL);
	ck_assert(rv == 0);
	binindex_dump(&binary_index);
}
END_TEST

TCase* create_binindex_tcase(void)
{
    TCase * tc;

    tc = tcase_create("binindex");
    tcase_add_checked_fixture(tc, ctx_setup, ctx_teardown);

    tcase_add_loop_test(tc, test_binindex_parsing, 0, NUM_BININDEXES);

    return tc;
}
