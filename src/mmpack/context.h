/*
 * @mindmaze_header@
 */

#ifndef CONTEXT_H
#define CONTEXT_H

#include <curl/curl.h>

#include "package-utils.h"
#include "settings.h"
#include "srcindex.h"
#include "strset.h"

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
 * @manually_inst: list of packages whose installation has been asked explicitly
 *                 by the user
 * @prefix:     path to the root of folder to use for prefix
 * @cwd:        path to where mmpack was invoked
 * @pkgcachedir: path to downloaded package cache folder
 */
struct mmpack_ctx {
	CURL * curl;
	char curl_errbuf[CURL_ERROR_SIZE];
	struct binindex binindex;
	struct srcindex srcindex;
	struct install_state installed;
	struct strset manually_inst;
	struct settings settings;
	mmstr* prefix;
	mmstr* cwd;
	mmstr* pkgcachedir;
};

int mmpack_ctx_init(struct mmpack_ctx * ctx, struct mmpack_opts* opts);
void mmpack_ctx_deinit(struct mmpack_ctx * ctx);
int mmpack_ctx_init_pkglist(struct mmpack_ctx * ctx);
int mmpack_ctx_use_prefix(struct mmpack_ctx * ctx, int flags);
int mmpack_ctx_save_installed_list(struct mmpack_ctx * ctx);
const mmstr* mmpack_ctx_get_pkgcachedir(struct mmpack_ctx * ctx);
mmstr* mmpack_repo_cachepath(struct mmpack_ctx* ctx, const char* repo_name,
                             const char* relpath);

static inline
int mmpack_ctx_is_init(struct mmpack_ctx const * ctx)
{
	return (ctx != NULL && ctx->prefix != NULL);
}


static inline
int mmpack_ctx_is_pkg_installed(const struct mmpack_ctx* ctx,
                                const struct binpkg* pkg)
{
	return install_state_get_pkg(&ctx->installed, pkg->name) == pkg;
}

#endif /* CONTEXT_H */
