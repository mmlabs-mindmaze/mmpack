/*
 * @mindmaze_header@
 */
#ifndef HASHSET_H
#define HASHSET_H

#include "crypto.h"
#include "install-state.h"


struct hashset {
	uint64_t mask;
	digest_t* slots;
};


int hashset_init_from_file(struct hashset* hashset, const char* path);
int hashset_contains(const struct hashset* hashset, const digest_t* digest);
void hashset_deinit(struct hashset* hashset);

int create_hashset(const char* path, int num, const digest_t* digest);

#endif /* ifndef HASHSET_H */
