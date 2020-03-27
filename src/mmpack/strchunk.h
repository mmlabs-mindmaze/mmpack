/*
 * @mindmaze_header@
 */
#ifndef STRCHUNK_H
#define STRCHUNK_H

#include "common.h"


/**
 * struct strchunk: memory slice view of a constant buffer
 * @buf:        pointer of the string memory (not null terminated)
 * @len:        length of the string
 *
 * This represent a subpart (or chunk) of a constant string memory. This
 * pointed memory is not owned by the structure, hence it shall not be modified
 * nor freed though it. The pointed string subpart is NOT null terminated.
 */
struct strchunk {
	const char* buf;
	int len;
};


/**
 * strchunk_find() - search the first occurrence of a character
 * @sv: string chunk to search in
 * @c:  character to search
 *
 * Return: position of the first occurrence of @c if found, otherwise sv.len
 */
static inline
int strchunk_find(struct strchunk sv, int c)
{
	int i;

	for (i = 0; i < sv.len; i++) {
		if (sv.buf[i] == c)
			break;
	}

	return i;
}


/**
 * strchunk_rfind() - search the last occurrence of a character
 * @sv: string chunk to search in
 * @c:  character to search
 *
 * Return: position of the last occurrence of @c if found, -1 otherwise
 */
static inline
int strchunk_rfind(struct strchunk sv, int c)
{
	int i;

	for (i = sv.len - 1; i >= 0; i--) {
		if (sv.buf[i] == c)
			break;
	}

	return i;
}


/**
 * strchunk_lpart() - Get the string chunk of the left part up to position
 * @sv:         original string chunk
 * @pos:        position up to which the substring must be got. may be negative
 *
 * Return:
 * string chunk corresponding to the left part of @sv before the character
 * indexed by @pos (excluding @pos). If @pos is negative, the return string
 * chunk is empty.
 */
static inline
struct strchunk strchunk_lpart(struct strchunk sv, int pos)
{
	return (struct strchunk) {
		       .buf = sv.buf,
		       .len = clamp(pos, 0, sv.len),
	};
}


/**
 * strchunk_lpart() - Get the string chunk of the right part to position
 * @sv:         original string chunk
 * @pos:        position up to which the substring must be got. may be negative
 *
 * Return:
 * string chunk corresponding to the right part of @sv after the character
 * indexed by @pos (excluding @pos). If @pos is higher than negative, the
 * return string chunk is empty.
 */
static inline
struct strchunk strchunk_rpart(struct strchunk sv, int pos)
{
	int off = clamp(pos+1, 0, sv.len);

	return (struct strchunk) {
		       .buf = sv.buf + off,
		       .len = sv.len - off,
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
struct strchunk strchunk_rstrip(struct strchunk sv)
{
	// Remove trailing whitspaces
	for (; sv.len > 0; sv.len--) {
		if (!is_whitespace(sv.buf[sv.len-1]))
			break;
	}

	return sv;
}


static inline
struct strchunk strchunk_lstrip(struct strchunk sv)
{
	// Remove leading whitespaces
	for (; sv.len; sv.len--, sv.buf++) {
		if (!is_whitespace(*sv.buf))
			break;
	}

	return sv;
}


static inline
struct strchunk strchunk_strip(struct strchunk sv)
{
	return strchunk_rstrip(strchunk_lstrip(sv));
}


int strchunk_parse_size(size_t* szval, struct strchunk sv);


#endif /* ifndef STRCHUNK_H */
