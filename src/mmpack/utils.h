/*
 * @mindmaze_header@
 */

#ifndef UTILS_H
#define UTILS_H

#include <mmlog.h>
#include <stdio.h>
#include "buffer.h"
#include "mmstring.h"
#include "strchunk.h"

typedef enum {
	OS_IS_UNKNOWN,
	OS_ID_DEBIAN,
	OS_ID_WINDOWS_10,
} os_id;

os_id get_os_id(void);

static inline
int is_path_separator(char c)
{
#if defined (_WIN32)
	return (c == '\\' || c == '/');
#else
	return (c == '/');
#endif
}


#define MMPACK_STATEDIR_RELPATH "var/lib/mmpack"
#define CFG_RELPATH "etc/mmpack-config.yaml"
#define LOG_RELPATH "var/log/mmpack.log"
#define PKGS_CACHEDIR_RELPATH "var/cache/mmpack/pkgs"
#define UNPACK_CACHEDIR_RELPATH "var/cache/mmpack/unpack"

#define INSTALLED_INDEX_RELPATH \
	MMPACK_STATEDIR_RELPATH "/installed"
#define MANUALLY_INST_RELPATH \
	MMPACK_STATEDIR_RELPATH "/manually-installed.txt"
#define REPO_INDEX_RELPATH \
	MMPACK_STATEDIR_RELPATH "/binindex"
#define SRC_INDEX_RELPATH \
	MMPACK_STATEDIR_RELPATH "/srcindex"
#define METADATA_RELPATH \
	MMPACK_STATEDIR_RELPATH "/metadata"

mmstr* get_xdg_subpath(enum mm_known_dir dirtype, const char* subdir);


#define SHA_HDR_REG "reg-"
#define SHA_HDR_SYM "sym-"
#define SHA_HDRLEN 4
/* string of header and SHA-256 in hexa (\0 NOT incl.) */
#define SHA_HEXSTR_LEN (SHA_HDRLEN + 32*2)

int sha_compute(mmstr* hash, const mmstr* filename, const mmstr* parent,
                int follow);
int check_hash(const mmstr* sha, const mmstr* parent, const mmstr* filename);


mmstr* mmstr_basename(mmstr* restrict basepath, const mmstr* restrict path);
mmstr* mmstr_dirname(mmstr* restrict dirpath, const mmstr* restrict path);
mmstr* mmstr_join_path(mmstr* restrict dst,
                       const mmstr* restrict p1, const mmstr* restrict p2);
mmstr* mmstr_join_path_realloc(mmstr* restrict dst,
                               const mmstr* restrict p1,
                               const mmstr* restrict p2);

int open_file_in_prefix(const mmstr* prefix, const mmstr* relpath, int oflag);
int map_file_in_prefix(const mmstr* prefix, const mmstr* relpath,
                       void** map, size_t* len);

void report_user_and_log(int mm_log_level, const char* fmt, ...);

#define info(fmt, ...) report_user_and_log(MM_LOG_INFO, fmt, ## __VA_ARGS__)
#define error(fmt, ...) report_user_and_log(MM_LOG_ERROR, fmt, ## __VA_ARGS__)

int prompt_user_confirm(void);

/**************************************************************************
 *                                                                        *
 *                            String helpers                              *
 *                                                                        *
 **************************************************************************/
char* strchr_or_end(const char * s, int c);

mmstr* linewrap_string(mmstr* restrict out, struct strchunk in,
                       int len, const char* indent_str);
mmstr* textwrap_string(mmstr* restrict out, struct strchunk in,
                       int len, const char* indent_str, const char* nl_seq);

/**************************************************************************
 *                                                                        *
 *                        External cmd execution                          *
 *                                                                        *
 **************************************************************************/
int execute_cmd(char* argv[]);
int execute_cmd_capture_output(char* argv[], struct buffer* output);


/**************************************************************************
 *                                                                        *
 *                        Compressed file handling                        *
 *                                                                        *
 **************************************************************************/
int load_compressed_file(const char* path, struct buffer* buff);
int save_compressed_file(const char* path, const struct buffer* buff);

#endif /* UTILS_H */
