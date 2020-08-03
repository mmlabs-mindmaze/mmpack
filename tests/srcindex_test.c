/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <check.h>
#include <stdbool.h>
#include <stdio.h>

#include "mmstring.h"
#include "srcindex.h"
#include "testcases.h"


#define SRCINDEX_DIR SRCDIR"/tests/source-indexes"


static struct srcindex source_index;
static struct repo repo0, repo1;


struct srcpkg_ref {
	mmstr* name;
	mmstr* filename;
	mmstr* srcsha;
	mmstr* version;
	size_t size;
	mmstr* repos;
};


static
mmstr* get_ref_field(FILE* fp)
{
	char line[128];
	mmstr* field;
	int len;

	if (fgets(line, sizeof(line), fp) == NULL)
		return NULL;

	field = mmstr_malloc_from_cstr(line);
	len = mmstrlen(field);

	if (field[len] == '\n')
		mmstr_setlen(field, len-1);

	return field;
}


static
void srcpkg_ref_deinit(struct srcpkg_ref* ref)
{
	mmstr_free(ref->name);
	mmstr_free(ref->filename);
	mmstr_free(ref->srcsha);
	mmstr_free(ref->version);
	mmstr_free(ref->repos);

	*ref = (struct srcpkg_ref) {NULL};
}


static
bool read_srcpkg_ref(struct srcpkg_ref* ref, FILE* fp)
{
	mmstr* size_field = NULL;
	bool retval = true;

	srcpkg_ref_deinit(ref);

	ref->name = get_ref_field(fp);
	ref->filename = get_ref_field(fp);
	ref->srcsha = get_ref_field(fp);
	size_field = get_ref_field(fp);
	ref->version = get_ref_field(fp);
	ref->repos = get_ref_field(fp);

	if (!ref->name
	   || !ref->filename
	   || !ref->version
	   || !ref->srcsha
	   || !ref->repos)
		retval = false;

	ref->size = atoi(size_field);
	mmstr_free(size_field);
	return retval;
}


static
void check_srcindex_content(const char* reference)
{
	FILE* fp;
	struct srcpkg_ref pkgref = {NULL};

	fp = fopen(reference, "r");
	ck_assert(fp != NULL);

	while (read_srcpkg_ref(&pkgref, fp)) {
	}

	fclose(fp);
	srcpkg_ref_deinit(&pkgref);
}


static
void setup_tcase(void)
{
	repo0.name = mmstrcpy_cstr_realloc(NULL, "first-repo");
	repo1.name = mmstrcpy_cstr_realloc(NULL, "second-repo");
}


static
void teardown_tcase(void)
{
	mmstr_free(repo0.name);
	mmstr_free(repo1.name);
}


static
void setup_test(void)
{
	srcindex_init(&source_index);
}


static
void teardown_test(void)
{
	srcindex_deinit(&source_index);
}


START_TEST(parsing)
{
	int rv;

	rv = srcindex_populate(&source_index, SRCINDEX_DIR"/simple", &repo0);
	ck_assert(rv == 0);
	check_srcindex_content(SRCINDEX_DIR"/reference.simple");
}
END_TEST


TCase* create_srcindex_tcase(void)
{
	TCase * tc;

	tc = tcase_create("srcindex");
	tcase_add_checked_fixture(tc, setup_tcase, teardown_tcase);
	tcase_add_checked_fixture(tc, setup_test, teardown_test);

	tcase_add_test(tc, parsing);

	return tc;
}
