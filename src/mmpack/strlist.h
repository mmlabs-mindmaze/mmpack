/*
 * @mindmaze_header@
 */
#ifndef STRLIST_H
#define STRLIST_H

#include "mmstring.h"
#include "strchunk.h"


struct strlist_elt {
	struct strlist_elt* next;
	struct mmstring str;
};

struct strlist {
	struct strlist_elt* head;
	struct strlist_elt* last;
};

void strlist_init(struct strlist* list);
void strlist_deinit(struct strlist* list);
int strlist_add_strchunk(struct strlist* list, struct strchunk sv);
int strlist_add(struct strlist* list, const char* str);
void strlist_remove(struct strlist* list, const mmstr* str);
void strlist_drop_after(struct strlist* list, struct strlist_elt* elt);


#endif /* STRLIST_H */
