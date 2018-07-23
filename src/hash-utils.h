/*
 * @mindmaze_header@
 */
#ifndef HASH_UTILS_H
#define HASH_UTILS_H

#define HASH_HEXSTR_SIZE (32*2+1) // string of SHA-256 in hexa (\0 incl.)


int hash_compute(char* hash, const char* filename, const char* parent);

#endif
