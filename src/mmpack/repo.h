/*
 * @mindmaze_header@
 */
#ifndef REPO_H
#define REPO_H

#include "crypto.h"
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


/**
 * struct remote_resource - data available in remote server
 * @filename:   path to resource in repository @repo relative to its url
 * @sha256:     hash of the file to download
 * @size:       size of the file to download
 * @repo:       pointer to repository from which the file must be downloaded
 * @next:       pointer to the next resource element (same resource provided by
 *              another repository)
 *
 * This structure represents the node in the list of resource that are
 * available in remote repository. Each node has a size and an hash that may or
 * may not be different from the other node in the list. This is done on
 * purpose. The same resource can indeed by provided by in different format
 * depending on the repository. For example, different mmpack binary package
 * can provide the same underlying data, ie, same sumsha, but the package could
 * be generated using for example different compression level or algorithm.
 */
struct remote_resource {
	const mmstr* filename;
	digest_t sha256;
	size_t size;
	const struct repo* repo;
	struct remote_resource* next;
};

struct remote_resource* remote_resource_create(const struct repo* repo);
void remote_resource_destroy(struct remote_resource* res);
struct remote_resource* remote_resource_from_repo(struct remote_resource* res,
                                                  const struct repo* repo);

#endif /* ifndef REPO_H */
