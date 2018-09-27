/*
 * @mindmaze_header@
 */

#ifndef CONTEXT_H
#define CONTEXT_H

#include <curl/curl.h>

#include "indextable.h"
#include "package-utils.h"
#include "settings.h"

struct mmpack_opts {
	const char* prefix;
};

/**
 * struct mmpack_ctx - context of a mmpack prefix
 * @curl:       common curl handle for reuse
 * @binindex:   binary index of all package available (in repo or installed)
 * @installed:  list of installed package (store in an index table)
 * @prefix:     path to the root of folder to use for prefix
 * @pkgcachedir: path to dowloaded package cache folder
 */
struct mmpack_ctx {
	CURL * curl;
	struct binindex binindex;
	struct install_state installed;
	struct settings settings;
	mmstr* prefix;
	mmstr* pkgcachedir;
};

int mmpack_ctx_init(struct mmpack_ctx * ctx, struct mmpack_opts* opts);
void mmpack_ctx_deinit(struct mmpack_ctx * ctx);
int mmpack_ctx_init_pkglist(struct mmpack_ctx * ctx);
int mmpack_ctx_save_installed_list(struct mmpack_ctx * ctx);
const mmstr* mmpack_ctx_get_pkgcachedir(struct mmpack_ctx * ctx);

static inline
int mmpack_ctx_is_init(struct mmpack_ctx const * ctx)
{
	return (ctx != NULL && ctx->prefix != NULL);
}

#endif /* CONTEXT_H */
