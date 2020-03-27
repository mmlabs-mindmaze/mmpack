/*
 * @mindmaze_header@
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#ifndef MIN
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#endif

#ifndef MAX
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#endif

#define ROUND_UP(x, y) ( (((x)+(y)-1) / (y)) * (y) )

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
#define MOUNT_TARGET "M:"
#else
#define MOUNT_TARGET "/run/mmpack"
#endif


static inline
int is_whitespace(int c)
{
	return (c == '\t'
	     || c == '\n'
	     || c == '\v'
	     || c == '\f'
	     || c == '\r'
	     || c == ' ');
}


static inline
int clamp(int v, int min, int max)
{
	if (v < min)
		v = min;
	else if (v > max)
		v = max;

	return v;
}


#endif /* COMMON_H */
