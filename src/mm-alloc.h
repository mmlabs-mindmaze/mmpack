#ifndef MM_ALLOC_H
#define MM_ALLOC_H

#include <mmlog.h>
#include <stdlib.h>

static inline
void * mm_malloc(size_t size)
{
	void * rv = malloc(size);
	mm_check(rv != NULL, "out of memory");

	return rv;
}

static inline
void * mm_realloc(void *ptr, size_t size)
{
	void * rv = realloc(ptr, size);
	mm_check(rv != NULL, "out of memory");

	return rv;
}

#endif /* MM_ALLOC_H */
