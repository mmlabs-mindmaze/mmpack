/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "download.h"

#include <stdio.h>
#include <mmerrno.h>
#include <mmsysio.h>

#include "context.h"
#include "repo.h"
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


static
int is_resource_in_cache(struct mmpack_ctx * ctx,
                         const struct remote_resource* res,
                         mmstr** downloaded_file)
{
	const struct remote_resource* from;
	int previous;
	int available = 0;
	mmstr* filename = *downloaded_file;
	const mmstr* cachedir = mmpack_ctx_get_pkgcachedir(ctx);

	// Set available if a valid package already is available in cache
	previous = mm_error_set_flags(MM_ERROR_SET, MM_ERROR_IGNORE);
	for (from = res; from != NULL && !available; from = from->next) {
		filename = mmstr_join_path_realloc(filename,
		                                   cachedir, from->sha256);
		if (check_hash(from->sha256, NULL, filename) == 0) {
			available = 1;
			break;
		}
	}
	mm_error_set_flags(previous, MM_ERROR_IGNORE);

	*downloaded_file = filename;
	return available;
}


/**
 * download_remote_resource() - get a remote resource
 * @ctx:        mmpack context used (used to get curl handle)
 * @res:        remote resource to fetch
 * @downloaded_file: pointer to mmstr that will receive the path to cached file
 *
 * This functions tries alternatively all remote repository and settle once
 * remote file has been successfully downloaded.
 *
 * Returns: 0 in case of success, -1 otherwise
 */
LOCAL_SYMBOL
int download_remote_resource(struct mmpack_ctx * ctx,
                             const struct remote_resource* res,
                             mmstr** downloaded_file)
{
	const struct remote_resource* from;
	const struct repo* repo;
	const mmstr* cachedir = mmpack_ctx_get_pkgcachedir(ctx);
	mmstr* filename = *downloaded_file;
	int previous;
	int done = 0;
	int rv = -1;
	struct mm_timespec ts[2] = {
		{.tv_sec = UTIME_NOW},
		{.tv_sec = UTIME_OMIT}
	};

	if (is_resource_in_cache(ctx, res, downloaded_file)) {
		mm_utimens(*downloaded_file, ts, 0);
		return 0;
	}

	// Stop logging error (still error are reported in state)
	previous = mm_error_set_flags(MM_ERROR_SET, MM_ERROR_NOLOG);

	// Download file from one of the repo in the cache
	for (from = res; from != NULL && !done; from = from->next) {
		repo = res->repo;
		if (!repo) {
			// Package has been provided on command line
			filename = mmstrcpy_realloc(filename, from->filename);
			done = 1;
			break;
		}
		printf("download %s from %s... ", from->filename, repo->url);
		filename = mmstr_join_path_realloc(filename,
		                                   cachedir, from->sha256);
		rv = download_from_repo(ctx, repo->url, from->filename,
		                        NULL, filename);
		printf("%s\n", rv ? "failed" : "ok");

		// Check previous download operation result and hash. If both
		// are success, then we are done here
		if (rv == 0 && check_hash(from->sha256, NULL, filename) == 0)
			done = 1;
	}

	// Restore error logging behavior
	mm_error_set_flags(previous, MM_ERROR_NOLOG);

	*downloaded_file = filename;
	return done ? 0 : -1;
}
