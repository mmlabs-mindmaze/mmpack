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

typedef struct sha_digest {
	uint8_t alignas(16) u8[SHA256_DIGEST_SIZE];
} digest_t;


struct typed_hash {
	digest_t digest;
	int type;
};


static inline
int digest_equal(const digest_t* hash1, const digest_t* hash2)
{
	return memcmp(hash1->u8, hash2->u8, sizeof(hash1->u8)) == 0;
}


int hexstr_from_digest(char* hexstr, const digest_t* digest);
int digest_from_hexstr(digest_t* digest, struct strchunk hexstr);
int sha_file_compute(digest_t* digest, const char* filename);
int sha_compute(mmstr* hash, const mmstr* filename);
int check_hash(const mmstr* sha, const mmstr* filename);
int compute_typed_hash(struct typed_hash* hash, const char* filename);
int check_typed_hash(const struct typed_hash* ref, const char* filename);
int check_digest(const digest_t* hash, const char* filename);


#endif /* CRYPTO_H */
