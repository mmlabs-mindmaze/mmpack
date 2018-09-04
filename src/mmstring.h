/*
 * @mindmaze_header@
 */

#ifndef MMSTRING_H
#define MMSTRING_H

#include <mmlib.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "mm-alloc.h"
#include "mmpack-common.h"


struct mmstring {
	int16_t max;
	int16_t len;
	char buf[];
};
#define MMSTR_NEEDED_SIZE(len)   (sizeof(struct mmstring)+1+(len))
#define MMSTR_SIZE_TO_MAXLEN(sz) ((sz)-sizeof(struct mmstring)-1)
#define MMSTR_HDR(str)           ((struct mmstring*)((str)-offsetof(struct mmstring, buf)))


typedef char mmstr;


static inline NONNULL_ARGS(1)
int mmstrlen(const mmstr* str)
{
	return MMSTR_HDR(str)->len;
}


static inline NONNULL_ARGS(1)
int mmstr_maxlen(const mmstr* str)
{
	return MMSTR_HDR(str)->max;
}


static inline NONNULL_ARGS(1)
void mmstr_setlen(mmstr* str, int len)
{
	struct mmstring* s = MMSTR_HDR(str);

	s->len = len;
	s->buf[len] = '\0';
}


static inline
mmstr* mmstr_init(void* ptr, int maxlen)
{
	struct mmstring* s = ptr;

	if (!s)
		return NULL;

	s->max = maxlen;
	s->len = 0;
	s->buf[0] = '\0';

	return s->buf;
}


static inline
mmstr* mmstr_copy(mmstr* str, const char* restrict data, int len)
{
	struct mmstring* s;

	if (!str)
		return NULL;

	s = MMSTR_HDR(str);
	memcpy(s->buf, data, len);
	s->buf[len] = '\0';
	s->len = len;

	return str;
}


static inline
void mmstr_free(const mmstr* str)
{
	if (str != NULL)
		free(MMSTR_HDR(str));
}


static inline
void mmstr_freea(const mmstr* str)
{
	if (str != NULL)
		mm_freea(MMSTR_HDR(str));
}


#define mmstr_malloc(maxlen)    mmstr_init(mm_malloc(MMSTR_NEEDED_SIZE(maxlen)), maxlen)
#define mmstr_malloca(maxlen)   mmstr_init(mm_malloca(MMSTR_NEEDED_SIZE(maxlen)), maxlen)
#define mmstr_alloca(maxlen)    mmstr_init(alloca(MMSTR_NEEDED_SIZE(maxlen)), maxlen)
#define mmstr_map_on_array(array)       mmstr_init(array, MMSTR_SIZE_TO_MAXLEN(sizeof(array)))

#define mmstr_malloc_copy(data, len)    mmstr_copy(mmstr_malloc(len), (data), (len))
#define mmstr_malloc_from_cstr(cstr)    mmstr_malloc_copy(cstr, strlen(cstr))
#define mmstr_malloca_copy(data, len)   mmstr_copy(mmstr_malloca(len), (data), (len))
#define mmstr_malloca_from_cstr(cstr)   mmstr_malloca_copy(cstr, strlen(cstr))
#define mmstr_alloca_copy(data, len)    mmstr_copy(mmstr_alloca(len), (data), (len))
#define mmstr_alloca_from_cstr(cstr)    mmstr_alloca_copy(cstr, strlen(cstr))


static inline NONNULL_ARGS(1)
mmstr* mmstr_resize(mmstr* str, int new_maxlen)
{
	struct mmstring* s = MMSTR_HDR(str);

	if (new_maxlen <= s->max)
		return s->buf;

	s = realloc(s, MMSTR_NEEDED_SIZE(new_maxlen));
	if (!s)
		return NULL;

	s->max = new_maxlen;
	return s->buf;
}


static inline NONNULL_ARGS(1,2)
int mmstrcmp(const mmstr* str1, const mmstr* str2)
{
	const struct mmstring* s1 = MMSTR_HDR(str1);
	const struct mmstring* s2 = MMSTR_HDR(str2);
	int rv;

	// Compare the initial part of each string
	rv = memcmp(s1->buf, s2->buf, MIN(s1->len, s2->len));
	if (rv != 0)
		return rv;

	// In case of common prefix, the rest of comparison revert to
	// simply check which string is the longest
	return s1->len - s2->len;
}


static inline NONNULL_ARGS(1,2)
int mmstrequal(const mmstr* str1, const mmstr* str2)
{
	const struct mmstring* s1 = MMSTR_HDR(str1);
	const struct mmstring* s2 = MMSTR_HDR(str2);

	return (   (s1->len == s2->len)
	        && (memcmp(s1->buf, s2->buf, s1->len) == 0));
}


static inline NONNULL_ARGS(1,2)
mmstr* mmstrcat(mmstr* restrict dst, const mmstr* restrict src)
{
	struct mmstring* d = MMSTR_HDR(dst);
	const struct mmstring* s = MMSTR_HDR(src);

	memcpy(d->buf + d->len, s->buf, s->len+1);
	d->len += s->len;

	return dst;
}


static inline NONNULL_ARGS(1,2)
mmstr* mmstrcpy(mmstr* restrict dst, const mmstr* restrict src)
{
	struct mmstring* d = MMSTR_HDR(dst);
	const struct mmstring* s = MMSTR_HDR(src);

	d->len = s->len;
	memcpy(d->buf, s->buf, s->len+1);

	return dst;
}


static inline NONNULL_ARGS(1,2)
mmstr* mmstrcat_cstr(mmstr* restrict dst, const char* restrict cstr)
{
	struct mmstring* d = MMSTR_HDR(dst);
	size_t len = strlen(cstr);

	memcpy(d->buf + d->len, cstr, len+1);
	d->len += len;

	return dst;
}


static inline NONNULL_ARGS(1,2)
mmstr* mmstrcpy_cstr(mmstr* restrict dst, const char* restrict cstr)
{
	struct mmstring* d = MMSTR_HDR(dst);

	d->len = strlen(cstr);
	memcpy(d->buf, cstr, d->len+1);

	return dst;
}


static inline NONNULL_ARGS(1)
mmstr* mmstrdup(const mmstr* restrict src)
{
	mmstr* dst;

	dst = mmstr_malloc(mmstr_maxlen(src));
	return mmstrcpy(dst, src);
}


#endif
