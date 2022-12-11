/*
 * @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <check.h>
#include <mmlib.h>
#include <stdlib.h>

#include "testcases.h"

static
Suite* api_suite(void)
{
	Suite *s = suite_create("mmpack");

	suite_add_tcase(s, create_binindex_tcase());
	suite_add_tcase(s, create_srcindex_tcase());
	suite_add_tcase(s, create_config_tcase());
	suite_add_tcase(s, create_dependencies_tcase());
	suite_add_tcase(s, create_version_tcase());
	suite_add_tcase(s, create_sha_tcase());
	suite_add_tcase(s, create_indextable_tcase());
	suite_add_tcase(s, create_hashset_tcase());
	suite_add_tcase(s, create_misc_tcase());
	suite_add_tcase(s, create_prefix_list_tcase());

	return s;
}


int main(void)
{
	int exitcode = EXIT_SUCCESS;
	Suite* suite;
	SRunner* runner;

	// Ensure that global user config shall not be loaded: any bad
	// formatting in the sense of the version currently being built could
	// lead to failure will being completely unrelated what is tested.
	mm_setenv("XDG_CONFIG_HOME", "/non-existing-dir", MM_ENV_OVERWRITE);

	suite = api_suite();
	runner = srunner_create(suite);
#if defined(CHECK_SUPPORT_TAP)
	srunner_set_tap(runner, "-");
#endif

	srunner_run_all(runner, CK_ENV);

#if !defined(CHECK_SUPPORT_TAP)
    if (srunner_ntests_failed(runner) != 0)
        exitcode = EXIT_FAILURE;
#endif

	srunner_free(runner);

	return exitcode;
}
