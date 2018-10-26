/*
 * @mindmaze_header@
 */

#ifndef MMPACK_INSTALL_H
#define MMPACK_INSTALL_H

#include "context.h"

#define INSTALL_SYNOPSIS \
	"install [inst-opts] <pkg1>[=<version1>] [<pkg2>[=<version2>] [...]]"

int mmpack_install(struct mmpack_ctx * ctx, int argc, const char* argv[]);

#endif /* MMPACK_INSTALL_H */

