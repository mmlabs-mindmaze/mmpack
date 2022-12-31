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

#include "buffer.h"
#include "crypto.h"
#include "mmstring.h"
#include "utils.h"
#include "testcases.h"

#define HASHFILE_DIR    TESTSDIR"/hash"

static
struct {
	size_t len;
	char name[64];
	digest_t refdigest;
} sha_cases[] = {
	{.len =  0, .name = "zero_len"},
	{.len =  4, .name = "tiny_file"},
	{.len =  8, .name = "hashsize_file"},
	{.len =  256, .name = "small_file"},
	{.len =  257, .name = "small_file2"},
	{.len =  512, .name = "afile"},
	{.len =  515, .name = "afile2"},
	{.len =  16 << 10, .name = "large_file"},
	{.len =  (16 << 10) + 63, .name = "large_file2"},
};
#define NUM_HASH_CASES	MM_NELEM(sha_cases)

static
struct {
	char name[64];
	char target[64];
	char refhash[SHA_HEXSTR_LEN+1];
} symlink_cases[] = {
	{.name = "a_link", .target = "zero_len",
	 .refhash = "6d712e7edfc58ec6ca9b28efd3a01175c7cdb4178921aab352ea0ec56b6cdab6"},
	{.name = "b_link", .target = "small_file",
	 .refhash = "63425cb6d2a9304d3ed60ea1aa2b31a2c7804c98ddf31fb818a45a36433a2f1c"},
	{.name = "links/bdir/blink", .target = "../adir",
	 .refhash = "4714d47f0664d0e100b078e25fc66994ee7251f172062040dcadc4fe4cd85f20"},
	{.name = "links/some dir/foo", .target = "to_non_existent",
	 .refhash = "88a3f6baef51ad2c2a21443bd3178d9f8bb7bbfcdd8b49146643272d1c572709"},
	{.name = "links/some dir/bar", .target = "../adir/to_non_existent",
	 .refhash = "c238df1ae2bd59950b09421be20f3c2f1e7e1c4d04ae872b9704664178b86ad6"},
	{.name = "c_link", .target = "links/adir",
	 .refhash = "b5f07c53c8f582a56708b4cb455a997a043a56da57ea3d94d23d0fc1369fddce"},
};
#define NUM_SYMLINK_CASES       MM_NELEM(symlink_cases)

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
 * gen_ref_md() - compute the reference digest with openssl cmdline tool
 * @digest:     buffer receiving the digest
 * filename:    path of the file to hash
 */
static
void gen_ref_digest(digest_t* digest, const char* filename)
{
	char* cmd[] = {"openssl", "sha256", "-binary", (char*)filename, NULL};
	struct buffer output;

	buffer_init(&output);
	execute_cmd_capture_output(cmd, &output);
	memcpy(digest, output.base, sizeof(*digest));
	buffer_deinit(&output);
}


static
void sha_setup(void)
{
	int i;
	char filename[sizeof(HASHFILE_DIR) + 64 + 2];

	// Create the test folder that will contains the files generated for the test
	mm_mkdir(HASHFILE_DIR, 0755, MM_RECURSIVE);

	// For each case, generate the filename from the test folder,
	// create the file and compute the reference hash
	for (i = 0; i < NUM_HASH_CASES; i++) {
		strcpy(filename, HASHFILE_DIR "/");
		strcat(filename, sha_cases[i].name);
		create_rand_file(filename, sha_cases[i].len);
		gen_ref_digest(&sha_cases[i].refdigest, filename);
	}

	// Create subfolder for symbolic links
	mm_mkdir(HASHFILE_DIR"/links/adir", 0755, MM_RECURSIVE);
	mm_mkdir(HASHFILE_DIR"/links/bdir", 0755, MM_RECURSIVE);
	mm_mkdir(HASHFILE_DIR"/links/some dir", 0755, MM_RECURSIVE);

	for (i = 0; i < NUM_SYMLINK_CASES; i++) {
		strcpy(filename, HASHFILE_DIR "/");
		strcat(filename, symlink_cases[i].name);
		mm_symlink(symlink_cases[i].target, filename);
	}
}


static
void sha_cleanup(void)
{
	mm_remove(HASHFILE_DIR, MM_RECURSIVE|MM_DT_ANY);
}


/**************************************************************************
 *                                                                        *
 *                         Hash test functions                            *
 *                                                                        *
 **************************************************************************/
STATIC_CONST_MMSTR(hashfile_dir, HASHFILE_DIR);


START_TEST(binary_sha)
{
	mmstr* fullpath = mmstr_alloca(256);
	const mmstr* filename = mmstr_alloca_from_cstr(sha_cases[_i].name);
	const digest_t* ref = &sha_cases[_i].refdigest;
	digest_t digest;

	mmstr_join_path(fullpath, hashfile_dir, filename);

	sha_file_compute(&digest, fullpath);
	ck_assert_mem_eq(&digest, ref, sizeof(digest));
	ck_assert(digest_equal(&digest, ref));
}
END_TEST


START_TEST(typed_hash_regfile)
{
	struct typed_hash hash;
	mmstr* fullpath = mmstr_alloca(256);
	const mmstr* filename = mmstr_alloca_from_cstr(sha_cases[_i].name);
	const digest_t* refdigest = &sha_cases[_i].refdigest;

	mmstr_join_path(fullpath, hashfile_dir, filename);

	compute_typed_hash(&hash, fullpath);
	ck_assert_mem_eq(&hash.digest, refdigest, sizeof(*refdigest));
	ck_assert_int_eq(hash.type, MM_DT_REG);
}
END_TEST


START_TEST(typed_hash_symlink)
{
	struct typed_hash hash;
	mmstr* fullpath = mmstr_alloca(256);
	const mmstr* filename = mmstr_alloca_from_cstr(symlink_cases[_i].name);
	const char* refhexstr = symlink_cases[_i].refhash;
	digest_t refdigest;

	digest_from_hexstr(&refdigest, strchunk_from_cstr(refhexstr));

	mmstr_join_path(fullpath, hashfile_dir, filename);

	compute_typed_hash(&hash, fullpath);
	ck_assert_mem_eq(&hash.digest, &refdigest, sizeof(refdigest));
	ck_assert_int_eq(hash.type, MM_DT_LNK);
}
END_TEST



static const digest_t ref_digest = {{
  0x98, 0x10, 0x76, 0x2e, 0x95, 0x72, 0x0f, 0x46, 0xfc, 0xdf, 0x29, 0x00,
  0xfa, 0x1d, 0x15, 0x77, 0x06, 0xaf, 0x21, 0x66, 0xa5, 0x0a, 0x30, 0xae,
  0x75, 0xe6, 0xe6, 0x65, 0xe3, 0xbd, 0xae, 0x54
}};
static char ref_hexstr[] = \
	"9810762e95720f46fcdf2900fa1d157706af2166a50a30ae75e6e665e3bdae54";

START_TEST(digest_to_str)
{
	const digest_t* digest = &ref_digest;
	char hexstr[SHA_HEXLEN + 1];

	hexstr_from_digest(hexstr, digest);
	hexstr[SHA_HEXLEN] = '\0';

	ck_assert_str_eq(hexstr, ref_hexstr);
}
END_TEST


START_TEST(digest_from_str)
{
	digest_t digest;
	struct strchunk hexstr = {
		.buf = ref_hexstr,
		.len = strlen(ref_hexstr),
	};

	digest_from_hexstr(&digest, hexstr);
	ck_assert_mem_eq(&digest, &ref_digest, sizeof(digest));
}
END_TEST


/**************************************************************************
 *                                                                        *
 *                         test suite creation                            *
 *                                                                        *
 **************************************************************************/
TCase* create_sha_tcase(void)
{
	TCase * tc;

	tc = tcase_create("sha");
	tcase_add_unchecked_fixture(tc, sha_setup, sha_cleanup);

	tcase_add_loop_test(tc, binary_sha, 0, NUM_HASH_CASES);
	tcase_add_loop_test(tc, typed_hash_regfile, 0, NUM_HASH_CASES);
	tcase_add_loop_test(tc, typed_hash_symlink, 0, NUM_SYMLINK_CASES);
	tcase_add_test(tc, digest_to_str);
	tcase_add_test(tc, digest_from_str);

	return tc;
}
