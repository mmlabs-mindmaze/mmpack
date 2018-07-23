/*
 * @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <check.h>
#include <stdlib.h>

#include "testcases.h"


static
Suite* api_suite(void)
{
	Suite *s = suite_create("mmpack");

	suite_add_tcase(s, create_config_tcase());

	return s;
}


int main(void)
{
	int exitcode = EXIT_SUCCESS;
	Suite* suite;
	SRunner* runner;

	suite = api_suite();
	runner = srunner_create(suite);

	srunner_run_all(runner, CK_ENV);
	if (srunner_ntests_failed(runner) != 0)
		exitcode = EXIT_FAILURE;

	srunner_free(runner);

	return exitcode;
}
