/*
 * @mindmaze_header@
 */

#ifndef COMMON_H
#define COMMON_H

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

#endif /* COMMON_H */
