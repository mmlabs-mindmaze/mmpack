/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <check.h>
#include <mmpredefs.h>
#include <stdint.h>
#include <string.h>

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
	char path_data[64], expected_data[64];
	mmstr* result = mmstr_malloc(64);
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

	mmstr_free(result);
}
END_TEST


START_TEST(parse_basename)
{
	int i;
	char path_data[64], expected_data[64];
	mmstr* result = mmstr_malloc(64);
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

	mmstr_free(result);
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
 *                        string helpers tests                            *
 *                                                                        *
 **************************************************************************/

static const char lipsum[] =
"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed non risus. "
"Suspendisse lectus tortor, dignissim sit amet, adipiscing nec, ultricies sed, "
"dolor. Cras elementum ultrices diam. Maecenas ligula massa, varius a, semper "
"congue, euismod non, mi. Proin porttitor, orci nec nonummy molestie, enim est "
"eleifend mi, non fermentum diam nisl sit amet erat. Duis semper. Duis arcu "
"massa, scelerisque vitae, consequat in, pretium a, enim. Pellentesque congue. "
"Ut in risus volutpat libero pharetra tempor. Cras vestibulum bibendum augue. "
"Praesent egestas leo in pede. Praesent blandit odio eu enim. Pellentesque sed "
"dui ut augue blandit sodales. Vestibulum ante ipsum primis in faucibus orci "
"luctus et ultrices posuere cubilia Curae; Aliquam nibh. Mauris ac mauris sed "
"pede pellentesque fermentum. Maecenas adipiscing ante non diam sodales "
"hendrerit.";


static const char lipsum_ref_70_3indent[] =
"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed non \n"
"   risus. Suspendisse lectus tortor, dignissim sit amet, adipiscing nec, \n"
"   ultricies sed, dolor. Cras elementum ultrices diam. Maecenas ligula \n"
"   massa, varius a, semper congue, euismod non, mi. Proin porttitor, \n"
"   orci nec nonummy molestie, enim est eleifend mi, non fermentum diam \n"
"   nisl sit amet erat. Duis semper. Duis arcu massa, scelerisque vitae, \n"
"   consequat in, pretium a, enim. Pellentesque congue. Ut in risus \n"
"   volutpat libero pharetra tempor. Cras vestibulum bibendum augue. \n"
"   Praesent egestas leo in pede. Praesent blandit odio eu enim. \n"
"   Pellentesque sed dui ut augue blandit sodales. Vestibulum ante ipsum \n"
"   primis in faucibus orci luctus et ultrices posuere cubilia Curae; \n"
"   Aliquam nibh. Mauris ac mauris sed pede pellentesque fermentum. \n"
"   Maecenas adipiscing ante non diam sodales hendrerit.";


static const char lipsum_ref_40_pipeindent[] =
"Lorem ipsum dolor sit amet, consectetur \n"
"|adipiscing elit. Sed non risus. \n"
"|Suspendisse lectus tortor, dignissim \n"
"|sit amet, adipiscing nec, ultricies \n"
"|sed, dolor. Cras elementum ultrices \n"
"|diam. Maecenas ligula massa, varius a, \n"
"|semper congue, euismod non, mi. Proin \n"
"|porttitor, orci nec nonummy molestie, \n"
"|enim est eleifend mi, non fermentum \n"
"|diam nisl sit amet erat. Duis semper. \n"
"|Duis arcu massa, scelerisque vitae, \n"
"|consequat in, pretium a, enim. \n"
"|Pellentesque congue. Ut in risus \n"
"|volutpat libero pharetra tempor. Cras \n"
"|vestibulum bibendum augue. Praesent \n"
"|egestas leo in pede. Praesent blandit \n"
"|odio eu enim. Pellentesque sed dui ut \n"
"|augue blandit sodales. Vestibulum ante \n"
"|ipsum primis in faucibus orci luctus et \n"
"|ultrices posuere cubilia Curae; Aliquam \n"
"|nibh. Mauris ac mauris sed pede \n"
"|pellentesque fermentum. Maecenas \n"
"|adipiscing ante non diam sodales \n"
"|hendrerit.";

static const
struct {
	const char* input;
	const char* indent;
	int len;
	const char* ref;
} wrap_cases[] = {
	{.input = lipsum, .indent="   ", .len = 70,
	  .ref = lipsum_ref_70_3indent},
	{.input = lipsum, .indent="|", .len = 40,
	  .ref = lipsum_ref_40_pipeindent},
};


START_TEST(str_wrapping)
{
	mmstr* wrapped = mmstr_malloc(128);
	struct strchunk input = {
		.buf = wrap_cases[_i].input,
		.len = strlen(wrap_cases[_i].input),
	};
	const char* ref = wrap_cases[_i].ref;

	wrapped = linewrap_string(wrapped, input,
	                          wrap_cases[_i].len,
	                          wrap_cases[_i].indent);

	ck_assert_str_eq(ref, wrapped);
	mmstr_free(wrapped);
}
END_TEST


static
const char input_wrap_nl[] =
"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed non risus. "
"Suspendisse lectus tortor, dignissim sit amet, adipiscing nec, ultricies sed, "
"dolor. Cras elementum ultrices diam.\n"
"Maecenas ligula massa, varius a, semper congue, euismod non, mi. Proin "
"porttitor, orci nec nonummy molestie, enim est eleifend mi, non fermentum diam "
"nisl sit amet erat.\n"
"\n"
"Duis semper. Duis arcu massa, scelerisque vitae, consequat in, pretium a, enim. "
"Pellentesque congue. Ut in risus volutpat libero pharetra tempor. Cras "
"vestibulum bibendum augue.";

static
const char ref_wrap_nl[] =
"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed non \n"
"    risus. Suspendisse lectus tortor, dignissim sit amet, adipiscing nec, \n"
"    ultricies sed, dolor. Cras elementum ultrices diam.\n"
"    .\n"
"    Maecenas ligula massa, varius a, semper congue, euismod non, mi. \n"
"    Proin porttitor, orci nec nonummy molestie, enim est eleifend mi, non \n"
"    fermentum diam nisl sit amet erat.\n"
"    .\n"
"    .\n"
"    Duis semper. Duis arcu massa, scelerisque vitae, consequat in, \n"
"    pretium a, enim. Pellentesque congue. Ut in risus volutpat libero \n"
"    pharetra tempor. Cras vestibulum bibendum augue.";


START_TEST(str_wrapping_nl)
{
	mmstr* wrapped = mmstr_malloc(128);
	struct strchunk input = {
		.buf = input_wrap_nl,
		.len = strlen(input_wrap_nl),
	};

	wrapped = textwrap_string(wrapped, input, 70, "    ", "\n    .");

	ck_assert_str_eq(ref_wrap_nl, wrapped);
	mmstr_free(wrapped);
}
END_TEST


START_TEST(mmstr_formatted)
{
	int len;
	char ref[256];
	char data[] = "Hello world!";
	mmstr* formatted = mmstr_malloc(12);

	len = sprintf(ref, "%s/fd/%i", data, 42);
	formatted = mmstr_asprintf(formatted, "%s/fd/%i", data, 42);

	ck_assert_int_eq(mmstr_maxlen(formatted), len);
	ck_assert_int_eq(mmstrlen(formatted), len);
	ck_assert_str_eq(formatted, ref);

	mmstr_free(formatted);
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
	tcase_add_loop_test(tc, str_wrapping, 0, MM_NELEM(wrap_cases));
	tcase_add_test(tc, str_wrapping_nl);
	tcase_add_test(tc, mmstr_formatted);

	return tc;
}



