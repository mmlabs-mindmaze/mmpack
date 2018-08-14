/*
 * @mindmaze_header@
 */

#ifndef MMPACK_COMMON_H
#define MMPACK_COMMON_H

/* Function attribute to specify some pointer arguments are non null */
#if defined(__GNUC__)
#  define NONNULL_ARGS(...)     __attribute__((nonnull (__VA_ARGS__)))
#else
#  define NONNULL_ARGS(...)
#endif

#ifndef MIN
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#endif

#ifndef MAX
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#endif

#define STR_EQUAL(str, len, const_str) \
	(len == (sizeof(const_str) - 1) \
	 && memcmp(str, const_str, sizeof(const_str) - 1) == 0)

#define STR_STARTS_WITH(str, len, const_str) \
	(len >= (sizeof(const_str) - 1) \
	 && memcmp(str, const_str, sizeof(const_str) - 1) == 0)

typedef enum {
	OS_IS_UNKNOWN,
	OS_ID_DEBIAN,
	OS_ID_WINDOWS_10,
} os_id;

os_id get_os_id(void);

char const * get_local_mmpack_binary_index_path(void);
char const * get_mmpack_installed_pkg_path(void);
char const * get_config_filename(void);

#define SHA_HEXSTR_SIZE (32*2+1) // string of SHA-256 in hexa (\0 incl.)

int sha_compute(char* hash, const char* filename, const char* parent);

#endif /* MMPACK_COMMON_H */
