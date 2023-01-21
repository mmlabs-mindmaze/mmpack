/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmerrno.h>
#include <mmlib.h>
#include <stdint.h>
#include <stdlib.h>

#include "strchunk.h"


static
int strchunk_parse_uintmax(uintmax_t* uimax_val, struct strchunk sv)
{
	int i;
	uintmax_t value, u;

	sv = strchunk_strip(sv);

	value = 0;
	for (i = 0; i < sv.len; i++) {
		if (sv.buf[i] < '0' || sv.buf[i] > '9')
			return ERANGE;

		if (value > UINTMAX_MAX/10)
			return ERANGE;

		u = sv.buf[i] - '0';
		value *= 10;
		if (value + u < value)
			return ERANGE;

		value += u;
	}

	*uimax_val = value;
	return 0;
}


/**
 * strchunk_parse_size() - convert string view to unsigned long
 * @ulval:      pointer to unsigned long that receive the converted value
 * @sv:         string view that must be converted
 *
 * Return: 0 in case of success, -1 otherwise with error state set accordingly.
 */
LOCAL_SYMBOL
int strchunk_parse_size(size_t* szval, struct strchunk sv)
{
	int errcode, reterrno;
	uintmax_t uimax_val;

	reterrno = strchunk_parse_uintmax(&uimax_val, sv);
	if (reterrno) {
		errcode = reterrno;
		goto error;
	}

	if (uimax_val > SIZE_MAX) {
		errcode = ERANGE;
		goto error;
	}

	*szval = uimax_val;
	return 0;

error:
	mm_raise_error(errcode, "fails to convert %.*s", sv.len, sv.buf);
	return -1;
}



/**
 * strchunk_parse_bool() - convert strchunk into 0 or 1 int
 * @val:        pointer to int receiving the converted value
 * @sc:         strchunk holding the string to convert
 *
 * Returns: 0 in case of success, -1 otherwise with error state set.
 */
LOCAL_SYMBOL
int strchunk_parse_bool(int* val, struct strchunk sc)
{
	int bval = -1;
	char tmp[8] = "";

	if ((size_t)sc.len > sizeof(tmp) - 1)
		goto error;

	memcpy(tmp, sc.buf, sc.len);
	if (mm_strcasecmp(tmp, "true") == 0)
		bval = 1;
	else if (mm_strcasecmp(tmp, "false") == 0)
		bval = 0;
	else if (mm_strcasecmp(tmp, "on") == 0)
		bval = 1;
	else if (mm_strcasecmp(tmp, "off") == 0)
		bval = 0;
	else if (mm_strcasecmp(tmp, "yes") == 0)
		bval = 1;
	else if (mm_strcasecmp(tmp, "no") == 0)
		bval = 0;
	else if (mm_strcasecmp(tmp, "y") == 0)
		bval = 1;
	else if (mm_strcasecmp(tmp, "n") == 0)
		bval = 0;

	if (bval != -1) {
		*val = bval;
		return 0;
	}

error:
	mm_raise_error(EINVAL, "invalid bool value: %.*s", sc.len, sc.buf);
	return -1;
}
