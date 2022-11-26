/*
 * @mindmaze_header@
 */

#ifndef CRYPTO_H
#define CRYPTO_H

#include <nettle/sha2.h>
#include <stdalign.h>
#include <stdint.h>
#include <string.h>

#include "mmstring.h"
#include "strchunk.h"


#define SHA_HDR_REG "reg-"
#define SHA_HDR_SYM "sym-"
#define SHA_HDRLEN 4
#define SHA_HEXLEN (32 * 2)
/* string of header and SHA-256 in hexa (\0 NOT incl.) */
#define SHA_HEXSTR_LEN (SHA_HDRLEN + SHA_HEXLEN)

struct sha_digest {
	uint8_t data[SHA256_DIGEST_SIZE];
};


static inline
int sha_equal(const struct sha_digest* hash1, const struct sha_digest* hash2)
{
	return memcmp(hash1->data, hash2->data, sizeof(hash1->data)) == 0;
}


int sha_digest_to_hexstr(char* hexstr, const struct sha_digest* digest);
int hexstr_to_sha_digest(struct sha_digest* digest, struct strchunk hexstr);
int sha_file_compute(struct sha_digest* digest, const char* filename);
int sha_compute(mmstr* hash, const mmstr* filename, int follow);
int check_hash(const mmstr* sha, const mmstr* filename);
int check_sha_digest(const struct sha_digest* hash, const char* filename);


#endif /* CRYPTO_H */
