/*
 * @mindmaze_header@
 */
#ifndef PREFIX_LIST_H
#define PREFIX_LIST_H

#include "hashset.h"


struct prefix {
	mmstr* path;
	struct hashset set;
};

struct prefix_list {
	int num;
	struct prefix prefixes[];
};

struct prefix_list* prefix_list_load(const char* current_prefix);
void prefix_list_destroy(struct prefix_list* list);
int register_in_prefix_list(const char* prefix_path);


#endif /* ifndef PREFIX_LIST_H */
