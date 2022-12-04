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


void hashset_init(struct hashset* hashset);
void hashset_deinit(struct hashset* hashset);
int hashset_create_slots(struct hashset* hashset, const char* path, int npkg);
void hashset_add(struct hashset* hashset, const digest_t* digest);
int hashset_load_from_file(struct hashset* hashset, const char* path);
int hashset_contains(const struct hashset* hashset, const digest_t* digest);

#endif /* ifndef HASHSET_H */
