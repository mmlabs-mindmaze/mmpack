/*
 * @mindmaze_header@
 */
#ifndef PATH_WIN32_H
#define PATH_WIN32_H

#include "mmstring.h"

void conv_env_pathlist_win32_to_posix(const char* envname);
char* get_relocated_path(const char* rel_path_from_executable);

#endif
