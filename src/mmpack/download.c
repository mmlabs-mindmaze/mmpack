/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "download.h"

#include <mmerrno.h>
#include <mmsysio.h>

#include "context.h"
#include "utils.h"

// On systems (Win32 that do not define EREMOTEIO, alias it to EIO
#if !defined (EREMOTEIO)
# define EREMOTEIO EIO
#endif

/**
 * get_error_from_curl() - retrieve error code and message from curl
 * @curl:               curl handle used for last transfer
 * @last_retcode:       return code of the last curl failed transfer
 * @errbuf:             error buffer set (with CURLOPT_ERRORBUFFER) for the
 *                      last transfer. This buffer acts also as output of
 *                      the function, hold the error message to print
 *
 * Return: error code to set in mm_raise_error()
 */
static
int get_error_from_curl(CURL* curl, int last_retcode, char* errbuf)
{
	long respcode, os_errno;
	int len;

	// Handle specifically when server returns HTTP error (ie >= 400)
	if (last_retcode == CURLE_HTTP_RETURNED_ERROR) {
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &respcode);
		if (respcode == 404) {
			strcpy(errbuf, "Remote resource not found");
			return MM_ENOTFOUND;
		} else {
			sprintf(errbuf, "Server replied error %li", respcode);
			return EREMOTEIO;
		}
	}

	// Fill error message from return code if not set yet
	len = strlen(errbuf);
	if (len == 0)
		strcpy(errbuf, curl_easy_strerror(last_retcode));

	// Translate curl error if not HTTP error
	switch (last_retcode) {
	case CURLE_URL_MALFORMAT:
		return MM_EBADFMT;

	case CURLE_NOT_BUILT_IN:
	case CURLE_FUNCTION_NOT_FOUND:
		return ENOSYS;

	case CURLE_COULDNT_RESOLVE_PROXY:
	case CURLE_COULDNT_RESOLVE_HOST:
		return MM_ENOTFOUND;

	case CURLE_REMOTE_ACCESS_DENIED:
	case CURLE_LOGIN_DENIED:
		return EACCES;

	default:
		curl_easy_getinfo(curl, CURLINFO_OS_ERRNO, &os_errno);
		if (os_errno == 0)
			os_errno = EREMOTEIO;

		return (int)os_errno;
	}
}


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
		curl_easy_setopt(ctx->curl, CURLOPT_FAILONERROR, 1L);
		curl_easy_setopt(ctx->curl,
		                 CURLOPT_WRITEFUNCTION,
		                 write_download_data);
		curl_easy_setopt(ctx->curl,
		                 CURLOPT_ERRORBUFFER,
		                 ctx->curl_errbuf);
	}

	return ctx->curl;
}


/**
 * download_from_repo() - download resource from specified repository
 * @ctx:        mmpack context used (used to get curl handle)
 * @repo:       URL of repository
 * @repo_relpath: path relative to @repo URL of the resource to download
 * @prefix:     folder from where to write the downloaded file (may be NULL)
 * @prefix_relpath: path relative to @prefix of the downloaded file.
 *
 * This function downloads the resource located at @repo_relpath relative to
 * the URL specified by @repo. The downloaded resource will be stored in a path
 * specified by @prefix_relpath. If @prefix is not NULL, this path is
 * interpreted relative to the folder specified by @prefix.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly
 */
LOCAL_SYMBOL
int download_from_repo(struct mmpack_ctx * ctx,
                       const mmstr* repo, const mmstr* repo_relpath,
                       const mmstr* prefix, const mmstr* prefix_relpath)
{
	mmstr* url;
	CURL* curl;
	CURLcode res;
	int fd, oflag, rv = -1;
	int err;

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
	ctx->curl_errbuf[0] = '\0';
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fd);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		err = get_error_from_curl(curl, res, ctx->curl_errbuf);
		mm_raise_error(err, "Failed to download %s (%s)",
		               url, ctx->curl_errbuf);
		goto exit;
	}

	rv = 0;

exit:
	mm_close(fd);
	mmstr_freea(url);
	return rv;
}
