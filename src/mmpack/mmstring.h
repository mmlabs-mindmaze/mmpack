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
#include "common.h"


struct mmstring {
	int16_t max;
	int16_t len;
	char buf[];
};
#define MMSTR_NEEDED_SIZE(len)   (sizeof(struct mmstring)+1+(len))
#define MMSTR_SIZE_TO_MAXLEN(sz) ((sz)-sizeof(struct mmstring)-1)
#define MMSTR_HDR(str)           ((struct mmstring*)((str)-offsetof(struct mmstring, buf)))


typedef char mmstr;

/**
 * macro STATIC_CONST_MMSTR - declare and initialize statically a mmstr*
 * @name:       name of the variable declared
 * @str_literal: string literal used to initialize mmstr buffer
 *
 * This macro declare a variable static const mmstr* named @name and
 * its value will be statically initialized with @str_literal.
 */
#define STATIC_CONST_MMSTR(name, str_literal)           \
static const struct {                                   \
	int16_t max;                                    \
	int16_t len;                                    \
	char buf[sizeof(str_literal)];                  \
} name ## _mmstring_data = {                            \
	.max = sizeof(str_literal) - 1,                 \
	.len = sizeof(str_literal) - 1,                 \
	.buf = str_literal,                             \
};                                                      \
static const mmstr* name =                              \
	(const mmstr*)&(name ## _mmstring_data.buf)

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


/**
 * mmstr_update_len_from_buffer() - update len when modified externally
 * @str:        string to update
 *
 * This function updates the len field of the mmstring when the string
 * buffer has been modified externally.
 *
 * Return: the updated string length.
 */
static inline NONNULL_ARGS(1)
int mmstr_update_len_from_buffer(mmstr* str)
{
	struct mmstring* s = MMSTR_HDR(str);

	s->len = strlen(s->buf);
	return s->len;
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


static inline
mmstr* mmstr_realloc(mmstr* str, int new_maxlen)
{
	struct mmstring* s = NULL;

	if (str != NULL)
		s = MMSTR_HDR(str);

	if (s && (new_maxlen <= s->max))
		return s->buf;

	s = mm_realloc(s, MMSTR_NEEDED_SIZE(new_maxlen));
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


/**
 * mmstrcpy_realloc() - copy a mmstr to another, resizing dest if needed
 * @dst:        destination string (may be NULL)
 * @src:        string to copy (may NOT be NULL)
 *
 * Return: modified @dst. The actual pointer may be different from the value
 * passed as input if a reallocation was necessary.
 */
static inline NONNULL_ARGS(2)
mmstr* mmstrcpy_realloc(mmstr* restrict dst, const mmstr* restrict src)
{
	dst = mmstr_realloc(dst, mmstrlen(src));
	return mmstrcpy(dst, src);
}


static inline NONNULL_ARGS(2)
mmstr* mmstrcat_realloc(mmstr* restrict dst, const mmstr* restrict src)
{
	dst = mmstr_realloc(dst, mmstrlen(dst) + mmstrlen(src));
	return mmstrcat(dst, src);
}


/**
 * mmstr_copy_realloc() - copy data to a string, resizing dest if needed
 * @dst:        destination string (may be NULL)
 * @data:       buffer holding the data to copy  to @dst
 * @len:        length of @data buffer
 *
 * Return: modified @dst. The actual pointer may be different from the value
 * passed as input if a reallocation was necessary.
 */
static inline NONNULL_ARGS(2)
mmstr* mmstr_copy_realloc(mmstr* dst, const char* restrict data, int len)
{
	dst = mmstr_realloc(dst, len);
	return mmstr_copy(dst, data, len);
}


/**
 * mmstrcpy_cstr_realloc() - copy C string, resizing dest if needed
 * @dst:        destination string (may be NULL)
 * @csrc:       null terminated string to copy
 *
 * Return: modified @dst. The actual pointer may be different from the value
 * passed as input if a reallocation was necessary.
 */
static inline NONNULL_ARGS(2)
mmstr* mmstrcpy_cstr_realloc(mmstr* restrict dst, const mmstr* restrict csrc)
{
	return mmstr_copy_realloc(dst, csrc, strlen(csrc));
}


#endif
