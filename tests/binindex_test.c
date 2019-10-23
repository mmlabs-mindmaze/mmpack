/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <check.h>

#include "package-utils.h"
#include "testcases.h"
#include "settings.h"

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
	struct repolist_elt repo;

	repo.url = mmstr_malloc_from_cstr(binindexes[_i]);
	repo.name = mmstr_malloc_from_cstr(binindexes[_i]);

	rv = binindex_populate(&binary_index, binindexes[_i], &repo);
	ck_assert(rv == 0);
	binindex_dump(&binary_index);

	mmstr_free(repo.url);
	mmstr_free(repo.name);
}
END_TEST


static
int check_from_repo_fully_set(struct from_repo * from_repo)
{
	struct from_repo * elt = from_repo;

	while (elt) {
		ck_assert(elt->size != 0);
		ck_assert(elt->sha256 != NULL);
		ck_assert(elt->filename != NULL);
		ck_assert(elt->repo != NULL);
		elt = elt->next;
	}
	return 0;
}


static
int check_pkg_fully_set(struct mmpkg* pkg, void * data)
{
	int* count = data;

	(*count)++;
	ck_assert(pkg->from_repo != NULL);
	check_from_repo_fully_set(pkg->from_repo);
	return 0;
}



START_TEST(test_deduplicate)
{
	int rv;
	int count;
	struct repolist_elt repo;

	repo.url = mmstr_malloc_from_cstr("http://url_simple.com");
	repo.name = mmstr_malloc_from_cstr("name_simple");

	rv = binindex_populate(&binary_index,
	                       TEST_BININDEX_DIR"/installed-simple.yaml", NULL);
	ck_assert_msg(rv == 0, "Installed list loading failed");


	rv = binindex_populate(&binary_index, TEST_BININDEX_DIR"/simple.yaml",
	                       &repo);
	ck_assert_msg(rv == 0, "repository list loading failed");

	// Check eack package has its repo specific fields set and count the
	// number of package
	count = 0;
	binindex_foreach(&binary_index, check_pkg_fully_set, &count);

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
