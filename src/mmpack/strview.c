/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmerrno.h>
#include <stdint.h>
#include <stdlib.h>

#include "strview.h"


static
int strview_parse_uintmax(uintmax_t* uimax_val, struct strview sv)
{
	int i;
	uintmax_t value, u;

	sv = strview_strip(sv);

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
 * strview_parse_size() - convert string view to unsigned long
 * @ulval:      pointer to unsigned long that receive the converted value
 * @sv:         string view that must be converted
 *
 * Return: 0 in case of success, -1 otherwise with error state set accordingly.
 */
LOCAL_SYMBOL
int strview_parse_size(size_t* szval, struct strview sv)
{
	int errcode, reterrno;
	uintmax_t uimax_val;

	reterrno = strview_parse_uintmax(&uimax_val, sv);
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
