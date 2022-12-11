/*
 * @mindmaze_header@
 */
#ifndef PREFIX_LIST_H
#define PREFIX_LIST_H

#include "strset.h"

void set_prefix_list_path(const char* path);
int load_other_prefixes(struct strset* set, const char* ignore_prefix);
int update_prefix_list_with_prefix(const char* prefix);


#endif /* ifndef PREFIX_LIST_H */
