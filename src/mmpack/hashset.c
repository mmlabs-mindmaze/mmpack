/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmerrno.h>
#include <mmpredefs.h>
#include <mmsysio.h>

#include "crypto.h"
#include "hashset.h"
#include "utils.h"


/**
 * hashset_init_from_file() - initialize and load hashset from file
 * @hashset:    pointer to hashset structure to initialize
 * @path:       path to file to load hashset from
 *
 * Return: 0 in case of success, -1 otherwise.
 */
LOCAL_SYMBOL
int hashset_init_from_file(struct hashset* hashset, const char* path)
{
	int fd;
	struct mm_stat st;

	*hashset = (struct hashset) {.slots = NULL};

	fd = mm_open(path, O_RDONLY, 0);
	if (fd < 0)
		return -1;

	if (mm_fstat(fd, &st))
		goto exit;

	// check size is power of 2
	if (st.size & (st.size-1)) {
		mm_raise_error(MM_EBADFMT, "size of %s not power of 2", path);
		goto exit;
	}

	hashset->mask = st.size / sizeof(*hashset->slots) - 1;
	hashset->slots = mm_mapfile(fd, 0, st.size, MM_MAP_SHARED|MM_MAP_READ);

exit:
	mm_close(fd);
	return hashset->slots ? 0 : -1;
}


/**
 * hashset_deinit() - deinitialize hashset
 * @hashset:    pointer to hashset structure to dispose
 */
LOCAL_SYMBOL
void hashset_deinit(struct hashset* hashset)
{
	mm_unmap(hashset->slots);
	hashset->slots = NULL;
}


/**
 * hashset_contains() - test whether a digest in the hashset
 * @hashset:    pointer to hashset structure
 * @digest:     pointer to digest to test the presence in hashset
 *
 * Return: 1 if @digest is in @hashset, 0 otherwise.
 */
LOCAL_SYMBOL
int hashset_contains(const struct hashset* hashset, const digest_t* digest)
{
	uint64_t mask = hashset->mask;
	uint64_t idx = digest->u64[0] & mask;
	const digest_t* restrict candidate;

	do {
		candidate = hashset->slots + idx;
		if (digest_equal(digest, candidate))
			return 1;

		idx = (idx + 1) & mask;
	} while (!digest_is_zero(candidate));

	return 0;
}


static
int hashset_create_slots(struct hashset* hashset, const char* path, int num)
{
	int fd, rv = -1;
	digest_t* slots;
	size_t filesize;

	// number of slot must be a power of 2, the table load small enough so
	// that search ends quickly on average. And anything smaller than a
	// page is a waste of disk space.
	filesize = next_pow2_u64(2 * num * sizeof(*slots));
	if (filesize < MM_PAGESZ)
		filesize = MM_PAGESZ;

	// Create the file of slots, resetting to its size to 0 if it exists.
	fd = mm_open(path, O_RDWR|O_CREAT|O_TRUNC, 0666);
	if (fd < 0)
		return -1;

	// resize file from 0 to filesize, all new data are guaranteed to be 0
	if (mm_ftruncate(fd, filesize))
		goto exit;

	slots = mm_mapfile(fd, 0, filesize, MM_MAP_SHARED|MM_MAP_RDWR);
	if (!slots)
		goto exit;

	hashset->mask = (filesize / sizeof(*slots)) - 1;
	hashset->slots = slots;
	rv = 0;

exit:
	mm_close(fd);
	return rv;
}


static
void hashset_add(struct hashset* hashset, const digest_t* restrict digest)
{
	uint64_t mask = hashset->mask;
	uint64_t idx;
	digest_t* restrict candidate;

	for (idx = digest->u64[0] & mask;; idx = (idx + 1) & mask) {
		candidate = hashset->slots + idx;
		if (digest_is_zero(candidate)) {
			*candidate = *digest;
			break;
		}
	}
}


/**
 * create_hashset() - generate an hashset file from an array of digest
 * @path:       path to file to create
 * @num:        number of element in the array pointed by @digest
 * @digest:     pointer to the array of digest
 *
 * Return: 0 in case of success, -1 otherwise.
 */
LOCAL_SYMBOL
int create_hashset(const char* path, int num, const digest_t* digest)
{
	struct hashset hashset = {.slots = NULL};
	int i;

	if (hashset_create_slots(&hashset, path, num))
		return -1;

	for (i = 0; i < num; i++)
		hashset_add(&hashset, digest + i);

	hashset_deinit(&hashset);

	return 0;
}

