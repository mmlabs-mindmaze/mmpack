/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <check.h>

#include "hashset.h"
#include "testcases.h"

/**************************************************************************
 *                                                                        *
 *                         test suite creation                            *
 *                                                                        *
 **************************************************************************/
TCase* create_hashset_tcase(void)
{
	TCase * tc;

	tc = tcase_create("hashset");

	return tc;
}
