/*
 * @mindmaze_header@
 */
#ifndef SETTINGS_H
#define SETTINGS_H

#include "mmstring.h"

/**
 * Struct repolist_elt - structure representing a repository
 * @next: the next element of the list
 * @url: the url of the repository from which packages can be retrieved
 * @name: the short name referencing the url
 */
struct repolist_elt {
	struct repolist_elt* next;
	mmstr* url;
	mmstr* name;
};

struct repolist {
	struct repolist_elt* head;
};

void repolist_init(struct repolist* list);
void repolist_deinit(struct repolist* list);
void repolist_reset(struct repolist* list);
void repolist_add(struct repolist* list, const char* name, const char* url);
int repolist_remove(struct repolist * list, const char * name);
struct repolist_elt* repolist_lookup(struct repolist * list,
                                     const char * name);

struct settings {
	struct repolist repo_list;
	mmstr* default_prefix;
};

void settings_init(struct settings* settings);
void settings_deinit(struct settings* settings);
void settings_reset(struct settings* settings);
int settings_load(struct settings* settings, const char* filename);
int settings_num_repo(const struct settings* settings);
const mmstr* settings_get_repo_url(const struct settings* settings, int index);
struct repolist_elt* settings_get_repo(const struct settings* settings,
                                       int index);
int settings_serialize(const mmstr* prefix,
                       const struct settings * settings,
                       int force_create);

#endif /* SETTINGS_H */
