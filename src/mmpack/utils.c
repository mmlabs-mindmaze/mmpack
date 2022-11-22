/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmargparse.h>
#include <mmerrno.h>
#include <mmlib.h>
#include <mmlog.h>
#include <mmsysio.h>
#include <nettle/sha2.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "buffer.h"
#include "common.h"
#include "mmstring.h"
#include "utils.h"
#include "xx-alloc.h"

#define MSG_MAXLEN 128
#define HASH_UPDATE_SIZE 512
#define BLK_SIZE 512

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif


/**************************************************************************
 *                                                                        *
 *                      Parse pathname components                         *
 *                                                                        *
 **************************************************************************/
static
const char* get_last_nonsep_ptr(const mmstr* path)
{
	const char* c = path + mmstrlen(path) - 1;

	// skip trailing path separators
	while (c > path && is_path_separator(*c))
		c--;

	return c;
}


static
const char* get_basename_ptr(const mmstr* path)
{
	const char * c, * lastptr;

	lastptr = get_last_nonsep_ptr(path);

	for (c = lastptr-1; c >= path; c--) {
		if (is_path_separator(*c))
			return (c == lastptr) ? c : c + 1;
	}

	return path;
}


/**
 * mmstr_basename() - get basename of a path
 * @basepath:   mmstr receiving the result
 * @path:       path whose basename must be computed
 *
 * NOTE: If maximum length of @basepath is greater or equal to length of @path,
 * then it is guaranteed that the result will not overflow @basepath.
 *
 * Return:
 * the pointer to @basepath or the reallocated mmstr pointer if its maximum
 * length was not large enough.
 */
LOCAL_SYMBOL
mmstr* mmstr_basename(mmstr* restrict basepath, const mmstr* restrict path)
{
	const char* baseptr;
	const char* lastptr;
	int len;

	baseptr = get_basename_ptr(path);
	lastptr = get_last_nonsep_ptr(path)+1;

	len = lastptr - baseptr;

	// if len == 0, path was "/" (or "//" or "///" or ...)
	if (len <= 0)
		len = 1;

	return mmstr_copy_realloc(basepath, baseptr, len);
}


/**
 * mmstr_dirname() - get directory name of a path
 * @dirpath:    mmstr receiving the result
 * @path:       path whose dirname must be computed
 *
 * NOTE: If maximum length of @dirpath is greater or equal to length of @path,
 * then it is guaranteed that the result will not overflow @dirpath.
 *
 * the pointer to @dirpath or the reallocated mmstr pointer if its maximum
 * length was not large enough.
 */
LOCAL_SYMBOL
mmstr* mmstr_dirname(mmstr* restrict dirpath, const mmstr* restrict path)
{
	const char* baseptr;
	const char* lastptr;

	baseptr = get_basename_ptr(path);

	if (baseptr == path) {
		if (is_path_separator(*baseptr))
			return mmstrcpy_cstr(dirpath, "/");

		return mmstrcpy_cstr(dirpath, ".");
	}

	lastptr = baseptr-1;

	// Remove trailing separator
	while (lastptr > path && is_path_separator(*lastptr))
		lastptr--;

	return mmstr_copy_realloc(dirpath, path, lastptr - path + 1);
}


static
int is_absolute_path(const mmstr * p)
{
#if defined (_WIN32)
	if (is_path_separator(p[0]))
		return 1;

	/* test for paths beginning with drive letters
	 * - first letter is a char
	 * - test for paths in windows format (X:\\)
	 * - test for paths in mixed format (X:/)
	 */
	return (isalpha(*p)
	        && (STR_EQUAL(p + 1, 3, ":\\\\")
	            || STR_EQUAL(p + 1, 2, ":/")));
#else
	return is_path_separator(p[0]);
#endif
}

/**
 * mmstr_join_path() - Join 2 path components intelligently
 * @dst:        mmstr receiving the result (its maxlen must be large enough)
 * @p1:         first path component
 * @p2:         second path component
 *
 * This function concatenate @p1 and @p2 with exactly one directory separator
 * (platform specific) between the 2 components.
 *
 * NOTE: If maximum length of @dst is greater or equal to length of @p1 plus
 * length of @p2 plus one, then it is guaranteed that the result will not
 * overflow @dst.
 *
 * Return: the pointer to @dst.
 */
LOCAL_SYMBOL
mmstr* mmstr_join_path(mmstr* restrict dst,
                       const mmstr* restrict p1, const mmstr* restrict p2)
{
	if (is_absolute_path(p2))
		return mmstrcpy(dst, p2);

	mmstrcpy(dst, p1);

	/* if p1 does not end with a '/', add it */
	if (!is_path_separator(p1[mmstrlen(p1)-1]))
		mmstrcat_cstr(dst, "/");

	return mmstrcat(dst, p2);
}


/**
 * mmstr_join_path() - Join 2 path components and realloc destination if needed
 * @dst:        mmstr receiving the result
 * @p1:         first path component
 * @p2:         second path component
 *
 * This function concatenate @p1 and @p2 with exactly one directory separator
 * (platform specific) between the 2 components.
 *
 * Return: the pointer to @dst.
 */
mmstr* mmstr_join_path_realloc(mmstr* restrict dst,
                               const mmstr* restrict p1,
                               const mmstr* restrict p2)
{
	if (is_absolute_path(p2))
		return mmstrcpy_realloc(dst, p2);

	dst = mmstr_realloc(dst, mmstrlen(p1) + mmstrlen(p2) + 1);

	mmstrcpy(dst, p1);

	/* if p1 does not end with a '/', add it */
	if (!is_path_separator(p1[mmstrlen(p1)-1])) {
		mmstrcat_cstr(dst, "/");
	}

	return mmstrcat(dst, p2);
}


/**************************************************************************
 *                                                                        *
 *                    File manipulation in prefix                         *
 *                                                                        *
 **************************************************************************/

/**
 * open_file_in_prefix() - open file in specified folder
 * @prefix:     folder from where to open the file (may be NULL)
 * @path:       path relative to @prefix of the file to open
 * @oflag:      control flags how to open the file (same as mm_open())
 *
 * This function opens a file descriptor for file located at @path
 * relatively to a folder specified by @prefix if not NULL. @oflag are the
 * same that can be passed to mm_open().
 *
 * If file may be created (ie @oflag contains O_CREAT), and the parent dir
 * do not exist, the parent dir will be created as well (and recursively)
 *
 * Return: a non-negative integer representing the file descriptor in case
 * of success. Otherwise -1 is returned with error state set accordingly.
 */
LOCAL_SYMBOL
int open_file_in_prefix(const mmstr* prefix, const mmstr* path, int oflag)
{
	int fd = -1;
	mmstr * tmp = NULL;
	mmstr * dirpath = NULL;

	if (prefix) {
		// Form path of file in prefix
		tmp = mmstr_malloca(mmstrlen(prefix) + mmstrlen(path) + 1);
		path = mmstr_join_path(tmp, prefix, path);
	}

	// If file may have to be created, try create parent dir if needed
	if (oflag & O_CREAT) {
		dirpath = mmstr_malloca(mmstrlen(path));
		mmstr_dirname(dirpath, path);
		if (mm_mkdir(dirpath, 0777, MM_RECURSIVE)) {
			fprintf(stderr, "Create parent dir of %s failed: %s\n",
			        path, mm_strerror(mm_get_lasterror_number()));
			goto exit;
		}
	}

	// Create file
	fd = mm_open(path, oflag, 0666);
	if (fd < 0)
		fprintf(stderr, "Failed to open %s: %s\n",
		        path, mm_strerror(mm_get_lasterror_number()));

exit:

	mmstr_freea(dirpath);
	mmstr_freea(tmp);

	return fd;
}


/**
 * map_file_in_prefix() - map file from a prefix in memory
 * @prefix:     folder from where to open the file (may be NULL)
 * @relpath:    path relative to @prefix of the file to open
 * @map:        pointer to void* receiving the address of mapped data
 * @len:        pointer to size_t variable receiving the length of mapped data
 *
 * This function maps the content of the file located at @relpath relatively to
 * a folder specified by @prefix if not NULL. The base address of the mapped
 * memory and its length will be set into the values pointed respectively by
 * @map and @len.
 *
 * When the mapping is no longer needed, it must be unmapped with mm_unmap().
 *
 * Return: 0 in case of success. Otherwise -1 is returned with error state set
 * accordingly.
 */
LOCAL_SYMBOL
int map_file_in_prefix(const mmstr* prefix, const mmstr* relpath,
                       void** map, size_t* len)
{
	struct mm_stat buf;
	int fd;

	fd = open_file_in_prefix(prefix, relpath, O_RDONLY);
	if (fd == -1)
		return -1;

	mm_fstat(fd, &buf);
	*len = buf.size;

	if (buf.size) {
		*map = mm_mapfile(fd, 0, buf.size, MM_MAP_READ);
		mm_check(*map != NULL);
	} else {
		*map = NULL;
	}

	mm_close(fd);
	return 0;
}


/**************************************************************************
 *                                                                        *
 *                            Host OS detection                           *
 *                                                                        *
 **************************************************************************/

#if defined (_WIN32)
LOCAL_SYMBOL
os_id get_os_id(void)
{
	return OS_ID_WINDOWS_10;
}
#elif defined (__linux)
#define OS_ID_CMD \
	"grep '^ID=' /etc/os-release | cut -f2- -d= | sed -e 's/\"//g'"
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
	    || strncasecmp(line, "linuxmint", len)
	    || strncasecmp(line, "raspbian", len)
	    || strncasecmp(line, "debian", len))
		id = OS_ID_DEBIAN;

exit:
	pclose(stream);
	free(line);
	return id;
}
#else /* !win32 && !linux */
LOCAL_SYMBOL
os_id get_os_id(void)
{
	return OS_IS_UNKNOWN;
}
#endif /* if defined (_WIN32) */


/**************************************************************************
 *                                                                        *
 *                               Default paths                            *
 *                                                                        *
 **************************************************************************/
LOCAL_SYMBOL
mmstr* get_xdg_subpath(enum mm_known_dir dirtype, const char* subdir)
{
	mmstr* filename;
	size_t filename_len;

	char const * xdg_dir = mm_get_basedir(dirtype);
	if (xdg_dir == NULL)
		return NULL;

	filename_len = strlen(xdg_dir) + strlen(subdir) + 1;
	filename = mmstr_malloc(filename_len);

	mmstrcat_cstr(filename, xdg_dir);
	mmstrcat_cstr(filename, "/");
	mmstrcat_cstr(filename, subdir);

	return filename;
}


/**************************************************************************
 *                                                                        *
 *                            SHA computation helper                      *
 *                                                                        *
 **************************************************************************/
/**
 * conv_to_hexstr() - convert byte array into hexadecimal string
 * @hexstr:     output string, must be (2*@len) long
 * @data:       byte array to convert
 * @len:        length of @data
 *
 * This function generates the hexadecimal string representation of a byte
 * array.
 *
 * Return: length of the string written in @hexstr
 */
static
int conv_to_hexstr(char* hexstr, const unsigned char* data, size_t len)
{
	const char hexlut[] = "0123456789abcdef";
	unsigned char d;
	size_t i;

	for (i = 0; i < len; i++) {
		d = data[i];
		hexstr[2*i + 0] = hexlut[(d >> 4) & 0x0F];
		hexstr[2*i + 1] = hexlut[(d >> 0) & 0x0F];
	}

	return 2*len;
}


/**
 * sha_fd_compute() - compute SHA256 hash of an open file
 * @hash:       mmstr* buffer receiving the hexadecimal form of hash. The
 *              pointed buffer must be SHA_HEXSTR_LEN long.
 * @fd:         file descriptor of a file opened for reading
 *
 * The computed hash is stored in hexadecimal as a mmstr* string in @hash whose
 * length must be at least SHA_HEXSTR_LEN long, as reported by mmstr_maxlen().
 *
 * Return: 0 in case of success, -1 if a problem of file reading has been
 * encountered.
 */
static
int sha_fd_compute(char* hash, int fd)
{
	uint8_t md[SHA256_DIGEST_SIZE], data[HASH_UPDATE_SIZE];
	struct sha256_ctx ctx;
	ssize_t rsz;
	int rv = 0;

	sha256_init(&ctx);

	do {
		rsz = mm_read(fd, data, sizeof(data));
		if (rsz < 0) {
			rv = -1;
			break;
		}

		sha256_update(&ctx, rsz, data);
	} while (rsz > 0);

	sha256_digest(&ctx, sizeof(md), md);

	conv_to_hexstr(hash, md, sizeof(md));

	return rv;
}


static
int sha_regfile_compute(mmstr* hash, const char* path, int with_prefix)
{
	int fd;
	int rv = 0;
	char* hexstr;

	// If with_prefix is set, SHA_HDR_REG ("reg-") is prefixed in hash
	if (with_prefix) {
		memcpy(hash, SHA_HDR_REG, SHA_HDRLEN);
		hexstr = hash + SHA_HDRLEN;
		mmstr_setlen(hash, SHA_HEXSTR_LEN);
	} else {
		hexstr = hash;
		mmstr_setlen(hash, SHA_HEXSTR_LEN - SHA_HDRLEN);
	}

	if ((fd = mm_open(path, O_RDONLY, 0)) < 0
	    || sha_fd_compute(hexstr, fd)) {
		rv = -1;
	}

	mm_close(fd);

	return rv;
}


static
int sha_symlink_compute(mmstr* hash, const char* path, size_t target_size)
{
	uint8_t md[SHA256_DIGEST_SIZE];
	struct sha256_ctx ctx;
	char* buff;
	int len;
	int rv = -1;

	buff = xx_malloca(target_size);
	if (mm_readlink(path, buff, target_size))
		goto exit;

	sha256_init(&ctx);
	sha256_update(&ctx, target_size-1, (uint8_t*)buff);
	sha256_digest(&ctx, sizeof(md), md);

	// Convert sha into hexadecimal and with "sym-" prefixed
	memcpy(hash, SHA_HDR_SYM, SHA_HDRLEN);
	len = conv_to_hexstr(hash + SHA_HDRLEN, md, sizeof(md));
	mmstr_setlen(hash, len + SHA_HDRLEN);

	rv = 0;

exit:
	mm_freea(buff);
	return rv;
}


/**
 * sha_compute() - compute SHA256 hash on specified file
 * @hash:       mmstr* buffer receiving the hexadecimal form of hash. The
 *              pointed string must be at least HASH_HEXSTR_LEN long.
 * @filename:   path of file whose hash must be computed
 * @follow:     if set to non zero and the file is a symlink, the hash is
 *              computed on the file it refers to (ie the symlink is
 *              followed). If set to zero the generated hash is prefixed
 *              by file type indicator (regular file or symlink).
 *
 * This function allows to compute the SHA256 hash of a file located at
 * @filename.
 *
 * The computed hash is stored in hexadecimal as a NULL terminated string
 * in @hash which must be at least SHA_HEXSTR_LEN long (this include the
 * NULL termination).
 *
 * If @follow is zero, hash string written in @hash is prefixed with the
 * type ("reg-" or "sym-"). The constraints on the size of the buffer
 * pointed to by @hash do not changes (SHA_HEXSTR_LEN takes into account
 * the possible type prefix).
 *
 * Return: 0 in case of success, -1 if a problem of file reading has been
 * encountered.
 */
LOCAL_SYMBOL
int sha_compute(mmstr* hash, const mmstr* filename, int follow)
{
	int rv = 0;
	int needed_len, with_prefix;
	struct mm_stat st;

	with_prefix = !follow;

	needed_len = SHA_HEXSTR_LEN - SHA_HDRLEN;
	needed_len += with_prefix ? SHA_HDRLEN : 0;
	if (mmstr_maxlen(hash) < needed_len)
		return mm_raise_error(EOVERFLOW, "hash argument to short");

	if (mm_stat(filename, &st, follow ? 0 : MM_NOFOLLOW)) {
		rv = -1;
		goto exit;
	}

	if (S_ISREG(st.mode)) {
		rv = sha_regfile_compute(hash, filename, with_prefix);
	} else if (S_ISLNK(st.mode)) {
		rv = sha_symlink_compute(hash, filename, st.size);
	} else {
		rv = mm_raise_error(EINVAL, "%s is neither a regular file "
		                    "or symlink", filename);
	}

exit:
	if (rv == -1)
		mm_log_error("Cannot compute SHA-256 of %s", filename);

	return rv;
}


/**
 * check_hash() - Check integrity of given file
 * @ref_sha: reference file sha256 to compare against
 * @filename: path of file whose hash must be computed
 *
 * Return: 0 if no issue has been found, -1 otherwise
 */
LOCAL_SYMBOL
int check_hash(const mmstr* refsha, const mmstr* filename)
{
	int follow;
	mmstr* sha = mmstr_alloca(SHA_HEXSTR_LEN);

	// If reference hash contains type prefix (ie its length is
	// SHA_HEXSTR_LEN), symlink must not be followed
	follow = 0;
	if (mmstrlen(refsha) != SHA_HEXSTR_LEN)
		follow = 1;

	if (sha_compute(sha, filename, follow))
		return -1;

	if (!mmstrequal(sha, refsha)) {
		mm_raise_error(EBADMSG, "bad SHA-256 detected %s", filename);
		return -1;
	}

	return 0;
}


/**************************************************************************
 *                                                                        *
 *                            String helpers                              *
 *                                                                        *
 **************************************************************************/

/**
 * strchr_or_end() - gives a pointer to the first occurrence of a character on
 *                   a string.
 * s: the string inspected
 * c: the character searched for in the string s
 *
 * Return: a pointer to the first occurrence of a character on a string. If the
 * character c is not found in s, then the pointer returned corresponds to the
 * end of the string s.
 */
LOCAL_SYMBOL
char* strchr_or_end(const char * s, int c)
{
	char * curr = (char*) s;

	while (*curr != c && *curr != '\0')
		curr++;

	return curr;
}


static
int find_break_pos(struct strchunk in, int len)
{
	int i;

	if (len >= in.len)
		return in.len;

	// Try to find a break between 0.66*len and len
	for (i = len-1; i > 0; i--) {
		if (in.buf[i] == ' ' || in.buf[i] == '\n')
			return i;
	}

	for (i = len+1; i > in.len; i++) {
		if (in.buf[i] == ' ' || in.buf[i] == '\n')
			break;
	}

	return i;
}


/**
 * linewrap_string() - wrap a single paragraph line in several lines
 * @out:        mmstr mpoint to which the wrapped text must be appended
 * @in:         input in strchunk
 * @len:        maximal length of each line (excluding indentation)
 * @indent_str: string to preprend to each line after a break
 *
 * Returns: the @out string with wrapped line append to it, possibly
 * reallocated if available space was not sufficient.
 */
LOCAL_SYMBOL
mmstr* linewrap_string(mmstr* restrict out, struct strchunk in,
                       int len, const char* indent_str)
{
	mmstr* indent = mmstr_alloca_from_cstr(indent_str);
	mmstr* wrapped = out;
	struct strchunk line;
	int i;

	// Add header and initial line
	i = find_break_pos(in, len);
	line = strchunk_lpart(in, i+1);
	in = strchunk_rpart(in, i);
	wrapped = mmstr_append_realloc(out, line.buf, line.len);

	// Add subsequent wrapped line
	while (in.len) {
		wrapped = mmstr_append_realloc(wrapped, "\n", 1);
		wrapped = mmstrcat_realloc(wrapped, indent);
		i = find_break_pos(in, len);
		line = strchunk_lpart(in, i+1);
		in = strchunk_rpart(in, i);
		wrapped = mmstr_append_realloc(wrapped, line.buf, line.len);
	}

	return wrapped;
}


/**
 * textwrap_string() - wrap a several paragraph line in several lines
 * @out:        mmstr mpoint to which the wrapped text must be appended
 * @in:         input in strchunk
 * @len:        maximal length of each line (excluding indentation)
 * @indent_str: string to preprend to each line after a break
 * @nl_seq:     the string to be use to replace each newline character on input
 *
 * Similar to linewrap_string() excepting that it can accommodate input with
 * newline. Each occurrence of newline is replaced by @nl_seq, and indentation
 * is inserted before resuming non newline character processing.
 *
 * Returns: the @out string with wrapped line append to it, possibly
 * reallocated if available space was not sufficient.
 */
LOCAL_SYMBOL
mmstr* textwrap_string(mmstr* restrict out, struct strchunk in,
                       int len, const char* indent_str, const char* nl_seq)
{
	struct strchunk line;
	mmstr* wrapped = out;
	mmstr* seq = mmstr_alloca_from_cstr(nl_seq);
	int pos;

	while (in.len) {
		// Copy text up the first newline
		pos = strchunk_find(in, '\n');
		line = strchunk_lpart(in, pos);
		in = strchunk_rpart(in, pos-1);
		wrapped = linewrap_string(wrapped, line, len, indent_str);

		// Replace all consecutive newline character by nl_seq
		while (in.len && in.buf[0] == '\n') {
			wrapped = mmstrcat_realloc(wrapped, seq);
			in.len--;
			in.buf++;
		}

		// Add indentation if text block continue
		if (in.len) {
			wrapped = mmstr_append_realloc(wrapped, "\n", 1);
			wrapped = mmstr_append_realloc(wrapped, indent_str,
			                               strlen(indent_str));
		}
	}

	return wrapped;
}


/**
 * mmstr_asprintf() - formatted output to string
 * @dst:        destination string (may be NULL)
 * @fmt:        printf-like format specifier
 *
 * This implements sprintf function targeting mmstr.
 *
 * NOTE: the performance of the function is not high, don't use it in the
 * hotpath of the codebase.
 *
 * Returns: @dst or reallocated @dst to accommodate the data.
 */
LOCAL_SYMBOL
mmstr* mmstr_asprintf(mmstr* restrict dst, const char* restrict fmt, ...)
{
	va_list args;
	int len;

	// Compute needed size
	va_start(args, fmt);
	len = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	dst = mmstr_realloc(dst, len);

	// Actual string formatting
	va_start(args, fmt);
	len = vsprintf(dst, fmt, args);
	va_end(args);

	mmstr_setlen(dst, len);

	return dst;
}


/**************************************************************************
 *                                                                        *
 *                            user/log interaction                        *
 *                                                                        *
 **************************************************************************/

/**
 * prompt_user_confirm() - interactively ask user for confirmation
 *
 * Interpret invalid or empty answer as denial.
 *
 * Return: 0 on acceptance, -1 otherwise
 */
LOCAL_SYMBOL
int prompt_user_confirm(void)
{
	int rv;
	char line[64];
	char answer;

	// check the user can enter its answer
	if (mm_isatty(0) != 1) {
		printf("Current command requires confirmation\n"
		       "Run again with --assume-yes flag set to proceed\n");
		return -1;
	}

	printf("Do you want to proceed? [y/N] ");
	if (fgets(line, sizeof(line), stdin) == NULL)
		return -1;

	rv = sscanf(line, "%c\n", &answer);
	if (rv != 0 && rv != EOF && answer == 'y')
		return 0;

	return -1;
}


/**
 * report_user_and_log() - print message to user and log it
 * @mm_log_level:        mmlog level to set to the logged message
 * @fmt:                printf-like format string
 *
 * This function format a message duplicates it both to standard output for
 * the user and in the log (through mmlog which will decorate the message and
 * send it to standard error).
 *
 * The message may or may not be terminated by a linefeed character. If
 * there is one, the message sent to log (mmlog facility) will be anyway
 * stripped from it terminating LF. This behavior allows sending sending
 * multipart message to user, while the log contains only self-contained
 * one-liners.
 */
LOCAL_SYMBOL
void report_user_and_log(int mm_log_level, const char* fmt, ...)
{
	char msg[MSG_MAXLEN+2];
	int lastchar_idx, msglen, has_lf;
	va_list ap;

	// If command completion is running, do not produce anything on
	// standard output or standard error
	if (mm_arg_is_completing())
		return;

	va_start(ap, fmt);
	msglen = vsnprintf(msg, MSG_MAXLEN+1, fmt, ap);
	va_end(ap);

	// Ensure msglen represents the length of buffer in msg even in
	// case of truncation or error
	msglen = MAX(msglen, 0);
	msglen = MIN(msglen, MSG_MAXLEN);

	// See if there was a trailing linefeed and remove it temporary if
	// so (because mmlog assume one-liner be added in log)
	lastchar_idx = (msglen == 0) ? 0 : msglen - 1;
	has_lf = (msg[lastchar_idx] == '\n');
	if (has_lf)
		msg[lastchar_idx] = '\0';

	// log message with mmlog (to STDERR)
	mm_log(mm_log_level, PACKAGE_NAME, msg);

	// restore trailing linefeed if there was one
	if (has_lf)
		msg[lastchar_idx] = '\n';

	// Write message to standard output
	fwrite(msg, 1, msglen, stdout);
	fflush(stdout);
}


/**************************************************************************
 *                                                                        *
 *                        External cmd execution                          *
 *                                                                        *
 **************************************************************************/

/**
 * execute_cmd() - execute a external command
 * @argv:       array of arg to pass to command including program (argv[0])
 *
 * Return: the exit code of the called command in case of success, -1 in
 * case of failure
 */
LOCAL_SYMBOL
int execute_cmd(char* argv[])
{
	int status;
	mm_pid_t pid;

	if (mm_spawn(&pid, argv[0], 0, NULL, 0, argv, NULL)
	    || mm_wait_process(pid, &status))
		return -1;

	if (MM_WSTATUS_SIGNALED & status)
		return mm_raise_error(EINTR, "Command %s failed", argv[0]);

	return status & MM_WSTATUS_CODEMASK;
}


/**
 * execute_cmd_capture_output() - execute a command and capture its output
 * @argv:       array of arg to pass to command including program (argv[0])
 * @output:     pointer to struct buffer that will hold the output of command
 *
 * Return: the exit code of the called command in case of success, -1 in
 * case of failure
 */
LOCAL_SYMBOL
int execute_cmd_capture_output(char* argv[], struct buffer* output)
{
	struct mm_remap_fd fdmap;
	int pipe_fds[2];
	void* data;
	ssize_t rsz;
	int rv = -1;
	int status;
	mm_pid_t pid;

	// Execute external command with STDOUT connected to a pipe
	mm_pipe(pipe_fds);
	fdmap.parent_fd = pipe_fds[1];
	fdmap.child_fd = STDOUT_FILENO;
	if (mm_spawn(&pid, argv[0], 1, &fdmap, 0, argv, NULL)) {
		mm_close(pipe_fds[1]);
		mm_close(pipe_fds[0]);
		return -1;
	}

	// Close writing end in this process so that reading end of command
	// output generates proper end of pipe
	mm_close(pipe_fds[1]);
	pipe_fds[1] = -1;

	// Read whole pipe connected to cmd output into buffer
	do {
		data = buffer_reserve_data(output, BLK_SIZE);
		rsz = mm_read(pipe_fds[0], data, BLK_SIZE);
		if (rsz < 0)
			goto exit;

		buffer_inc_size(output, rsz);
	} while (rsz > 0);

	rv = 0;

exit:
	mm_close(pipe_fds[0]);
	mm_wait_process(pid, &status);
	if (MM_WSTATUS_SIGNALED & status)
		return mm_raise_error(EINTR, "Command %s failed", argv[0]);

	return (rv == -1) ? -1 : status & MM_WSTATUS_CODEMASK;
}


/**************************************************************************
 *                                                                        *
 *                        Compressed file handling                        *
 *                                                                        *
 **************************************************************************/

#define BLOCK_SZ        8192

static
int from_zlib_error(int zlib_errnum)
{
	switch (zlib_errnum) {
	case Z_ERRNO:           return errno;
	case Z_MEM_ERROR:       return ENOMEM;
	case Z_STREAM_ERROR:    return EINVAL;
	default:                return MM_EBADFMT;
	}
}

/**
 * load_compressed_file() - load content of a compressed file in a buffer
 * @path:       path relative to @prefix of the file to open
 * @output:     pointer to struct buffer that will hold the file content
 *
 * Return: 0 in case if success. Otherwise -1 is returned with error state set
 * accordingly.
 */
LOCAL_SYMBOL
int load_compressed_file(const char* path, struct buffer* buff)
{
	gzFile file;
	void* block;
	int rlen, errnum, rv;
	const char* errmsg;

	file = gzopen(path, "r");
	if (!file)
		return mm_raise_from_errno("%s cannot be opened", path);

	rv = 0;
	do {
		block = buffer_reserve_data(buff, BLOCK_SZ);
		rlen = gzread(file, block, BLOCK_SZ);
		if (rlen < 0) {
			errmsg = gzerror(file, &errnum);
			rv = mm_raise_error(from_zlib_error(errnum), errmsg);
			goto exit;
		}

		buffer_inc_size(buff, rlen);
	} while (rlen);

exit:
	if (gzclose(file) != Z_OK) {
		errmsg = gzerror(file, &errnum);
		rv = mm_raise_error(from_zlib_error(errnum), errmsg);
	}

	return rv;
}


LOCAL_SYMBOL
int save_compressed_file(const char* path, const struct buffer* buff)
{
	gzFile file;
	int errnum, rv, rlen;
	const char* errmsg;
	const char* data = buff->base;
	int remaining = buff->size;

	file = gzopen(path, "w");
	if (!file)
		return mm_raise_from_errno("%s cannot be opened", path);

	rv = 0;
	while (remaining) {
		rlen = gzwrite(file, data, remaining);
		if (rlen <= 0) {
			errmsg = gzerror(file, &errnum);
			rv = mm_raise_error(from_zlib_error(errnum), errmsg);
			goto exit;
		}

		remaining -= rlen;
		data += rlen;
	}

exit:
	if (gzclose(file) != Z_OK) {
		errmsg = gzerror(file, &errnum);
		rv = mm_raise_error(from_zlib_error(errnum), errmsg);
	}

	return rv;
}
