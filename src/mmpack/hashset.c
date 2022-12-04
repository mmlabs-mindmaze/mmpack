/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmsysio.h>
#include <mmerrno.h>

#include "crypto.h"
#include "hashset.h"
#include "utils.h"


LOCAL_SYMBOL
void hashset_init(struct hashset* hashset)
{
	*hashset = (struct hashset) {0};
}


LOCAL_SYMBOL
void hashset_deinit(struct hashset* hashset)
{
	mm_unmap(hashset->slots);
	hashset->slots = NULL;
}


LOCAL_SYMBOL
int hashset_load_from_file(struct hashset* hashset, const char* path)
{
	int fd;
	int rv = -1;
	struct mm_stat buf;

	fd = mm_open(path, O_RDONLY, 0);
	if (fd < 0)
		return -1;

	if (mm_fstat(fd, &buf))
		goto exit;

	// check size is power of 2
	if (buf.size & (buf.size-1)) {
		mm_raise_error(MM_EBADFMT, "size of %s not power of 2", path);
		goto exit;
	}

	hashset->slots = mm_mapfile(fd, 0, buf.size, MM_MAP_READ);
	rv = hashset->slots ? 0 : -1;

	hashset->mask = buf.size / sizeof(*hashset->slots) - 1;

exit:
	mm_close(fd);
	return rv;
}


LOCAL_SYMBOL
int hashset_create_slots(struct hashset* hashset, const char* path, int npkg)
{
	int fd;
	digest_t* slots;
	uint64_t nslot = next_pow2_u64(npkg * 2);
	size_t filesize = nslot * sizeof(*slots);

	fd = mm_open(path, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if (fd < 0)
		return -1;

	mm_ftruncate(fd, filesize);
	slots = mm_mapfile(fd, 0, filesize, MM_MAP_SHARED|MM_MAP_RDWR);

	hashset->mask = nslot - 1;
	hashset->slots = slots;
	return 0;
}


LOCAL_SYMBOL
void hashset_add(struct hashset* hashset, const digest_t* digest)
{
	uint64_t mask = hashset->mask;
	uint64_t idx = digest->u64[0] & mask;
	digest_t* restrict candidate;

	for (idx = digest->u64[0] & mask;; idx = (idx + 1) & mask) {
		candidate = hashset->slots + idx;
		if (digest_is_zero(candidate))
			*candidate = *digest;
	}
}


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

