/*
 * @mindmaze_header@
 */
#ifndef STRVIEW_H
#define STRVIEW_H

#include "common.h"


/**
 * struct strview: string view on constant buffer
 * @buf:        pointer of the string memory (not null terminated)
 * @len:        length of the string
 */
struct strview {
	const char* buf;
	int len;
};


/**
 * strview_find() - search the first occurrence of a character
 * @sv: string view to search in
 * @c:  character to search
 *
 * Return: position of the first occurrence of @c if found, otherwise sv.len
 */
static inline
int strview_find(struct strview sv, int c)
{
	int i;

	for (i = 0; i < sv.len; i++) {
		if (sv.buf[i] == c)
			break;
	}

	return i;
}


/**
 * strview_rfind() - search the last occurrence of a character
 * @sv: string view to search in
 * @c:  character to search
 *
 * Return: position of the last occurrence of @c if found, -1 otherwise
 */
static inline
int strview_rfind(struct strview sv, int c)
{
	int i;

	for (i = sv.len - 1; i >= 0; i--) {
		if (sv.buf[i] == c)
			break;
	}

	return i;
}


/**
 * strview_lpart() - Get the string view of the left part up to position
 * @sv:         original string view
 * @pos:        position up to which the substring must be got. may be negative
 *
 * Return: string view corresponding to the left part of @sv before the
 * character indexed by @pos (excluding @pos). If @pos is negative, the return
 * string view is empty.
 */
static inline
struct strview strview_lpart(struct strview sv, int pos)
{
	return (struct strview) {
		.buf = sv.buf,
		.len = clamp(pos, 0, sv.len),
	};
}


/**
 * strview_lpart() - Get the string view of the right part to position
 * @sv:         original string view
 * @pos:        position up to which the substring must be got. may be negative
 *
 * Return: string view corresponding to the right part of @sv after the
 * character indexed by @pos (excluding @pos). If @pos is higher than negative, the return
 * string view is empty.
 */
static inline
struct strview strview_rpart(struct strview sv, int pos)
{
	int off = clamp(pos+1, 0, sv.len);

	return (struct strview) {
		.buf = sv.buf + off,
		.len = sv.len - off,
	};
}


/**
 * strview_getline() - extract view of next line
 * @in:         pointer to input string view
 *
 * Extract from pointed view @in a new view reflecting the first line
 * contained in @in. If a new line delimiter cannot be found, the result will
 * be equal to the input.
 *
 * The pointed view by @in is updated to map the content of the input string
 * just after the new line character found. If none is found, the updated view
 * shall be empty (reflecting that the line found has consumed all the string).
 *
 * Returns: the string view of the first line.
 */
static inline
struct strview strview_getline(struct strview *in)
{
	struct strview data = *in;
	int pos;

	pos = strview_find(data, '\n');
	*in  = strview_rpart(data, pos);

	return strview_lpart(data, pos);
}


static inline
struct strview strview_rstrip(struct strview sv)
{
	// Remove trailing whitspaces
	for (; sv.len > 0; sv.len--) {
		if (!is_whitespace(sv.buf[sv.len-1]))
			break;
	}

	return sv;
}


static inline
struct strview strview_lstrip(struct strview sv)
{
	// Remove leading whitespaces
	for (; sv.len; sv.len--, sv.buf++) {
		if (!is_whitespace(*sv.buf))
			break;
	}

	return sv;
}


static inline
struct strview strview_strip(struct strview sv)
{
	return strview_rstrip(strview_lstrip(sv));
}


int strview_parse_size(size_t* szval, struct strview sv);


#endif
