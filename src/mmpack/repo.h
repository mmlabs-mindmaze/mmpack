/*
 * @mindmaze_header@
 */
#ifndef REPO_H
#define REPO_H

#include "mmstring.h"

/**
 * struct repo - structure representing a repository
 * @url: the url of the repository from which packages can be retrieved
 * @name: the short name referencing the url
 * @enabled: indicates if the repository is enabled (1) or disabled (0).
 */
struct repo {
	mmstr* url;
	mmstr* name;
	int enabled;
};


struct repolist_elt {
	struct repolist_elt* next;
	struct repo repo;
};

struct repolist {
	struct repolist_elt* head;
};

void repolist_init(struct repolist* list);
void repolist_deinit(struct repolist* list);
void repolist_reset(struct repolist* list);
struct repo* repolist_add(struct repolist* list, const char* name);
int repolist_remove(struct repolist * list, const char * name);
struct repo* repolist_lookup(struct repolist * list, const char * name);
int repolist_num_repo(const struct repolist* list);


struct repo_iter {
	struct repolist_elt* next;
};


static inline
struct repo* repo_iter_next(struct repo_iter* iter)
{
	struct repolist_elt* elt = iter->next;

	if (elt == NULL)
		return NULL;

	iter->next = elt->next;
	return &elt->repo;
}


static inline
struct repo* repo_iter_first(struct repo_iter* iter, struct repolist* list)
{
	iter->next = list->head;
	return repo_iter_next(iter);
}

#endif /* ifndef REPO_H */
