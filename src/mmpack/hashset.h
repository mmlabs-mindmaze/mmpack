/*
 * @mindmaze_header@
 */
#ifndef HASHSET_H
#define HASHSET_H

#include "crypto.h"


struct hashset {
	uint32_t mask;
	digest_t* slots;
};


void hashset_init(struct hashset* hashset);
void hashset_deinit(struct hashset* hashset);
int hashset_load_from_file(struct hashset* hashset, const char* path);
int hashset_contains(struct hashset* hashset, const digest_t* digest);

#endif /* ifndef HASHSET_H */
