/*
 * @mindmaze_header@
 */

#ifndef MMPACK_CONTEXT_H
#define MMPACK_CONTEXT_H

#include <curl/curl.h>

struct mmpack_ctx {
	CURL * curl;
};

int mmpack_ctx_init(struct mmpack_ctx * ctx);
void mmpack_ctx_deinit(struct mmpack_ctx * ctx);

#endif /* MMPACK_CONTEXT_H */
