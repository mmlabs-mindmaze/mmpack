/*
 * @mindmaze_header@
 */
#ifndef MANUALLY_INSTALLED_H
#define MANUALLY_INSTALLED_H

#include "action-solver.h"
#include "indextable.h"

void remove_from_manually_installed(struct strset * manually_inst,
                                    const mmstr * name);
int load_manually_installed(const mmstr * prefix,
                            struct strset * manually_inst);
int save_manually_installed(const mmstr * prefix,
                            struct strset * manually_inst);

#endif /* MANUALLY_INSTALLED_H */
