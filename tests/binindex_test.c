/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <check.h>

#include "package-utils.h"
#include "repo.h"
#include "testcases.h"
#include "settings.h"

#define TEST_BININDEX_DIR SRCDIR"/tests/binary-indexes"
#define NUM_PKGS_IN_SIMPLE_YAML 6

static struct binindex binary_index;

static const char * binindexes[] = {
	TEST_BININDEX_DIR"/simplest.gz",
	TEST_BININDEX_DIR"/simple.gz",
	TEST_BININDEX_DIR"/circular.gz",
	TEST_BININDEX_DIR"/complex-dependency.gz",
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
	struct repo repo;

	repo.url = mmstr_malloc_from_cstr(binindexes[_i]);
	repo.name = mmstr_malloc_from_cstr(binindexes[_i]);

	rv = binindex_populate(&binary_index, binindexes[_i], &repo);
	ck_assert(rv == 0);

	mmstr_free(repo.url);
	mmstr_free(repo.name);
}
END_TEST


static
int check_from_repo_fully_set(struct remote_resource* res)
{
	struct remote_resource* elt = res;

	while (elt) {
		ck_assert(elt->size != 0);
		ck_assert(elt->filename != NULL);
		ck_assert(elt->repo != NULL);
		elt = elt->next;
	}
	return 0;
}


START_TEST(test_deduplicate)
{
	int rv;
	int count;
	struct repo repo;
	struct binpkg* pkg;
	struct pkg_iter iter;

	repo.url = mmstr_malloc_from_cstr("http://url_simple.com");
	repo.name = mmstr_malloc_from_cstr("name_simple");

	rv = binindex_populate(&binary_index,
	                       TEST_BININDEX_DIR"/installed-simple.gz", NULL);
	ck_assert_msg(rv == 0, "Installed list loading failed");


	rv = binindex_populate(&binary_index, TEST_BININDEX_DIR"/simple.gz",
	                       &repo);
	ck_assert_msg(rv == 0, "repository list loading failed");

	// Check eack package has its repo specific fields set and count the
	// number of package
	count = 0;
	pkg = pkg_iter_first(&iter, &binary_index);
	for (; pkg != NULL; pkg = pkg_iter_next(&iter), count++) {
		ck_assert(pkg->remote_res != NULL);
		check_from_repo_fully_set(pkg->remote_res);
	}

	ck_assert_int_eq(count, NUM_PKGS_IN_SIMPLE_YAML);

	mmstr_free(repo.url);
	mmstr_free(repo.name);
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
