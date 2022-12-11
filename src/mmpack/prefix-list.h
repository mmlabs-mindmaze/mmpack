/*
 * @mindmaze_header@
 */
#ifndef PREFIX_LIST_H
#define PREFIX_LIST_H

#include "strset.h"

int load_other_prefixes(struct strset* set, const char* current_prefix);
int update_prefix_list_with_current_prefix(const char* current_prefix);


#endif /* ifndef PREFIX_LIST_H */
