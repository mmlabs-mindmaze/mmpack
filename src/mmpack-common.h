/*
 * @mindmaze_header@
 */

#ifndef MMPACK_COMMON_H
#define MMPACK_COMMON_H

#include <curl/curl.h>


#define STR_EQUAL(str, len, const_str) \
	(len == (sizeof(const_str) - 1) \
	 && memcmp(str, const_str, sizeof(const_str) - 1) == 0)

struct mmpack_ctx {
	CURL * curl;
};


int mmpack_ctx_init(struct mmpack_ctx * ctx);
void mmpack_ctx_deinit(struct mmpack_ctx * ctx);


#endif /* MMPACK_COMMON_H */
