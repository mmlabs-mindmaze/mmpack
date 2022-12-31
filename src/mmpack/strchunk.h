/*
 * @mindmaze_header@
 */
#ifndef STRCHUNK_H
#define STRCHUNK_H

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "common.h"


/**
 * struct strchunk: memory slice view of a constant buffer
 * @buf:        pointer of the string memory (not null terminated)
 * @len:        length of the string
 *
 * This represents a subpart (or chunk) of a constant string memory. This
 * pointed memory is not owned by the structure, hence it shall not be modified
 * nor freed through it. The pointed string subpart is NOT null terminated.
 */
struct strchunk {
	const char* buf;
	int len;
};


/**
 * strchunk_from_cstr() - Get the string chunk from a string
 * @str:         nul-terminated string
 *
 * Return: string chunk pointing to @str.
 */
static inline
struct strchunk strchunk_from_cstr(const char* str)
{
	return (struct strchunk) {.buf = str, .len = strlen(str)};
}


/**
 * strchunk_find() - search the first occurrence of a character
 * @sc: string chunk to search in
 * @c:  character to search
 *
 * Return: position of the first occurrence of @c if found, otherwise @sc.len
 */
static inline
int strchunk_find(struct strchunk sc, int c)
{
	int i;

	for (i = 0; i < sc.len; i++) {
		if (sc.buf[i] == c)
			break;
	}

	return i;
}


/**
 * strchunk_rfind() - search the last occurrence of a character
 * @sc: string chunk to search in
 * @c:  character to search
 *
 * Return: position of the last occurrence of @c if found, -1 otherwise
 */
static inline
int strchunk_rfind(struct strchunk sc, int c)
{
	int i;

	for (i = sc.len - 1; i >= 0; i--) {
		if (sc.buf[i] == c)
			break;
	}

	return i;
}


/**
 * strchunk_lpart() - Get the string chunk of the left part up to position
 * @sc:         original string chunk
 * @pos:        position up to which the substring must be got. may be negative
 *
 * Return:
 * string chunk corresponding to the left part of @sc before the character
 * indexed by @pos (excluding @pos). If @pos is negative, the return string
 * chunk is empty.
 */
static inline
struct strchunk strchunk_lpart(struct strchunk sc, int pos)
{
	return (struct strchunk) {
		       .buf = sc.buf,
		       .len = clamp(pos, 0, sc.len),
	};
}


/**
 * strchunk_rpart() - Get the string chunk of the right part from position
 * @sc:         original string chunk
 * @pos:        position from which the substring must be got. may be negative
 *
 * Return:
 * string chunk corresponding to the right part of @sc after the character
 * indexed by @pos (excluding @pos). If @pos is higher than length of @sc, the
 * return string chunk is empty.
 */
static inline
struct strchunk strchunk_rpart(struct strchunk sc, int pos)
{
	int off = clamp(pos+1, 0, sc.len);

	return (struct strchunk) {
		       .buf = sc.buf + off,
		       .len = sc.len - off,
	};
}


/**
 * strchunk_getline() - extract next line chunk
 * @in:         pointer to input string chunk
 *
 * Extract from pointed string chunk @in a new chunk reflecting the first line
 * contained in @in. If a new line delimiter cannot be found, the result will
 * be equal to the input.
 *
 * The chunk pointed by @in is updated to map the content of the input data
 * just after the new line character found. If none is found, the updated chunk
 * shall be empty (reflecting that the line found has consumed all the data).
 *
 * Returns: the string chunk of the first line.
 */
static inline
struct strchunk strchunk_getline(struct strchunk* in)
{
	struct strchunk data = *in;
	int pos;

	pos = strchunk_find(data, '\n');
	*in = strchunk_rpart(data, pos);

	return strchunk_lpart(data, pos);
}


static inline
struct strchunk strchunk_rstrip(struct strchunk sc)
{
	// Remove trailing whitspaces
	for (; sc.len > 0; sc.len--) {
		if (!is_whitespace(sc.buf[sc.len-1]))
			break;
	}

	return sc;
}


static inline
struct strchunk strchunk_lstrip(struct strchunk sc)
{
	// Remove leading whitespaces
	for (; sc.len; sc.len--, sc.buf++) {
		if (!is_whitespace(*sc.buf))
			break;
	}

	return sc;
}


static inline
struct strchunk strchunk_strip(struct strchunk sc)
{
	return strchunk_rstrip(strchunk_lstrip(sc));
}


/**
 * strchunk_is_whitespace() - test if a chunk if only made of whitespaces
 * @sc: string chunk to test
 *
 * Returns: true if @sc is only composed of whitespaces, false otherwise.
 */
static inline
bool strchunk_is_whitespace(struct strchunk sc)
{
	int i;

	for (i = 0; i < sc.len; i++) {
		if (!is_whitespace(sc.buf[i]))
			return false;
	}

	return true;
}


/**
 * strchunk_extent() - get the minimal chunk that contains 2 string chunks
 * @sc1:        first chunk to merge
 * @sc2:        second chunk to merge
 *
 * Find the smallest region that will contain both @sc1 and @sc2. If the two
 * are disjoint, the resulting region may contains characters neither
 * referenced by @sc1 or @sc2.
 *
 * Returns: the smallest string chunk that overlaps both @sc1 and @sc2
 */
static inline
struct strchunk strchunk_extent(struct strchunk sc1, struct strchunk sc2)
{
	const char * start, * end;

	if (sc2.len == 0)
		return sc1;

	if (sc1.len == 0)
		return sc2;

	start = MIN(sc1.buf, sc2.buf);
	end = MAX(sc1.buf + sc1.len, sc2.buf + sc2.len);

	return (struct strchunk) {.buf = start, .len = end - start};
}


static inline
bool strchunk_equal(struct strchunk sc, const char* str)
{
	if (strlen(str) != (size_t)sc.len)
		return false;

	return (memcmp(sc.buf, str, sc.len) == 0);
}


/**
 * strchunk_extract() - get the token matching charset
 * @sc:         the string from which the token is searched
 * @charset:    string of characters which should be matched
 *
 * Returns: the longest string at the beginning @sc that is composed only of
 * characters in @charset.
 */
static inline
struct strchunk strchunk_extract(struct strchunk sc, const char* charset)
{
	int i, len, charset_len;

	charset_len = strlen(charset);
	for (len = 0; len < sc.len; len++) {
		for (i = charset_len; i >= 0; i--) {
			if (charset[i] == sc.buf[len])
				break;
		}

		if (i < 0)
			break;
	}

	return (struct strchunk) {.buf = sc.buf, .len = len};
}


int strchunk_parse_size(size_t* szval, struct strchunk sc);


#endif /* ifndef STRCHUNK_H */
