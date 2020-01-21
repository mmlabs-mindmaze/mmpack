/*
 * @mindmaze_header@
 */
#ifndef MANUALLY_INSTALLED_H
#define MANUALLY_INSTALLED_H

#include "action-solver.h"
#include "indextable.h"

int suppress_from_if_in_manually_installed(struct strset * manually_inst,
                                           const mmstr * name);
int complete_manually_installed(struct strset * manually_inst,
                                struct pkg_request * reqlist);
int load_manually_installed(const mmstr * prefix,
                            struct strset * manually_inst);
int dump_manually_installed(const mmstr * prefix,
                            struct strset * manually_inst);

#endif /* MANUALLY_INSTALLED_H */
