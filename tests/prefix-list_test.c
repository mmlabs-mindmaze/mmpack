/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <check.h>
#include <mmsysio.h>
#include <string.h>

#include "hashset.h"
#include "mmstring.h"
#include "prefix-list.h"
#include "testcases.h"
#include "utils.h"

#define PREFIX_DIR       TESTSDIR"/prefix-list-tests/prefixes"
#define PREFIX_LIST_PATH TESTSDIR"/prefix-list-tests/known_prefixes"

STATIC_CONST_MMSTR(first_prefix, PREFIX_DIR"/first");
STATIC_CONST_MMSTR(second_prefix, PREFIX_DIR"/second");
STATIC_CONST_MMSTR(third_prefix, PREFIX_DIR"/third");
STATIC_CONST_MMSTR(first_missing_prefix, PREFIX_DIR"/first_missing");
STATIC_CONST_MMSTR(second_missing_prefix, PREFIX_DIR"/second_missing");


static
void make_dummy_prefix(const char* prefix)
{
	mmstr *path, *dir;

	path = mmstr_asprintf(NULL, "%s/"HASHSET_RELPATH, prefix);
	dir = mmstr_malloc(mmstrlen(path));
	mmstr_dirname(dir, path);

	mm_mkdir(dir, 0777, MM_RECURSIVE);
	create_hashset(path, 0, NULL);

	mmstr_free(dir);
	mmstr_free(path);
}


static
void setup(void)
{
	mm_mkdir(PREFIX_DIR, 0777, MM_RECURSIVE);
	set_prefix_list_path(PREFIX_LIST_PATH);

	make_dummy_prefix(first_prefix);
	make_dummy_prefix(second_prefix);
	make_dummy_prefix(third_prefix);
}


static
void cleanup(void)
{
	set_prefix_list_path(NULL);
}

/**************************************************************************
 *                                                                        *
 *                         prefix list test functions                     *
 *                                                                        *
 **************************************************************************/


static
void gen_known_prefix(const char* prefixes[])
{
	int fd, i;

	fd = mm_open(PREFIX_LIST_PATH, O_WRONLY|O_CREAT|O_TRUNC, 0666);

	for (i = 0; prefixes[i]; i++) {
		mm_write(fd, prefixes[i], strlen(prefixes[i]));
		mm_write(fd, "\n", 1);
	}

	mm_close(fd);
}


static
int check_prefix_in_list(const char* prefix)
{
	int rv;
	char* cmd[] = {"grep", "-q", (char*)prefix, PREFIX_LIST_PATH, NULL};

	rv = execute_cmd(cmd);
	ck_assert(rv >= 0);
	return (rv == 0);
}


START_TEST(filter_prefix_list)
{
	struct strset prefix_set;
	const char* init_prefixes[] = {
		first_prefix,
		first_missing_prefix,
		second_prefix,
		second_missing_prefix,
		NULL
	};
	gen_known_prefix(init_prefixes);

	strset_init(&prefix_set, STRSET_HANDLE_STRINGS_MEM);
	load_other_prefixes(&prefix_set, third_prefix);

	ck_assert(strset_contains(&prefix_set, first_prefix));
	ck_assert(strset_contains(&prefix_set, second_prefix));
	ck_assert(!strset_contains(&prefix_set, first_missing_prefix));
	ck_assert(!strset_contains(&prefix_set, second_missing_prefix));

	strset_deinit(&prefix_set);

	ck_assert(check_prefix_in_list(first_prefix));
	ck_assert(check_prefix_in_list(second_prefix));
	ck_assert(!check_prefix_in_list(first_missing_prefix));
	ck_assert(!check_prefix_in_list(second_missing_prefix));
}
END_TEST


START_TEST(update_with_prefix)
{
	const char* init_prefixes[] = {
		first_prefix,
		first_missing_prefix,
		second_prefix,
		second_missing_prefix,
		NULL
	};
	gen_known_prefix(init_prefixes);

	update_prefix_list_with_prefix(third_prefix);

	ck_assert(check_prefix_in_list(first_prefix));
	ck_assert(check_prefix_in_list(second_prefix));
	ck_assert(check_prefix_in_list(first_missing_prefix));
	ck_assert(check_prefix_in_list(second_missing_prefix));

	ck_assert(check_prefix_in_list(third_prefix));
}
END_TEST


/**************************************************************************
 *                                                                        *
 *                         test suite creation                            *
 *                                                                        *
 **************************************************************************/
TCase* create_prefix_list_tcase(void)
{
	TCase * tc;

	tc = tcase_create("prefix-list");
	tcase_add_unchecked_fixture(tc, setup, cleanup);

	tcase_add_test(tc, filter_prefix_list);
	tcase_add_test(tc, update_with_prefix);

	return tc;
}
