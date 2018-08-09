/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmlib.h>
#include <mmsysio.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mmerrno.h>
#include <mmlib.h>

#include "mmpack-common.h"
#include "mmpack-context.h"

LOCAL_SYMBOL
int mmpack_ctx_init(struct mmpack_ctx * ctx)
{
	memset(ctx, 0, sizeof(*ctx));

	return 0;
}


LOCAL_SYMBOL
void mmpack_ctx_deinit(struct mmpack_ctx * ctx)
{
	if (ctx->curl != NULL)
		curl_easy_cleanup(ctx->curl);
	ctx->curl = NULL;
}
