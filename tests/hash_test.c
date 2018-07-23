/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <check.h>
#include <mmlog.h>
#include <mmpredefs.h>
#include <mmsysio.h>
#include <stdio.h>
#include <stdlib.h>

#include "hash-utils.h"
#include "testcases.h"

#define HASHFILE_DIR    BUILDDIR"/hash"

static
struct {
	size_t len;
	char name[64];
	char refhash[HASH_HEXSTR_SIZE];
} hash_cases[] = {
	{.len =  0, .name = "zero_len"},
	{.len =  4, .name = "tiny_file"},
	{.len =  8, .name = "hashsize_file"},
	{.len =  256, .name = "small_file"},
	{.len =  257, .name = "small_file2"},
	{.len =  512, .name = "afile"},
	{.len =  515, .name = "afile2"},
	{.len =  1024*1024*64, .name = "large_file"},
	{.len =  1024*1024*64 + 63, .name = "large_file2"},
};
#define NUM_HASH_CASES	MM_NELEM(hash_cases)

/**************************************************************************
 *                                                                        *
 *                    setup and cleanup test environment                  *
 *                                                                        *
 **************************************************************************/
/**
 * create_rand_file() - Create a file with random content
 * @filename:   location where the file must be created
 * @len:        length in byte of the created file
 */
static
void create_rand_file(const char* filename, size_t len)
{
	int i, fd;
	int data[512];
	size_t iter_sz;
	ssize_t rsz;

	fd = mm_open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	mm_check(fd >= 0);

	while (len) {
		// Create random block data (can be bigger than needed)
		for (i = 0; i < MM_NELEM(data); i++)
			data[i] = rand();

		iter_sz = (len > sizeof(data)) ? sizeof(data) : len;
		rsz = mm_write(fd, data, iter_sz);
		mm_check(rsz >= 0);

		len -= rsz;
	}

	mm_close(fd);
}


/**
 * gen_ref_hash() - compute the reference hash with openssl cmdline tool
 * @hash:       buffer receiving the hash (must be HASH_HEXSTR_SIZE long)
 * filename:    path of the file to hash
 */
static
void gen_ref_hash(char* hash, const char* filename)
{
	char cmd[512];
	FILE* fp;

	sprintf(cmd, "openssl sha256 -hex %s | cut -f2 -d' '", filename);
	fp = popen(cmd, "r");
	fread(hash, HASH_HEXSTR_SIZE-1, 1, fp);
	fclose(fp);

	hash[HASH_HEXSTR_SIZE-1] = '\0';
}


static
void hash_setup(void)
{
	int i;
	char filename[128];

	// Create the test folder that will contains the files generated for the test
	mm_mkdir(HASHFILE_DIR, 0755, MM_RECURSIVE);

	// For each case, generate the filename from the test folder,
	// create the file and compute the reference hash
	for (i = 0; i < NUM_HASH_CASES; i++) {
		sprintf(filename, "%s/%s", HASHFILE_DIR, hash_cases[i].name);
		create_rand_file(filename, hash_cases[i].len);
		gen_ref_hash(hash_cases[i].refhash, filename);
	}
}


static
void hash_cleanup(void)
{
	mm_remove(HASHFILE_DIR, MM_RECURSIVE|MM_DT_ANY);
}


/**************************************************************************
 *                                                                        *
 *                         Hash test functions                            *
 *                                                                        *
 **************************************************************************/
START_TEST(hashfile_with_parent)
{
	char hash[HASH_HEXSTR_SIZE];
	const char* filename = hash_cases[_i].name;
	const char* refhash = hash_cases[_i].refhash;

	hash_compute(hash, filename, HASHFILE_DIR);
	ck_assert_str_eq(hash, refhash);
}
END_TEST


START_TEST(hashfile_without_parent)
{
	char hash[HASH_HEXSTR_SIZE];
	char fullpath[256];
	const char* filename = hash_cases[_i].name;
	const char* refhash = hash_cases[_i].refhash;

	sprintf(fullpath, "%s/%s", HASHFILE_DIR, filename);

	hash_compute(hash, fullpath, NULL);
	ck_assert_str_eq(hash, refhash);
}
END_TEST


/**************************************************************************
 *                                                                        *
 *                         test suite creation                            *
 *                                                                        *
 **************************************************************************/
TCase* create_hash_tcase(void)
{
	TCase * tc;

	tc = tcase_create("hash");
	tcase_add_unchecked_fixture(tc, hash_setup, hash_cleanup);

	tcase_add_loop_test(tc, hashfile_without_parent, 0, NUM_HASH_CASES);
	tcase_add_loop_test(tc, hashfile_with_parent, 0, NUM_HASH_CASES);

	return tc;
}
