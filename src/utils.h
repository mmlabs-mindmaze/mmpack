/*
 * @mindmaze_header@
 */

#ifndef UTILS_H
#define UTILS_H

#include <mmlog.h>
#include <stdio.h>
#include "mmstring.h"

typedef enum {
	OS_IS_UNKNOWN,
	OS_ID_DEBIAN,
	OS_ID_WINDOWS_10,
} os_id;

os_id get_os_id(void);

static inline
int is_path_separator(char c)
{
#if defined(_WIN32)
	return (c == '\\' || c == '/');
#else
	return (c == '/');
#endif
}



#define MMPACK_STATEDIR_RELPATH "var/lib/mmpack"
#define INSTALLED_INDEX_RELPATH MMPACK_STATEDIR_RELPATH "/installed.yaml"
#define REPO_INDEX_RELPATH      MMPACK_STATEDIR_RELPATH "/binindex.yaml"
#define METADATA_RELPATH        MMPACK_STATEDIR_RELPATH "/metadata"
#define PKGS_CACHEDIR_RELPATH   "var/cache/mmpack/pkgs"
#define CFG_RELPATH             "etc/mmpack-config.yaml"
#define LOG_RELPATH             "var/log/mmpack.log"

mmstr* get_default_mmpack_prefix(void);
mmstr* get_config_filename(void);

#define SHA_HEXSTR_LEN (32*2) // string of SHA-256 in hexa (\0 NOT incl.)

int sha_compute(mmstr* hash, const mmstr* filename, const mmstr* parent);


mmstr* mmstr_basename(mmstr* restrict basepath, const mmstr* restrict path);
mmstr* mmstr_dirname(mmstr* restrict dirpath, const mmstr* restrict path);
mmstr* mmstr_join_path(mmstr* restrict dst,
                       const mmstr* restrict p1, const mmstr* restrict p2);

int open_file_in_prefix(const mmstr* prefix, const mmstr* relpath, int oflag);

void report_user_and_log(int mmlog_level, const char* fmt, ...);

#define info(fmt, ...) report_user_and_log(MMLOG_INFO, fmt, ## __VA_ARGS__)
#define error(fmt, ...) report_user_and_log(MMLOG_ERROR, fmt, ## __VA_ARGS__)

int prompt_user_confirm(void);


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
};

void strlist_init(struct strlist* list);
void strlist_deinit(struct strlist* list);
int strlist_add(struct strlist* list, const mmstr* str);


#endif /* UTILS_H */
