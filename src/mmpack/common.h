/*
 * @mindmaze_header@
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

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

static inline
uint64_t next_pow2_u64(uint64_t v)
{
	// 1ull << 64 and __builtin_clzll(0) are undefined
	if (v <= 1)
		return 1;

	return 1ull << (64 - __builtin_clzll(v - 1));
}

#ifdef _WIN32
#define MOUNT_TARGET "/m/"
#else
#define MOUNT_TARGET "/run/mmpack"
#endif

#endif /* COMMON_H */
