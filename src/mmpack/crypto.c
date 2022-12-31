/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmsysio.h>
#include <nettle/sha2.h>
#include <stdlib.h>
#include <string.h>

#include "crypto.h"
#include "mmerrno.h"
#include "xx-alloc.h"

#define HASH_UPDATE_SIZE 512


/**************************************************************************
 *                                                                        *
 *                            SHA computation helper                      *
 *                                                                        *
 **************************************************************************/
/**
 * hexstr_from_digest() - convert binary sha256 hash into hexadecimal string
 * @hexstr:     output string, must be (2*@len) long
 * @digest:     pointer to structure holding the SHA256 digest.
 *
 * This function generates the hexadecimal string representation of a binary
 * SHA256 digest.
 *
 * Return: length of the string written in @hexstr
 */
LOCAL_SYMBOL
int hexstr_from_digest(char* hexstr, const digest_t* digest)
{
	const char hexlut[] = "0123456789abcdef";
	uint8_t d;
	size_t i;

	for (i = 0; i < sizeof(digest->u8); i++) {
		d = digest->u8[i];
		hexstr[2*i + 0] = hexlut[(d >> 4) & 0x0F];
		hexstr[2*i + 1] = hexlut[(d >> 0) & 0x0F];
	}

	return 2*sizeof(digest->u8);
}


/**
 * digest_from_hexstr() - convert hexadecimal string to digest
 * @digest:     pointer to digest to fill
 * @hexstr:     string chunk pointing a hexadecimal SHA256 value (64 bytes)
 *
 * Return: 0 in case of success, -1 if @hexstr points to an invalid value.
 */
LOCAL_SYMBOL
int digest_from_hexstr(digest_t* digest, struct strchunk hexstr)
{
	int i;
	uint8_t d;
	char c;

	if (hexstr.len != SHA_HEXLEN)
		goto error;

	for (i = 0; i < SHA_HEXLEN; i++) {
		c = hexstr.buf[i];
		if (c >= '0' && c <= '9')
			d = c - '0';
		else if (c >= 'a' && c <= 'f')
			d = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			d = c - 'A' + 10;
		else
			goto error;

		if (i % 2 == 0)
			digest->u8[i / 2] = d << 4;
		else
			digest->u8[i / 2] += d;
	}
	return 0;

error:
	return mm_raise_error(EINVAL, "invalid hexstr (l%i) (%.*s) argument",
	                      hexstr.len, hexstr.len, hexstr.buf);
}


/**
 * sha_file_compute() - compute SHA256 hash of a file
 * @digest:     pointer to structure holding the SHA256 digest.
 * @path:       path to file to hash
 *
 * Return: 0 in case of success, -1 if a problem of file reading has been
 * encountered.
 */
LOCAL_SYMBOL
int sha_file_compute(digest_t* digest, const char* path)
{
	uint8_t data[HASH_UPDATE_SIZE];
	struct sha256_ctx ctx;
	ssize_t rsz;
	int fd, rv = 0;

	sha256_init(&ctx);
	fd = mm_open(path, O_RDONLY, 0);
	if (fd < 0)
		return -1;

	do {
		rsz = mm_read(fd, data, sizeof(data));
		if (rsz < 0) {
			rv = -1;
			break;
		}

		sha256_update(&ctx, rsz, data);
	} while (rsz > 0);

	mm_close(fd);
	sha256_digest(&ctx, sizeof(digest->u8), digest->u8);

	return rv;
}


static
int sha_symlink_compute(digest_t* digest, const char* path, size_t target_size)
{
	struct sha256_ctx ctx;
	char* buff;
	int rv = -1;

	buff = xx_malloca(target_size);
	if (mm_readlink(path, buff, target_size))
		goto exit;

	sha256_init(&ctx);
	sha256_update(&ctx, target_size-1, (uint8_t*)buff);
	sha256_digest(&ctx, sizeof(digest->u8), digest->u8);

	rv = 0;

exit:
	mm_freea(buff);
	return rv;
}


/**
 * compute_typed_hash() - compute hash along with type on specified file
 * @hash:       pointer to struct typed_hash to received the values.
 * @filename:   path of file whose hash must be computed
 *
 * Return: 0 in case of success, -1 if a problem of file reading has been
 * encountered.
 */
LOCAL_SYMBOL
int compute_typed_hash(struct typed_hash* hash, const char* filename)
{
	int rv = -1;
	struct mm_stat st;

	if (mm_stat(filename, &st, MM_NOFOLLOW))
		goto exit;

	if (S_ISREG(st.mode)) {
		hash->type = MM_DT_REG;
		rv = sha_file_compute(&hash->digest, filename);
	} else if (S_ISLNK(st.mode)) {
		hash->type = MM_DT_LNK;
		rv = sha_symlink_compute(&hash->digest, filename, st.size);
	} else {
		mm_raise_error(EINVAL, "%s is neither a regular file "
		               "or symlink", filename);
	}

exit:
	if (rv)
		mm_log_error("Cannot compute SHA-256 of %s", filename);
	return rv;
}


/**
 * check_typed_hash() - Check integrity of given file
 * @ref:      reference typed hash to compare against
 * @filename: path of file whose hash must be computed
 *
 * Return: 0 if no issue has been found, -1 otherwise
 */
LOCAL_SYMBOL
int check_typed_hash(const struct typed_hash* ref, const char* filename)
{
	struct typed_hash hash;

	if (compute_typed_hash(&hash, filename))
		return -1;

	if (hash.type != ref->type) {
		mm_raise_error(EBADMSG, "unexpected filetype for %s", filename);
		return -1;
	}

	if (!digest_equal(&hash.digest, &ref->digest)) {
		mm_raise_error(EBADMSG, "bad SHA-256 detected %s", filename);
		return -1;
	}

	return 0;
}


/**
 * check_digest() - Check integrity of given file using digest
 * @ref:      reference sha256 digest to compare against
 * @filename: path of file whose hash must be computed
 *
 * Return: 0 if no issue has been found, -1 otherwise
 */
LOCAL_SYMBOL
int check_digest(const digest_t* ref, const char* filename)
{
	digest_t sha;

	if (sha_file_compute(&sha, filename))
		return -1;

	if (!digest_equal(&sha, ref)) {
		mm_raise_error(EBADMSG, "bad SHA-256 detected %s", filename);
		return -1;
	}

	return 0;
}


