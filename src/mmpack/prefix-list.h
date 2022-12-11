/*
 * @mindmaze_header@
 */
#ifndef PREFIX_LIST_H
#define PREFIX_LIST_H

#include "strlist.h"

struct strlist load_other_prefixes(const char* current_prefix);
int update_prefix_list_with_current_prefix(const char* current_prefix);


#endif /* ifndef PREFIX_LIST_H */
