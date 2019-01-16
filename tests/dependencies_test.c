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
	TEST_BININDEX_DIR"/complex-dependency.yaml",
};
#define NUM_VALID_BININDEXES MM_NELEM(valid_binindexes)

static const char * valid_mmpack_deps[][7] = {
	{"pkg-a", NULL},
	{"pkg-f", "pkg-e", "pkg-d", "pkg-c", "pkg-b", "pkg-a", NULL},
	{"pkg-c", "pkg-a", "pkg-b", NULL},
	{"pkg-a", "pkg-b", "pkg-c", "pkg-d", "pkg-e", NULL},
};

static const char * invalid_binindexes[] = {
	TEST_BININDEX_DIR"/unsolvable-dependencies.yaml",
};
#define NUM_INVALID_BININDEXES MM_NELEM(invalid_binindexes)


/**************************************************************************
 *                                                                        *
 *                          action stack verification                     *
 *                                                                        *
 **************************************************************************/

static
int is_dep_fullfil_in_stack(const struct mmpkg_dep* dep,
                            const struct action_stack* stack)
{
	int i;
	const struct mmpkg* pkg;

	for (i = 0; i < stack->index; i++) {
		pkg = stack->actions[i].pkg;
		if (!mmstrequal(pkg->name, dep->name))
			continue;

		if (  pkg_version_compare(pkg->version, dep->max_version) > 0
		   || pkg_version_compare(dep->min_version, pkg->version) > 0) {
			fprintf(stderr, "dependency not fulfilled!\n");
			mmpkg_dep_dump(dep, "MMPACK");
			mmpkg_dump(pkg);
			return 0;
		}

		return 1;
	}

	fprintf(stderr, "dependency %s not found\n", dep->name);
	return 0;
}


static
int is_stack_consistent(const struct action_stack* stack)
{
	int i;
	const struct mmpkg_dep* dep;

	for (i = 0; i < stack->index; i++) {
		dep = stack->actions[i].pkg->mpkdeps;
		while (dep) {
			if (!is_dep_fullfil_in_stack(dep, stack))
				return 0;

			dep = dep->next;
		}
	}

	return 1;
}


static
int does_stack_meet_requests(const struct action_stack* stack,
                             const struct pkg_request* req)
{
	const struct mmpkg* pkg;
	int i;

	while (req != NULL) {
		pkg = NULL;
		for (i = 0; i < stack->index; i++) {
			if (mmstrequal(stack->actions[i].pkg->name, req->name)) {
				pkg = stack->actions[i].pkg;
				break;
			}
		}

		if (!pkg) {
			fprintf(stderr, "%s requirement not found\n", req->name);
			return 0;
		}

		if (req->version && !mmstrequal(req->version, pkg->version)) {
			fprintf(stderr, "version of %s requirement not fulfilled\n"
			                "pkg:%s != req:%s", req->name,
					pkg->version, req->version);
			return 0;
		}

		req = req->next;
	}

	return 1;
}


static
int are_pkgs_expected_in_stack(const struct action_stack* stack,
                               const char* expected_pkgs[])
{
	int i, j, found;
	const char* pkgname;

	// Check all package in stack are in expected_pkgs
	for (i = 0; i < stack->index; i++) {
		pkgname = stack->actions[i].pkg->name;
		found = 0;
		for (j = 0; expected_pkgs[j]; j++) {
			if (!strcmp(expected_pkgs[j], pkgname)) {
				found = 1;
				break;
			}
		}

		if (!found) {
			fprintf(stderr, "%s is not expected\n", pkgname);
			return 0;
		}
	}

	// Check all package in expected_pkgs are in stack
	for (j = 0; expected_pkgs[j]; j++) {
		pkgname = expected_pkgs[j];
		found = 0;
		for (i = 0; i < stack->index; i++) {
			if (!strcmp(stack->actions[i].pkg->name, pkgname)) {
				found = 1;
				break;
			}
		}

		if (!found) {
			fprintf(stderr, "%s is not found in stack\n", pkgname);
			return 0;
		}
	}

	return 1;
}

/**************************************************************************
 *                                                                        *
 *                                 tests                                  *
 *                                                                        *
 **************************************************************************/
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
	int rv;
	struct action_stack * actions;
	struct pkg_request req;

	rv = binindex_populate(&ctx.binindex, valid_binindexes[_i], 0);
	ck_assert(rv == 0);

	req = (struct pkg_request){.name = pkg_a_name, .version = vers001};
	actions = mmpkg_get_install_list(&ctx, &req);
	ck_assert(actions != NULL);

	mmpack_action_stack_dump(actions);
	ck_assert(does_stack_meet_requests(actions, &req));
	ck_assert(is_stack_consistent(actions));
	ck_assert(are_pkgs_expected_in_stack(actions, valid_mmpack_deps[_i]));

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
	ck_assert(does_stack_meet_requests(actions, req));
	ck_assert(is_stack_consistent(actions));
	ck_assert(are_pkgs_expected_in_stack(actions, valid_mmpack_deps[1]));

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


/**************************************************************************
 *                                                                        *
 *                              test case setup                           *
 *                                                                        *
 **************************************************************************/
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
