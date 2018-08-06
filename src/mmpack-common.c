/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mmpack-common.h"


#if defined(_WIN32)
LOCAL_SYMBOL
os_id get_os_id(void)
{
	return OS_ID_WINDOWS_10;
}
#elif defined( __linux)
#define OS_ID_CMD "grep '^ID=' /etc/os-release | cut -f2- -d= | sed -e 's/\"//g'"
LOCAL_SYMBOL
os_id get_os_id(void)
{
	FILE * stream;
	char * line = NULL;
	size_t len = 0;
	ssize_t nread;

	stream = popen(OS_ID_CMD, "r");

	nread = getline(&line, &len, stream);
	fclose(stream);
	free(line);
	if (nread == -1)
		return OS_IS_UNKNOWN;

	if (strncasecmp(line, "ubuntu", len)
			||  strncasecmp(line, "debian", len))
		return OS_ID_DEBIAN;

	return OS_IS_UNKNOWN;
}
#else /* !win32 && !linux */
LOCAL_SYMBOL
os_id get_os_id(void)
{
	return OS_IS_UNKNOWN;
}
#endif

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
