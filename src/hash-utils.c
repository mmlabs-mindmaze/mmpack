/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmlib.h>
#include <mmsysio.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <string.h>

#include "hash-utils.h"

#define HASH_UPDATE_SIZE	512

/**
 * conv_to_hexstr() - convert byte array into hexadecimal string
 * @hexstr:     output string, must be (2*@len + 1) long
 * @data:       byte array to convert
 * @len:        length of @data
 *
 * This function generates the hexadecimal string representation of a byte
 * array. The string set in @hexstr will be NULL terminated
 */
static
void conv_to_hexstr(char* hexstr, const unsigned char* data, size_t len)
{
	const char hexlut[] = "0123456789abcdef";
	unsigned char d;

	// Add null termination
	hexstr[2*len] = '\0';

	while (len > 0) {
		len--;
		d = data[len];
		hexstr[2*len + 0] = hexlut[(d >> 4) & 0x0F];
		hexstr[2*len + 1] = hexlut[(d >> 0) & 0x0F];
	}
}


/**
 * hash_fd_compute() - compute SHA256 hash of an open file
 * @hash:       string buffer receiving the hexadecimal form of hash. The
 *              pointed buffer must be HASH_HEXSTR_SIZE long.
 * @fd:         file descriptor of a file opened for reading
 *
 * The computed hash is stored in hexadecimal as a NULL-terminated string
 * in @hash string buffer which must be at least HASH_HEXSTR_SIZE long
 * (this include the NULL termination).
 *
 * Return: 0 in case of success, -1 if a problem of file reading has been
 * encountered.
 */
static
int hash_fd_compute(char* hash, int fd)
{
	unsigned char md[SHA256_DIGEST_LENGTH], data[HASH_UPDATE_SIZE];
	SHA256_CTX ctx;
	ssize_t rsz;
	int rv = 0;

	SHA256_Init(&ctx);

	do {
		rsz = mm_read(fd, data, sizeof(data));
		if (rsz < 0) {
			rv = -1;
			break;
		}

		SHA256_Update(&ctx, data, rsz);
	} while (rsz > 0);

	SHA256_Final(md, &ctx);
	conv_to_hexstr(hash, md, sizeof(md));

	return rv;
}


/**
 * hash_compute() - compute SHA256 hash on specified file
 * @hash:       string buffer receiving the hexadecimal form of hash. The
 *              pointed buffer must be HASH_HEXSTR_SIZE long.
 * @filename:   path of file whose hash must be computed
 * @parent:     prefix directory to prepend to @filename to get the
 *              final path of the file to hash. This may be NULL
 *
 * This function allows to compute the SHA256 hash of a file located at
 *  * @parent/@filename if @parent is non NULL
 *  * @filename if @parent is NULL
 *
 * The computed hash is stored in hexadecimal as a NULL-terminated string
 * in @hash string buffer which must be at least HASH_HEXSTR_SIZE long
 * (this include the NULL termination).
 *
 * Return: 0 in case of success, -1 if a problem of file reading has been
 * encountered.
 */
LOCAL_SYMBOL
int hash_compute(char* hash, const char* filename, const char* parent)
{
	char* fullpath;
	size_t len;
	int fd = -1;
	int rv = 0;

	fullpath = (char*)filename;
	if (parent != NULL) {
		len = strlen(filename) + strlen(parent) + 2;
		fullpath = mm_malloca(len);
		if (!fullpath)
			return -1;

		sprintf(fullpath, "%s/%s", parent, filename);
	}

	/* Open file and compute hash and close */
	if (  (fd = mm_open(fullpath, O_RDONLY, 0)) < 0
	   || hash_fd_compute(hash, fd)) {
		rv = -1;
	}
	mm_close(fd);

	if (fullpath != filename)
		mm_freea(fullpath);

	return rv;
}
