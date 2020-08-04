/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <check.h>

#include "repo.h"
#include "settings.h"
#include "testcases.h"

#define TEST_CONFIG SRCDIR"/tests/mmpack-config.yaml"

START_TEST(test_parse_settings)
{
	int rv;
	struct settings settings;
	struct repo_iter iter;
	struct repo* repo;

	settings_init(&settings);

	rv = settings_load(&settings, TEST_CONFIG);
	ck_assert(rv == 0);
	ck_assert_int_eq(repolist_num_repo(&settings.repo_list), 3);

	repo = repo_iter_first(&iter, &settings.repo_list);
	ck_assert_str_eq(repo->url, "http://mmpack.is.cool:8888/");
	repo = repo_iter_next(&iter);
	ck_assert_str_eq(repo->url, "https://www.awesome.com");
	repo = repo_iter_next(&iter);
	ck_assert_str_eq(repo->url, "http://another.host.com/");

	ck_assert_str_eq(settings.default_prefix, "a/path/to/prefix");

	settings_deinit(&settings);
}
END_TEST

TCase* create_config_tcase(void)
{
    TCase * tc;

    tc = tcase_create("config");

    tcase_add_test(tc, test_parse_settings);

    return tc;
}
