/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <check.h>

#include "package-utils.h"
#include "testcases.h"


START_TEST(test_version_formats)
{
	ck_assert(pkg_version_compare("1", "2") < 0);

	ck_assert(pkg_version_compare("1.0.0", "2.0.0") < 0);
	ck_assert(pkg_version_compare("2.0.0", "1.0.0") > 0);
	ck_assert(pkg_version_compare("1.2.3", "5.1.0") < 0);
	ck_assert(pkg_version_compare("1.2.3", "1.2.3") == 0);
	ck_assert(pkg_version_compare("v1.2.3", "v2.3.4") < 0);
	ck_assert(pkg_version_compare("1", "1.1") < 0);
	ck_assert(pkg_version_compare("1.2", "1.2.1") < 0);
	ck_assert(pkg_version_compare("16.04", "18.04") < 0);
	ck_assert(pkg_version_compare("16.04", "18.04") < 0);
	ck_assert(pkg_version_compare("16.10", "16.9") > 0);
	ck_assert(pkg_version_compare("01.10", "10.9") < 0);
	ck_assert(pkg_version_compare("01.9", "1.9") == 0);
	ck_assert(pkg_version_compare("v01.9.0", "v1.90.0") < 0);
	ck_assert(pkg_version_compare("vv1.9.0", "v01.9.0") > 0);

	ck_assert(pkg_version_compare("1.0.0", "any") == 0);
	ck_assert(pkg_version_compare("any", "1.0.0") == 0);
}
END_TEST

TCase* create_version_tcase(void)
{
    TCase * tc;

    tc = tcase_create("version");
    tcase_add_checked_fixture(tc, NULL, NULL);

    tcase_add_test(tc, test_version_formats);

    return tc;
}
