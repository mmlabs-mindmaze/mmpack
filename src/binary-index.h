/*
 * @mindmaze_header@
 */
#ifndef BINARY_INDEX_H
#define BINARY_INDEX_H

#include "context.h"
#include "mmstring.h"

int binary_index_populate(struct mmpack_ctx * ctx, char const * index_filename);
void binary_index_dump(struct indextable const * binindex);

int installed_index_populate(struct mmpack_ctx * ctx, char const * index_filename);
void installed_index_dump(struct indextable const * installed);

#endif /* BINARY_INDEX_H */
