/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <curl/curl.h>
#include <mmerrno.h>

#include "mmpack-common.h"
#include "mmpack-context.h"
#include "mmpack-config.h"
#include "mmpack-update.h"

static
int mmpack_update(char const * server, char const * url, void * arg)
{
	CURLcode res;
	struct mmpack_ctx * ctx = (struct mmpack_ctx *) arg;

	if (ctx->curl == NULL) {
		ctx->curl = curl_easy_init();
		if (ctx->curl == NULL)
			return mm_raise_from_errno("curl init failed");
	}

	curl_easy_setopt(ctx->curl, CURLOPT_URL, url);
	curl_easy_setopt(ctx->curl, CURLOPT_FOLLOWLOCATION, 1L);

	res = curl_easy_perform(ctx->curl);
	if(res != CURLE_OK)
		return mm_raise_from_errno("update from %s failed: %s",
				server, curl_easy_strerror(res));

	return 0;
}


LOCAL_SYMBOL
int mmpack_update_all(struct mmpack_ctx * ctx)
{
	return foreach_config_server(get_config_filename(), mmpack_update, ctx);
}
