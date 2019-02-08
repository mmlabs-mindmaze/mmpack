/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <check.h>

#include "action-solver.h"
#include "common.h"
#include "context.h"
#include "package-utils.h"
#include "testcases.h"

#define TEST_BININDEX_DIR SRCDIR"/tests/binary-indexes"

static struct mmpack_ctx ctx;

static const char * valid_binindexes[] = {
	TEST_BININDEX_DIR"/simplest.yaml",
	TEST_BININDEX_DIR"/simple.yaml",
	TEST_BININDEX_DIR"/circular.yaml",
};
#define NUM_VALID_BININDEXES MM_NELEM(valid_binindexes)

static const char * valid_mmpack_deps[][6] = {
	{"pkg-a"},
	{"pkg-f", "pkg-e", "pkg-d", "pkg-c", "pkg-b", "pkg-a"},
	{"pkg-c", "pkg-a", "pkg-b"},
};

static const char * invalid_binindexes[] = {
	TEST_BININDEX_DIR"/dependency-issue.yaml",
};
#define NUM_INVALID_BININDEXES MM_NELEM(invalid_binindexes)

STATIC_CONST_MMSTR(pkg_a_name, "pkg-a");
STATIC_CONST_MMSTR(pkg_b_name, "pkg-b");
STATIC_CONST_MMSTR(vers001, "0.0.1");

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

START_TEST(test_valid_dependencies)
{
	int rv, i;
	struct action_stack * actions;
	struct pkg_request req;
	char const * pkg;

	rv = binindex_populate(&ctx.binindex, valid_binindexes[_i], 0);
	ck_assert(rv == 0);

	req = (struct pkg_request){.name = pkg_a_name, .version = vers001};
	actions = mmpkg_get_install_list(&ctx, &req);
	ck_assert(actions != NULL);

	mmpack_action_stack_dump(actions);
	for (i = 0 ; i < actions->index ; i++) {
		pkg = actions->actions[i].pkg->name;
		ck_assert(strcmp(pkg, valid_mmpack_deps[_i][i]) == 0);
	}

	mmpack_action_stack_destroy(actions);
}
END_TEST


START_TEST(test_valid_dependencies_multiple_req)
{
	int rv;
	struct action_stack * actions;
	struct pkg_request req[2];

	rv = binindex_populate(&ctx.binindex, TEST_BININDEX_DIR"/simple.yaml", 0);
	ck_assert(rv == 0);

	req[0] = (struct pkg_request){.name = pkg_b_name, .next = &req[1]};
	req[1] = (struct pkg_request){.name = pkg_a_name, .version = vers001};
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

	rv = binindex_populate(&ctx.binindex, invalid_binindexes[_i], 0);
	ck_assert(rv == 0);

	req = (struct pkg_request){.name = pkg_a_name, .version = vers001};
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
