/*
 * @mindmaze_header@
 */
#ifndef SETTINGS_H
#define SETTINGS_H

#include "utils.h"
#include "mmstring.h"

struct settings {
	struct strlist repo_list;
	mmstr* default_prefix;
};

void settings_init(struct settings* settings);
void settings_deinit(struct settings* settings);
int settings_load(struct settings* settings, const char* filename);
int settings_num_repo(const struct settings* settings);
const mmstr* settings_get_repo_url(const struct settings* settings, int index);

#endif /* SETTINGS_H */
