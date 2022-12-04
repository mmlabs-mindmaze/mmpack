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

	hashset->mask = buf.size - 1;

exit:
	mm_close(fd);
	return rv;
}


LOCAL_SYMBOL
int hashset_contains(struct hashset* hashset, const digest_t* digest)
{
	uint32_t mask = hashset->mask;
	uint32_t idx = digest->u32[0] & mask;
	const digest_t* candidate;

	do {
		candidate = hashset->slots + idx;
		if (digest_equal(digest, candidate))
			return 1;

		idx = (idx + 1) & mask;
	} while (!digest_is_zero(candidate));

	return 0;
}

