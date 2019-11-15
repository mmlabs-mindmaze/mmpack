#ifndef XX_ALLOC_H
#define XX_ALLOC_H

#include <mmlog.h>
#include <stdlib.h>

static inline
void* xx_malloc(size_t size)
{
	void * rv = malloc(size);
	mm_check(rv != NULL);

	return rv;
}

static inline
void* xx_realloc(void * ptr, size_t size)
{
	void * rv = realloc(ptr, size);
	mm_check(rv != NULL);

	return rv;
}

#endif /* XX_ALLOC_H */
