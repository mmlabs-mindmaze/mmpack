/*
 * @mindmaze_header@
 */

#ifndef MMPACK_DOWNLOAD_H
#define MMPACK_DOWNLOAD_H

#include "context.h"

#define DOWNLOAD_SYNOPSIS \
	"download <pkg1>[=<version1>]"

int mmpack_download(struct mmpack_ctx * ctx, int argc, const char* argv[]);

#endif /* MMPACK_DOWNLOAD_H */

