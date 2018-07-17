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
    Suite* suite;
    SRunner* runner;

    suite = api_suite();
    runner = srunner_create(suite);

    srunner_run_all(runner, CK_ENV);

    srunner_free(runner);

    return 0;
}
