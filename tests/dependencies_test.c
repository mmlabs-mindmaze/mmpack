/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <check.h>

#include "binary-index.h"
#include "mmpack-context.h"
#include "package-utils.h"
#include "testcases.h"

#define TEST_BININDEX_DIR SRCDIR"/binary-indexes"

static struct mmpack_ctx ctx;

static const char * valid_binindexes[] = {
	TEST_BININDEX_DIR"/simplest.yaml",
	TEST_BININDEX_DIR"/simple.yaml",
	TEST_BININDEX_DIR"/circular.yaml",
};
#define NUM_VALID_BININDEXES MM_NELEM(valid_binindexes)

static const char * invalid_binindexes[] = {
	TEST_BININDEX_DIR"/dependency-issue.yaml",
};
#define NUM_INVALID_BININDEXES MM_NELEM(invalid_binindexes)

static
void ctx_setup(void)
{
	mmpack_ctx_init(&ctx);
}

static
void ctx_teardown(void)
{
	mmpack_ctx_deinit(&ctx);
}

START_TEST(test_valid_dependencies)
{
	int rv;
	struct action_stack * actions;
	struct pkg_request req;

	rv = binary_index_populate(&ctx, valid_binindexes[_i]);
	ck_assert(rv == 0);

	req = (struct pkg_request){.name = "pkg-a", .version = "0.0.1"};
	actions = mmpkg_get_install_list(&ctx, &req);
	ck_assert(actions != NULL);

	mmpack_action_stack_dump(actions);
	mmpack_action_stack_destroy(actions);
}
END_TEST


START_TEST(test_valid_dependencies_multiple_req)
{
	int rv;
	struct action_stack * actions;
	struct pkg_request req[2];

	rv = binary_index_populate(&ctx, TEST_BININDEX_DIR"/simple.yaml");
	ck_assert(rv == 0);

	req[0] = (struct pkg_request){.name = "pkg-b", .next = &req[1]};
	req[1] = (struct pkg_request){.name = "pkg-a", .version = "0.0.1"};
	actions = mmpkg_get_install_list(&ctx, req);
	ck_assert(actions != NULL);

	mmpack_action_stack_dump(actions);
	mmpack_action_stack_destroy(actions);
}
END_TEST

START_TEST(test_invalid_dependencies)
{
	int rv;
	struct action_stack * actions;
	struct pkg_request req;

	rv = binary_index_populate(&ctx, invalid_binindexes[_i]);
	ck_assert(rv == 0);

	req = (struct pkg_request){.name = "pkg-a", .version = "0.0.1"};
	actions = mmpkg_get_install_list(&ctx, &req);
	ck_assert(actions == NULL);
}
END_TEST

TCase* create_dependencies_tcase(void)
{
    TCase * tc;

    tc = tcase_create("dependencies");
    tcase_add_checked_fixture(tc, ctx_setup, ctx_teardown);

    tcase_add_loop_test(tc, test_valid_dependencies, 0, NUM_VALID_BININDEXES);
    tcase_add_loop_test(tc, test_invalid_dependencies, 0, NUM_INVALID_BININDEXES);
    tcase_add_test(tc, test_valid_dependencies_multiple_req);

    return tc;
}
