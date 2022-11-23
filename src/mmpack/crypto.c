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
#include "mmstring.h"

#define HASH_UPDATE_SIZE 512


/**************************************************************************
 *                                                                        *
 *                            SHA computation helper                      *
 *                                                                        *
 **************************************************************************/
/**
 * conv_to_hexstr() - convert byte array into hexadecimal string
 * @hexstr:     output string, must be (2*@len) long
 * @data:       byte array to convert
 * @len:        length of @data
 *
 * This function generates the hexadecimal string representation of a byte
 * array.
 *
 * Return: length of the string written in @hexstr
 */
static
int conv_to_hexstr(char* hexstr, const unsigned char* data, size_t len)
{
	const char hexlut[] = "0123456789abcdef";
	unsigned char d;
	size_t i;

	for (i = 0; i < len; i++) {
		d = data[i];
		hexstr[2*i + 0] = hexlut[(d >> 4) & 0x0F];
		hexstr[2*i + 1] = hexlut[(d >> 0) & 0x0F];
	}

	return 2*len;
}


/**
 * sha_fd_compute() - compute SHA256 hash of an open file
 * @hash:       mmstr* buffer receiving the hexadecimal form of hash. The
 *              pointed buffer must be SHA_HEXSTR_LEN long.
 * @fd:         file descriptor of a file opened for reading
 *
 * The computed hash is stored in hexadecimal as a mmstr* string in @hash whose
 * length must be at least SHA_HEXSTR_LEN long, as reported by mmstr_maxlen().
 *
 * Return: 0 in case of success, -1 if a problem of file reading has been
 * encountered.
 */
static
int sha_fd_compute(char* hash, int fd)
{
	uint8_t md[SHA256_DIGEST_SIZE], data[HASH_UPDATE_SIZE];
	struct sha256_ctx ctx;
	ssize_t rsz;
	int rv = 0;

	sha256_init(&ctx);

	do {
		rsz = mm_read(fd, data, sizeof(data));
		if (rsz < 0) {
			rv = -1;
			break;
		}

		sha256_update(&ctx, rsz, data);
	} while (rsz > 0);

	sha256_digest(&ctx, sizeof(md), md);

	conv_to_hexstr(hash, md, sizeof(md));

	return rv;
}


static
int sha_regfile_compute(mmstr* hash, const char* path, int with_prefix)
{
	int fd;
	int rv = 0;
	char* hexstr;

	// If with_prefix is set, SHA_HDR_REG ("reg-") is prefixed in hash
	if (with_prefix) {
		memcpy(hash, SHA_HDR_REG, SHA_HDRLEN);
		hexstr = hash + SHA_HDRLEN;
		mmstr_setlen(hash, SHA_HEXSTR_LEN);
	} else {
		hexstr = hash;
		mmstr_setlen(hash, SHA_HEXSTR_LEN - SHA_HDRLEN);
	}

	if ((fd = mm_open(path, O_RDONLY, 0)) < 0
	    || sha_fd_compute(hexstr, fd)) {
		rv = -1;
	}

	mm_close(fd);

	return rv;
}


static
int sha_symlink_compute(mmstr* hash, const char* path, size_t target_size)
{
	uint8_t md[SHA256_DIGEST_SIZE];
	struct sha256_ctx ctx;
	char* buff;
	int len;
	int rv = -1;

	buff = xx_malloca(target_size);
	if (mm_readlink(path, buff, target_size))
		goto exit;

	sha256_init(&ctx);
	sha256_update(&ctx, target_size-1, (uint8_t*)buff);
	sha256_digest(&ctx, sizeof(md), md);

	// Convert sha into hexadecimal and with "sym-" prefixed
	memcpy(hash, SHA_HDR_SYM, SHA_HDRLEN);
	len = conv_to_hexstr(hash + SHA_HDRLEN, md, sizeof(md));
	mmstr_setlen(hash, len + SHA_HDRLEN);

	rv = 0;

exit:
	mm_freea(buff);
	return rv;
}


/**
 * sha_compute() - compute SHA256 hash on specified file
 * @hash:       mmstr* buffer receiving the hexadecimal form of hash. The
 *              pointed string must be at least HASH_HEXSTR_LEN long.
 * @filename:   path of file whose hash must be computed
 * @follow:     if set to non zero and the file is a symlink, the hash is
 *              computed on the file it refers to (ie the symlink is
 *              followed). If set to zero the generated hash is prefixed
 *              by file type indicator (regular file or symlink).
 *
 * This function allows to compute the SHA256 hash of a file located at
 * @filename.
 *
 * The computed hash is stored in hexadecimal as a NULL terminated string
 * in @hash which must be at least SHA_HEXSTR_LEN long (this include the
 * NULL termination).
 *
 * If @follow is zero, hash string written in @hash is prefixed with the
 * type ("reg-" or "sym-"). The constraints on the size of the buffer
 * pointed to by @hash do not changes (SHA_HEXSTR_LEN takes into account
 * the possible type prefix).
 *
 * Return: 0 in case of success, -1 if a problem of file reading has been
 * encountered.
 */
LOCAL_SYMBOL
int sha_compute(mmstr* hash, const mmstr* filename, int follow)
{
	mmstr* fullpath = NULL;
	int rv = 0;
	int needed_len, with_prefix;
	struct mm_stat st;

	with_prefix = !follow;

	needed_len = SHA_HEXSTR_LEN - SHA_HDRLEN;
	needed_len += with_prefix ? SHA_HDRLEN : 0;
	if (mmstr_maxlen(hash) < needed_len)
		return mm_raise_error(EOVERFLOW, "hash argument to short");

	if (mm_stat(filename, &st, follow ? 0 : MM_NOFOLLOW)) {
		rv = -1;
		goto exit;
	}

	if (S_ISREG(st.mode)) {
		rv = sha_regfile_compute(hash, filename, with_prefix);
	} else if (S_ISLNK(st.mode)) {
		rv = sha_symlink_compute(hash, filename, st.size);
	} else {
		rv = mm_raise_error(EINVAL, "%s is neither a regular file "
		                    "or symlink", filename);
	}

exit:
	mmstr_freea(fullpath);
	if (rv == -1)
		mm_log_error("Cannot compute SHA-256 of %s", filename);

	return rv;
}


/**
 * check_hash() - Check integrity of given file
 * @ref_sha: reference file sha256 to compare against
 * @filename: path of file whose hash must be computed
 *
 * Return: 0 if no issue has been found, -1 otherwise
 */
LOCAL_SYMBOL
int check_hash(const mmstr* refsha, const mmstr* filename)
{
	int follow;
	mmstr* sha = mmstr_alloca(SHA_HEXSTR_LEN);

	// If reference hash contains type prefix (ie its length is
	// SHA_HEXSTR_LEN), symlink must not be followed
	follow = 0;
	if (mmstrlen(refsha) != SHA_HEXSTR_LEN)
		follow = 1;

	if (sha_compute(sha, filename, follow))
		return -1;

	if (!mmstrequal(sha, refsha)) {
		mm_raise_error(EBADMSG, "bad SHA-256 detected %s", filename);
		return -1;
	}

	return 0;
}


