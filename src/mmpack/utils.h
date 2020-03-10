/*
 * @mindmaze_header@
 */

#ifndef UTILS_H
#define UTILS_H

#include <mmlog.h>
#include <stdio.h>
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
	MMPACK_STATEDIR_RELPATH "/installed.yaml"
#define MANUALLY_INST_RELPATH \
	MMPACK_STATEDIR_RELPATH "/manually-installed.txt"
#define REPO_INDEX_RELPATH \
	MMPACK_STATEDIR_RELPATH "/binindex.yaml"
#define SRC_INDEX_RELPATH \
	MMPACK_STATEDIR_RELPATH "/srcindex"
#define METADATA_RELPATH \
	MMPACK_STATEDIR_RELPATH "/metadata"

mmstr* get_config_filename(void);

#define SHA_HDR_REG "reg-"
#define SHA_HDR_SYM "sym-"
#define SHA_HDRLEN 4
/* string of header and SHA-256 in hexa (\0 NOT incl.) */
#define SHA_HEXSTR_LEN (SHA_HDRLEN + 32*2)

int sha_compute(mmstr* hash, const mmstr* filename, const mmstr* parent,
                int follow);


mmstr* mmstr_basename(mmstr* restrict basepath, const mmstr* restrict path);
mmstr* mmstr_dirname(mmstr* restrict dirpath, const mmstr* restrict path);
mmstr* mmstr_join_path(mmstr* restrict dst,
                       const mmstr* restrict p1, const mmstr* restrict p2);

int open_file_in_prefix(const mmstr* prefix, const mmstr* relpath, int oflag);
int map_file_in_prefix(const mmstr* prefix, const mmstr* relpath,
                       void** map, size_t* len);

void report_user_and_log(int mm_log_level, const char* fmt, ...);

#define info(fmt, ...) report_user_and_log(MM_LOG_INFO, fmt, ## __VA_ARGS__)
#define error(fmt, ...) report_user_and_log(MM_LOG_ERROR, fmt, ## __VA_ARGS__)

int prompt_user_confirm(void);

char* strchr_or_end(const char * s, int c);


/**************************************************************************
 *                                                                        *
 *                            string list                                 *
 *                                                                        *
 **************************************************************************/

struct strlist_elt {
	struct strlist_elt* next;
	struct mmstring str;
};

struct strlist {
	struct strlist_elt* head;
	struct strlist_elt* last;
};

void strlist_init(struct strlist* list);
void strlist_deinit(struct strlist* list);
int strlist_add_strchunk(struct strlist* list, struct strchunk sv);
int strlist_add(struct strlist* list, const char* str);
void strlist_remove(struct strlist* list, const mmstr* str);



/**************************************************************************
 *                                                                        *
 *                               buffer                                   *
 *                                                                        *
 **************************************************************************/
struct buffer {
	void* base;
	size_t size;
	size_t max_size;
};


void buffer_init(struct buffer* buf);
void buffer_deinit(struct buffer* buf);
void* buffer_reserve_data(struct buffer* buf, size_t sz);
size_t buffer_inc_size(struct buffer* buf, size_t sz);
void* buffer_dec_size(struct buffer* buf, size_t sz);
void buffer_push(struct buffer* buf, const void* data, size_t sz);
void buffer_pop(struct buffer* buf, void* data, size_t sz);
void* buffer_take_data_ownership(struct buffer* buf);

/**************************************************************************
 *                                                                        *
 *                        External cmd execution                          *
 *                                                                        *
 **************************************************************************/
int execute_cmd(char* argv[]);
int execute_cmd_capture_output(char* argv[], struct buffer* output);

#endif /* UTILS_H */
