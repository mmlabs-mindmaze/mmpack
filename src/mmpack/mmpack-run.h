/*
 * @mindmaze_header@
 */

#ifndef MMPACK_RUN_H
#define MMPACK_RUN_H

#include "context.h"

#define RUN_SYNOPSIS "run [run_opts] [cmd...]"

int mmpack_run(struct mmpack_ctx * ctx, int argc, const char* argv[]);

#endif /* MMPACK_RUN_H */

