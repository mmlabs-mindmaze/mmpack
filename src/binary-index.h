/*
 * @mindmaze_header@
 */
#ifndef BINARY_INDEX_H
#define BINARY_INDEX_H

#include "mmpack-context.h"
#include "mmstring.h"

int binary_index_populate(struct mmpack_ctx * ctx, char const * index_filename);
void binary_index_dump(struct indextable const * binindex);

#endif /* BINARY_INDEX_H */
