/*
 * @mindmaze_header@
 */

#ifndef UTILS_H
#define UTILS_H

typedef enum {
	OS_IS_UNKNOWN,
	OS_ID_DEBIAN,
	OS_ID_WINDOWS_10,
} os_id;

os_id get_os_id(void);

char const * get_local_mmpack_binary_index_path(void);
char const * get_local_mmpack_installed_index_path(void);
char const * get_mmpack_installed_pkg_path(void);
char const * get_default_mmpack_prefix(void);
char const * get_config_filename(void);

#define SHA_HEXSTR_SIZE (32*2+1) // string of SHA-256 in hexa (\0 incl.)

int sha_compute(char* hash, const char* filename, const char* parent);

#endif /* UTILS_H */

