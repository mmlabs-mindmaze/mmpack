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

#include <mmerrno.h>
#include <mmlib.h>

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
	os_id id = OS_IS_UNKNOWN;

	stream = popen(OS_ID_CMD, "r");

	nread = getline(&line, &len, stream);
	if (nread == -1)
		goto exit;

	if (strncasecmp(line, "ubuntu", len)
			||  strncasecmp(line, "debian", len))
		id = OS_ID_DEBIAN;

exit:
	fclose(stream);
	free(line);
	return id;
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


static
char const * get_default_path(enum mm_known_dir dirtype,
                              char const * default_filename)
{
	char * filename;
	size_t filename_len;

	char const * xdg_home = mm_get_basedir(dirtype);
	if (xdg_home == NULL)
		return NULL;

	filename_len = strlen(xdg_home) + strlen(default_filename) + 2;
	filename = malloc(filename_len);
	if (filename == NULL)
		return NULL;

	filename[0] = '\0';
	strcat(filename, xdg_home);
	strcat(filename, "/");
	strcat(filename, default_filename);

	return filename;
}


LOCAL_SYMBOL
char const * get_local_mmpack_binary_index_path(void)
{
	return get_default_path(MM_DATA_HOME, "mmpack-binindex.yaml");
}


LOCAL_SYMBOL
char const * get_mmpack_installed_pkg_path(void)
{
	return get_default_path(MM_DATA_HOME, "mmpack-installed.yaml");
}


LOCAL_SYMBOL
char const * get_config_filename(void)
{
	return get_default_path(MM_CONFIG_HOME, "mmpack-config.yaml");
}
