/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <check.h>

#include "mmpack-config.h"
#include "testcases.h"

#define TEST_CONFIG SRCDIR"/mmpack-config.yaml"

static
int check_server_entry(char const * server, char const * url, void * arg)
{
	static int i = 0;

	(void) arg;

	i++;
	if (i == 1) {
		ck_assert_str_eq(server, "server1");
		ck_assert_str_eq(url, "http://mmpack-server-1:8888/");
	} else if (i == 2) {
		ck_assert_str_eq(server, "server2");
		ck_assert_str_eq(url, "http://mmpack-server-2:8888/");
	} else {
		ck_abort();
	}

	return 0;
}

START_TEST(test_foreach_config_server)
{
	int rv;

	rv = foreach_config_server(TEST_CONFIG, check_server_entry, NULL);
	ck_assert(rv == 0);
}
END_TEST

TCase* create_config_tcase(void)
{
    TCase * tc;

    tc = tcase_create("config");
    tcase_add_checked_fixture(tc, NULL, NULL);

    tcase_add_test(tc, test_foreach_config_server);

    return tc;
}
