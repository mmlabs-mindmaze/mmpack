/*
 * @mindmaze_header@
 */
#ifndef SETTINGS_H
#define SETTINGS_H

#include "mmstring.h"
#include "repo.h"

int create_empty_index_files(const mmstr* prefix, char const * name);
int create_initial_index_files(const mmstr* prefix, struct repolist* repos);

struct settings {
	struct repolist repo_list;
	mmstr* default_prefix;
	int disable_import_other;
};

void settings_init(struct settings* settings);
void settings_deinit(struct settings* settings);
void settings_reset(struct settings* settings);
int settings_load(struct settings* settings, const char* filename);
int settings_serialize(const mmstr* prefix,
                       const struct settings * settings,
                       int force_create);

#endif /* SETTINGS_H */
