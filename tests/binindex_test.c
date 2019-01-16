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
#define NUM_PKGS_IN_SIMPLE_YAML 6

static struct binindex binary_index;

static const char * binindexes[] = {
	TEST_BININDEX_DIR"/simplest.yaml",
	TEST_BININDEX_DIR"/simple.yaml",
	TEST_BININDEX_DIR"/circular.yaml",
	TEST_BININDEX_DIR"/complex-dependency.yaml",
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

	rv = binindex_populate(&binary_index, binindexes[_i], 0);
	ck_assert(rv == 0);
	binindex_dump(&binary_index);
}
END_TEST


static
int check_pkg_fully_set(struct mmpkg* pkg, void * data)
{
	int* count = data;

	(*count)++;
	ck_assert(pkg->size != 0);
	ck_assert(pkg->sha256 != NULL);
	ck_assert(pkg->filename != NULL);
	return 0;
}



START_TEST(test_deduplicate)
{
	int rv;
	int count;

	rv = binindex_populate(&binary_index, TEST_BININDEX_DIR"/installed-simple.yaml", -1);
	ck_assert_msg(rv == 0, "Installed list loading failed");
	rv = binindex_populate(&binary_index, TEST_BININDEX_DIR"/simple.yaml", 0);
	ck_assert_msg(rv == 0, "repository list loading failed");

	// Check eack package has its repo specific fields set and count the
	// number of package
	count = 0;
	binindex_foreach(&binary_index, check_pkg_fully_set, &count);

	ck_assert_int_eq(count, NUM_PKGS_IN_SIMPLE_YAML);
}
END_TEST


TCase* create_binindex_tcase(void)
{
    TCase * tc;

    tc = tcase_create("binindex");
    tcase_add_checked_fixture(tc, ctx_setup, ctx_teardown);

    tcase_add_loop_test(tc, test_binindex_parsing, 0, NUM_BININDEXES);
    tcase_add_test(tc, test_deduplicate);

    return tc;
}
