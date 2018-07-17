/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <curl/curl.h>

#include "mmpack-config.h"
#include "mmpack-update.h"

/*
 * TODO: lazy curl init, ond once ...
 */
static
int mmpack_update(char const * server, char const * url)
{
	CURL *curl;
	CURLcode res;

	(void) server; /* TODO: user server name in error message */

	curl = curl_easy_init();
	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

		res = curl_easy_perform(curl);
		if(res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n",
					curl_easy_strerror(res));

		curl_easy_cleanup(curl);
	}

	return 0;
}


LOCAL_SYMBOL
int mmpack_update_all(void)
{
	return foreach_config_server(get_config_filename(), mmpack_update);
}
