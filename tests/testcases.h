/*
 * @mindmaze_header@
 */
#ifndef TESTCASES_H
#define TESTCASES_H

#include <check.h>

TCase* create_binindex_tcase(void);
TCase* create_srcindex_tcase(void);
TCase* create_config_tcase(void);
TCase* create_dependencies_tcase(void);
TCase* create_version_tcase(void);
TCase* create_sha_tcase(void);
TCase* create_indextable_tcase(void);
TCase* create_hashset_tcase(void);
TCase* create_misc_tcase(void);

#endif /* TESTCASES_H */
