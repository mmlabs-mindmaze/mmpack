/*
 * @mindmaze_header@
 */
#ifndef SETTINGS_H
#define SETTINGS_H

#include "mmstring.h"

struct settings {
	mmstr* repo_url;
};

void settings_init(struct settings* settings);
void settings_deinit(struct settings* settings);
int settings_load(struct settings* settings, const char* filename);

#endif /* SETTINGS_H */
