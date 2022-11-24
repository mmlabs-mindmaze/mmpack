/*
 * @mindmaze_header@
 */

#ifndef CRYPTO_H
#define CRYPTO_H

#include <nettle/sha2.h>
#include <stdalign.h>
#include <stdint.h>

#include "mmstring.h"


#define SHA_HDR_REG "reg-"
#define SHA_HDR_SYM "sym-"
#define SHA_HDRLEN 4
/* string of header and SHA-256 in hexa (\0 NOT incl.) */
#define SHA_HEXSTR_LEN (SHA_HDRLEN + 32*2)

struct sha_digest {
	uint8_t data[SHA256_DIGEST_SIZE];
};

int sha_file_compute(struct sha_digest* digest, const char* filename);
int sha_compute(mmstr* hash, const mmstr* filename, int follow);
int check_hash(const mmstr* sha, const mmstr* filename);


#endif /* CRYPTO_H */
