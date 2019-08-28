/*
 * @mindmaze_header@
 */

#ifndef CONTEXT_H
#define CONTEXT_H

#include <curl/curl.h>

#include "indextable.h"
#include "package-utils.h"
#include "settings.h"

#define CTX_SKIP_PKGLIST 0x01
#define CTX_SKIP_REDIRECT_LOG 0x02

struct mmpack_opts {
	const char* prefix;
	const char* version;
};

/**
 * struct mmpack_ctx - context of a mmpack prefix
 * @curl:       common curl handle for reuse
 * @curl:       buffer where curl may write error message
 * @binindex:   binary index of all package available (in repo or installed)
 * @installed:  list of installed package (store in an index table)
 * @prefix:     path to the root of folder to use for prefix
 * @pkgcachedir: path to dowloaded package cache folder
 * @cacheindex: temporary string that hold the latest result of
 *              mmpack_ctx_get_cache_index()
 */
struct mmpack_ctx {
	CURL * curl;
	char curl_errbuf[CURL_ERROR_SIZE];
	struct binindex binindex;
	struct install_state installed;
	struct settings settings;
	mmstr* prefix;
	mmstr* pkgcachedir;
	mmstr* cacheindex;
};

int mmpack_ctx_init(struct mmpack_ctx * ctx, struct mmpack_opts* opts);
void mmpack_ctx_deinit(struct mmpack_ctx * ctx);
int mmpack_ctx_init_pkglist(struct mmpack_ctx * ctx);
int mmpack_ctx_use_prefix(struct mmpack_ctx * ctx, int flags);
int mmpack_ctx_save_installed_list(struct mmpack_ctx * ctx);
const mmstr* mmpack_ctx_get_pkgcachedir(struct mmpack_ctx * ctx);
const mmstr* mmpack_ctx_get_cache_index(struct mmpack_ctx * ctx,
                                        int repo_index);

static inline
int mmpack_ctx_is_init(struct mmpack_ctx const * ctx)
{
	return (ctx != NULL && ctx->prefix != NULL);
}

#endif /* CONTEXT_H */
