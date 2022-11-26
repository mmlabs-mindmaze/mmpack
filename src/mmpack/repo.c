/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "repo.h"

#include "mmstring.h"
#include "utils.h"
#include "xx-alloc.h"


/**************************************************************************
 *                      repository list implementation                    *
 **************************************************************************/

static
struct repolist_elt* repolist_elt_create(const char* name)
{
	struct repolist_elt* elt;

	elt = xx_malloc(sizeof(*elt));
	*elt = (struct repolist_elt) {
		.repo = {
			.name = mmstr_malloc_from_cstr(name),
			.enabled = 1,
		},
	};

	return elt;
}


static
void repolist_elt_destroy(struct repolist_elt* elt)
{
	mmstr_free(elt->repo.url);
	mmstr_free(elt->repo.name);
	free(elt);
}


/**
 * repolist_init() - init repolist structure
 * @list: repolist structure to initialize
 *
 * To be cleansed by calling repolist_deinit()
 */
LOCAL_SYMBOL
void repolist_init(struct repolist* list)
{
	*list = (struct repolist) {0};
}


/**
 * repolist_deinit() - cleanup repolist structure
 * @list: repolist structure to cleanse
 */
LOCAL_SYMBOL
void repolist_deinit(struct repolist* list)
{
	struct repolist_elt * elt, * next;

	elt = list->head;

	while (elt) {
		next = elt->next;
		repolist_elt_destroy(elt);
		elt = next;
	}
}


/**
 * repolist_reset() - empty and reinit repolist structure
 * @list: repolist structure to reset
 */
LOCAL_SYMBOL
void repolist_reset(struct repolist* list)
{
	repolist_deinit(list);
	repolist_init(list);
}


/**
 * repolist_num_repo() - count the number of repositories in the repo list
 * @list: initialized repolist structure
 *
 * Returns: the number of repositories
 */
LOCAL_SYMBOL
int repolist_num_repo(const struct repolist* list)
{
	struct repolist_elt* elt;
	int num;

	num = 0;
	for (elt = list->head; elt; elt = elt->next)
		num++;

	return num;
}


/**
 * repolist_add() - add a repository to the list
 * @list: initialized repolist structure
 * @name: the short name referencing the url
 *
 * Returns: the repolist_elt created in case of success, NULL otherwise.
 */
LOCAL_SYMBOL
struct repo* repolist_add(struct repolist* list, const char* name)
{
	struct repolist_elt* elt;
	char default_name[16];

	// set index-based default name if name unset
	if (name == NULL) {
		sprintf(default_name, "repo-%i", repolist_num_repo(list));
		name = default_name;
	}

	// check that no repository possesses already this name
	if (repolist_lookup(list, name)) {
		error("repository \"%s\" already exists\n", name);
		return NULL;
	}

	// Insert the element at the head of the list
	elt = repolist_elt_create(name);
	elt->next = list->head;
	list->head = elt;

	return &elt->repo;
}


/**
 * repolist_lookup() - lookup a repository from the list by name
 * @list: pointer to an initialized repolist structure
 * @name: the short name referencing the url
 *
 * Return: a pointer to the repolist element on success, NULL otherwise.
 */
LOCAL_SYMBOL
struct repo* repolist_lookup(struct repolist * list, const char * name)
{
	int name_len = strlen(name);
	struct repolist_elt * elt;

	for (elt = list->head; elt != NULL; elt = elt->next) {
		if (name_len == mmstrlen(elt->repo.name)
		    && strncmp(elt->repo.name, name, name_len) == 0) {
			return &elt->repo;
		}
	}

	return NULL;
}


/**
 * repolist_remove() - remove a repository to the list
 * @list: pointer to an initialized repolist structure
 * @name: the short name referencing the url
 *
 * Return: always return 0 on success, a negative value otherwise
 */
LOCAL_SYMBOL
int repolist_remove(struct repolist * list, const char * name)
{
	int name_len = strlen(name);
	struct repolist_elt * elt;
	struct repolist_elt * prev = NULL;

	for (elt = list->head; elt != NULL; elt = elt->next) {
		if (name_len == mmstrlen(elt->repo.name)
		    && strncmp(elt->repo.name, name, name_len) == 0) {

			if (prev != NULL)
				prev->next = elt->next;
			else
				list->head = elt->next;

			repolist_elt_destroy(elt);
			return 0;
		}

		prev = elt;
	}

	return -1;
}


/**************************************************************************
 *                      remote resource implementation                    *
 **************************************************************************/

/**
 * remote_resource_create() - allocate and initialize remote resource data
 * @repo:       pointer to repository from which the data is provided
 *
 * Returns: pointer to allocated structure
 */
LOCAL_SYMBOL
struct remote_resource* remote_resource_create(const struct repo* repo)
{
	struct remote_resource* res;

	res = xx_malloc(sizeof(*res));
	*res = (struct remote_resource) {.repo = repo};

	return res;
}


/**
 * remote_resource_destroy() - cleanup remote resource data
 * @res:        pointer to remote resource data
 *
 * This cleans up pointed data and the linked one.
 */
LOCAL_SYMBOL
void remote_resource_destroy(struct remote_resource* res)
{
	struct remote_resource * elt, * next;

	elt = res;
	while (elt) {
		next = elt->next;
		mmstr_free(elt->filename);
		free(elt);
		elt = next;
	}
}


/**
 * remote_resource_from_repo() - get the remote data associated with a repo
 * @res:        pointer to remote resource data
 * @repo:       pointer to the repository for which the remote data is queried
 *
 * Returns: pointer to matching remote resource if found, NULL otherwise
 */
LOCAL_SYMBOL
struct remote_resource* remote_resource_from_repo(struct remote_resource* res,
                                                  const struct repo* repo)
{
	for (; res != NULL; res = res->next) {
		if (res->repo == repo)
			break;
	}

	return res;
}
