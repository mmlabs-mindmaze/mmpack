/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <check.h>

#include "settings.h"
#include "testcases.h"

#define TEST_CONFIG SRCDIR"/tests/mmpack-config.yaml"

START_TEST(test_parse_settings)
{
	int rv;
	struct settings settings;

	settings_init(&settings);

	rv = settings_load(&settings, TEST_CONFIG);
	ck_assert(rv == 0);
	ck_assert_int_eq(settings_num_repo(&settings), 3);
	ck_assert_str_eq(settings_get_repo_url(&settings, 0), "http://mmpack.is.cool:8888/");
	ck_assert_str_eq(settings_get_repo_url(&settings, 1), "https://www.awesome.com");
	ck_assert_str_eq(settings_get_repo_url(&settings, 2), "http://another.host.com/");

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
