/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <check.h>
#include <stdint.h>

#include "mmstring.h"
#include "testcases.h"
#include "utils.h"
#include "common.h"


/**************************************************************************
 *                                                                        *
 *                       Path component parsing tests                     *
 *                                                                        *
 **************************************************************************/
static const struct {
	const char* path;
	const char* dir;
	const char* base;
} dir_comp_cases[] = {
	{.path = "/usr/lib",    .dir = "/usr",  .base = "lib"},
	{.path = "/usr/",       .dir = "/",     .base = "usr"},
	{.path = "usr",         .dir = ".",     .base = "usr"},
	{.path = "/",           .dir = "/",     .base = "/"},
	{.path = ".",           .dir = ".",     .base = "."},
	{.path = "..",          .dir = ".",     .base = ".."},
	{.path = "/usr//lib",   .dir = "/usr",  .base = "lib"},
	{.path = "/usr//lib//", .dir = "/usr",  .base = "lib"},
	{.path = "/usr///",     .dir = "/",     .base = "usr"},
	{.path = "///usr/",     .dir = "/",     .base = "usr"},
	{.path = "///",         .dir = "/",     .base = "/"},
	{.path = "./",          .dir = ".",     .base = "."},
	{.path = "../",         .dir = ".",     .base = ".."},
#if defined(_WIN32)
	{.path = "\\usr\\",     .dir = "\\",    .base = "usr"},
	{.path = "\\usr\\lib",  .dir = "\\usr", .base = "lib"},
#endif
};


START_TEST(parse_dirname)
{
	int i;
	char result_data[64], path_data[64], expected_data[64];
	mmstr* result = mmstr_map_on_array(result_data);
	mmstr* path = mmstr_map_on_array(path_data);
	mmstr* expected = mmstr_map_on_array(expected_data);

	for (i = 0; i < MM_NELEM(dir_comp_cases); i++) {
		mmstrcpy_cstr(path, dir_comp_cases[i].path);
		mmstrcpy_cstr(expected, dir_comp_cases[i].dir);

		ck_assert(mmstr_dirname(result, path) == result);

		if (!mmstrequal(result, expected)) {
			ck_abort_msg("mmstr_dirname(%s)=%s (expect %s)",
			             path, result, expected);
		}
	}
}
END_TEST


START_TEST(parse_basename)
{
	int i;
	char result_data[64], path_data[64], expected_data[64];
	mmstr* result = mmstr_map_on_array(result_data);
	mmstr* path = mmstr_map_on_array(path_data);
	mmstr* expected = mmstr_map_on_array(expected_data);

	for (i = 0; i < MM_NELEM(dir_comp_cases); i++) {
		mmstrcpy_cstr(path, dir_comp_cases[i].path);
		mmstrcpy_cstr(expected, dir_comp_cases[i].base);

		ck_assert(mmstr_basename(result, path) == result);

		if (!mmstrequal(result, expected)) {
			ck_abort_msg("mmstr_basename(%s)=%s (expect %s)",
			             path, result, expected);
		}
	}
}
END_TEST


START_TEST(next_pow2)
{
	ck_assert(next_pow2_u64(0) == 1);
	ck_assert(next_pow2_u64(1) == 1);
	ck_assert(next_pow2_u64(2) == 2);
	ck_assert(next_pow2_u64(3) == 4);
	ck_assert(next_pow2_u64(4) == 4);
	ck_assert(next_pow2_u64(5) == 8);
	ck_assert(next_pow2_u64(6) == 8);
	ck_assert(next_pow2_u64(7) == 8);
	ck_assert(next_pow2_u64(8) == 8);
	ck_assert(next_pow2_u64(0xFFFFFFFFull) == 0x100000000ull);
	ck_assert(next_pow2_u64(0x100000000ull) == 0x100000000ull);
	ck_assert(next_pow2_u64(0x100000001ull) == 0x200000000ull);
}
END_TEST

/**************************************************************************
 *                                                                        *
 *                          Test suite setup                              *
 *                                                                        *
 **************************************************************************/
LOCAL_SYMBOL
TCase* create_misc_tcase(void)
{
	TCase *tc = tcase_create("misc");

	tcase_add_test(tc, parse_dirname);
	tcase_add_test(tc, parse_basename);
	tcase_add_test(tc, next_pow2);

	return tc;
}



