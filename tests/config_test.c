/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <check.h>

#include "settings.h"
#include "testcases.h"

#define TEST_CONFIG SRCDIR"/mmpack-config.yaml"

START_TEST(test_parse_settings)
{
	int rv;
	struct settings settings;

	settings_init(&settings);

	rv = settings_load(&settings, TEST_CONFIG);
	ck_assert(rv == 0);
	ck_assert(settings.repo_url != NULL);
	ck_assert_str_eq(settings.repo_url, "http://another.host.com/");

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
