/*
 * @mindmaze_header@
 */
#ifndef HASHSET_H
#define HASHSET_H

#include "crypto.h"


struct hashset {
	int nbit;
	const digest_t*	slots;
};


void hashset_init(struct hashset* hashset);
int hashset_load_from_file(struct hashset* hashset, const char* path);
void hashset_deinit(struct hashset* hashset);

#endif /* ifndef HASHSET_H */
