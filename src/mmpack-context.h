/*
 * @mindmaze_header@
 */

#ifndef MMPACK_CONTEXT_H
#define MMPACK_CONTEXT_H

#include <curl/curl.h>
#include <yaml.h>

#include "indextable.h"

struct mmpack_ctx {
	CURL * curl;
	yaml_parser_t parser;
	struct indextable binindex;
};

int mmpack_ctx_init(struct mmpack_ctx * ctx);
void mmpack_ctx_deinit(struct mmpack_ctx * ctx);

static inline
int mmpack_ctx_is_init(struct mmpack_ctx const * ctx)
{
	return (ctx != NULL && ctx->binindex.num_buckets != 0);
}

#endif /* MMPACK_CONTEXT_H */
