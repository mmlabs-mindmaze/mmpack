/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <check.h>

#include "binary-index.h"
#include "context.h"
#include "package-utils.h"
#include "testcases.h"

#define TEST_BININDEX_DIR SRCDIR"/tests/binary-indexes"

static struct mmpack_ctx ctx;

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
	struct mmpack_opts opts = {.prefix = NULL };
	mmpack_ctx_init(&ctx, &opts);
}

static
void ctx_teardown(void)
{
	mmpack_ctx_deinit(&ctx);
}

START_TEST(test_binindex_parsing)
{
	int rv;

	rv = binary_index_populate(&ctx, binindexes[_i]);
	ck_assert(rv == 0);
	binary_index_dump(&ctx.binindex);
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
