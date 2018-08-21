#ifndef MM_ALLOC_H
#define MM_ALLOC_H

#include <mmerrno.h>
#include <mmpredefs.h>
#include <stdlib.h>

static inline
void * mm_malloc(size_t size)
{
	void * rv = malloc(size);
	if (UNLIKELY(rv == NULL))
		exit(mm_raise_error(ENOMEM, "out of memory"));

	return rv;
}

static inline
void * mm_realloc(void *ptr, size_t size)
{
	void * rv = realloc(ptr, size);
	if (UNLIKELY(rv == NULL))
		exit(mm_raise_error(ENOMEM, "out of memory"));

	return rv;
}

#endif /* MM_ALLOC_H */
