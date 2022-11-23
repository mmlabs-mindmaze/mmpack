/*
 * @mindmaze_header@
 */

#ifndef CRYPTO_H
#define CRYPTO_H

#include "mmstring.h"


#define SHA_HDR_REG "reg-"
#define SHA_HDR_SYM "sym-"
#define SHA_HDRLEN 4
/* string of header and SHA-256 in hexa (\0 NOT incl.) */
#define SHA_HEXSTR_LEN (SHA_HDRLEN + 32*2)

int sha_compute(mmstr* hash, const mmstr* filename, int follow);
int check_hash(const mmstr* sha, const mmstr* filename);


#endif /* CRYPTO_H */
