/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "download.h"

#include <mmsysio.h>

#include "context.h"
#include "utils.h"

static
size_t write_download_data(char* buffer, size_t size, size_t nmemb, void* data)
{
	int fd = *((int*)data);
	char* buf;
	size_t buflen;
	ssize_t rsz;

	buf = buffer;
	buflen = size*nmemb;
	while (buflen > 0) {
		rsz = mm_write(fd, buf, buflen);
		if (rsz < 0)
			break;

		buflen -= rsz;
		buf += rsz;
	}

	return (buf - buffer);
}


static
CURL* get_curl_handle(struct mmpack_ctx * ctx)
{
	if (ctx->curl == NULL) {
		ctx->curl = curl_easy_init();
		if (ctx->curl == NULL) {
			mm_raise_from_errno("curl init failed");
			return NULL;
		}

		// Common config
		curl_easy_setopt(ctx->curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(ctx->curl, CURLOPT_WRITEFUNCTION, write_download_data);
	}

	return ctx->curl;
}


LOCAL_SYMBOL
int download_from_repo(struct mmpack_ctx * ctx,
                       const mmstr* repo, const mmstr* repo_relpath,
		       const mmstr* prefix, const mmstr* prefix_relpath)
{
	mmstr* url;
	CURL* curl;
	CURLcode res;
	int fd, oflag, rv = -1;

	curl = get_curl_handle(ctx);
	if (!curl)
		return -1;

	// Form remote resource URL
	url = mmstr_malloca(mmstrlen(repo) + mmstrlen(repo_relpath) + 1);
	mmstr_join_path(url, repo, repo_relpath);

	// Open destination file
	oflag = O_WRONLY|O_CREAT|O_TRUNC;
	fd = open_file_in_prefix(prefix, prefix_relpath, oflag);
	if (fd < 0)
		goto exit;

	// Perform the download
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fd);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		mm_raise_from_errno("Failed to download %s", curl_easy_strerror(res));
		goto exit;
	}
	rv = 0;

exit:
	mm_close(fd);
	mmstr_freea(url);
	return rv;
}
