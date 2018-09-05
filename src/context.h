/*
 * @mindmaze_header@
 */

#ifndef CONTEXT_H
#define CONTEXT_H

#include <curl/curl.h>
#include <yaml.h>

#include "indextable.h"

struct mmpack_opts {
	const char* prefix;
};

struct mmpack_ctx {
	CURL * curl;
	yaml_parser_t parser;
	struct indextable binindex;
	struct indextable installed;
	mmstr* prefix;
};

int mmpack_ctx_init(struct mmpack_ctx * ctx, struct mmpack_opts* opts);
void mmpack_ctx_deinit(struct mmpack_ctx * ctx);

static inline
int mmpack_ctx_is_init(struct mmpack_ctx const * ctx)
{
	return (ctx != NULL && ctx->binindex.num_buckets != 0);
}

#endif /* CONTEXT_H */
