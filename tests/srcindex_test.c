/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <check.h>
#include <stdbool.h>
#include <stdio.h>

#include "crypto.h"
#include "mmstring.h"
#include "srcindex.h"
#include "testcases.h"


#define SRCINDEX_DIR    SRCDIR"/tests/source-indexes"


struct srcpkg_ref {
	mmstr* name;
	mmstr* filenames;
	digest_t srcsha;
	mmstr* version;
	size_t size;
	mmstr* repos;
};


static struct srcindex source_index;
static struct srcpkg_ref srcpkg_ref;
static struct repo repo0, repo1;


/**
 * get_ref_field() - return the next line value
 * @fp:         file stream opened for reading
 *
 * Return:
 * the content of the next line allocated in an mmstr*. call mmstr_free() when
 * not needed anymorea.
 */
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

	if (field[len-1] == '\n')
		mmstr_setlen(field, len-1);

	return field;
}


static
int get_ref_sha_field(digest_t* digest, FILE* fp)
{
	char line[128];

	if (fgets(line, sizeof(line), fp) == NULL)
		return -1;

	struct strchunk sc = {.buf = line, .len = strlen(line)};
	return digest_from_hexstr(digest, strchunk_strip(sc));
}


/**
 * srcpkg_ref_reinit() - cleanup package reference and reset fields
 * @ref:        source package reference to reset
 */
static
void srcpkg_ref_reinit(struct srcpkg_ref* ref)
{
	mmstr_free(ref->name);
	mmstr_free(ref->filenames);
	mmstr_free(ref->version);
	mmstr_free(ref->repos);

	*ref = (struct srcpkg_ref) {NULL};
}


/**
 * read_srcpkg_ref() - read one source package reference entry
 * @ref:         pointer to a package reference to fill
 * @fp:          file stream opened on a reference file
 *
 * Return:
 * true if one entry has been fully read, false if end of file has been reached
 */
static
bool read_srcpkg_ref(struct srcpkg_ref* ref, FILE* fp)
{
	mmstr* size_field = NULL;
	mmstr* delim = NULL;
	bool retval = false;

	srcpkg_ref_reinit(ref);

	ref->name = get_ref_field(fp);
	get_ref_sha_field(&ref->srcsha, fp);
	size_field = get_ref_field(fp);
	ref->version = get_ref_field(fp);
	ref->repos = get_ref_field(fp);
	ref->filenames = get_ref_field(fp);

	// Read all lines until end of file or a line starting with "=="
	delim = get_ref_field(fp);
	while (delim) {
		if (strncmp(delim, "==", 2) == 0)
			break;

		mmstr_free(delim);
		delim = get_ref_field(fp);
	}

	if (!ref->name
	   || !ref->filenames
	   || !ref->version
	   || !size_field
	   || !ref->repos) {
		goto exit;
	}

	ref->size = atoi(size_field);
	retval = true;
	printf("name=%s\nfilename=%s\n\n", ref->name, ref->filenames);

exit:
	mmstr_free(delim);
	mmstr_free(size_field);
	return retval;
}



/**
 * check_srcpkg() - check a source package match the reference
 * @pkg:        pointer to source package to test
 * @ref:        pointer to reference source package
 *
 * In case of mismatch, ck_abort*() is called
 */
static
void check_srcpkg(const struct srcpkg* pkg, const struct srcpkg_ref* ref)
{
	char *tokenrepo, *tokenfname;
	char *ptr_repo, *ptr_fname;
	struct remote_resource* res = pkg->remote_res;

	ck_assert_str_eq(pkg->name, ref->name);
	ck_assert_str_eq(pkg->version, ref->version);

	// iterate over space separated value of ref and check it match the
	// fields of each remote resource list element. sha256 and size should
	// remain the same.
	tokenrepo = ref->repos;
	tokenfname = ref->filenames;
	while (1) {
		tokenrepo = strtok_r(tokenrepo, " ", &ptr_repo);
		tokenfname = strtok_r(tokenfname, " ", &ptr_fname);
		if (!tokenrepo)
			break;

		ck_assert(res != NULL);
		ck_assert_str_eq(res->repo->name, tokenrepo);
		ck_assert_str_eq(res->filename, tokenfname);
		ck_assert(digest_equal(&res->sha256, &ref->srcsha));
		ck_assert_int_eq(res->size, ref->size);

		tokenrepo = tokenfname = NULL;
		res = res->next;
	}
	ck_assert(res == NULL);
}


/**
 * check_srcindex_content() - check source package in index match reference
 * @srcindex:   pointer to source index to inspect
 * @reference:  path to file describing the expected content
 *
 * In case of mismatch, ck_abort*() is called
 */
static
void check_srcindex_content(struct srcindex* srcindex, const char* reference)
{
	FILE* fp;
	struct srcpkg_ref* ref = &srcpkg_ref;
	const struct srcpkg* pkg;

	fp = fopen(reference, "r");
	ck_assert(fp != NULL);

	while (read_srcpkg_ref(ref, fp)) {
		pkg = srcindex_lookup(srcindex,
				      ref->name, ref->version, &ref->srcsha);
		ck_assert(pkg != NULL);
		check_srcpkg(pkg, ref);
	}

	fclose(fp);
	srcpkg_ref_reinit(ref);
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
	srcpkg_ref_reinit(&srcpkg_ref);
}


static
void teardown_test(void)
{
	srcindex_deinit(&source_index);
	srcpkg_ref_reinit(&srcpkg_ref);
}


START_TEST(simple)
{
	int rv;
	struct srcindex* srcindex = &source_index;

	rv = srcindex_populate(srcindex, SRCINDEX_DIR"/simple", &repo0);
	ck_assert(rv == 0);
	check_srcindex_content(srcindex, SRCINDEX_DIR"/reference.simple");
}
END_TEST


START_TEST(multi)
{
	int rv;
	struct srcindex* srcindex = &source_index;

	rv = srcindex_populate(srcindex, SRCINDEX_DIR"/multi-repo0", &repo0);
	ck_assert(rv == 0);
	rv = srcindex_populate(srcindex, SRCINDEX_DIR"/multi-repo1", &repo1);
	ck_assert(rv == 0);
	check_srcindex_content(srcindex, SRCINDEX_DIR"/reference.multi");
}
END_TEST


TCase* create_srcindex_tcase(void)
{
	TCase * tc;

	tc = tcase_create("srcindex");
	tcase_add_checked_fixture(tc, setup_tcase, teardown_tcase);
	tcase_add_checked_fixture(tc, setup_test, teardown_test);

	tcase_add_test(tc, simple);
	tcase_add_test(tc, multi);

	return tc;
}
