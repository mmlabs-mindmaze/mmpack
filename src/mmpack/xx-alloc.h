#ifndef XX_ALLOC_H
#define XX_ALLOC_H

#include <mmlog.h>
#include <stdlib.h>


static inline
void* xx_mm_aligned_alloc(size_t alignment, size_t size)
{
	void * rv = mm_aligned_alloc(alignment, size);
	mm_check(rv != NULL);

	return rv;
}


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

static inline
void* _xx_malloca_on_heap(size_t size)
{
	void * rv = _mm_malloca_on_heap(size);
	mm_check(rv != NULL);

	return rv;
}

#define xx_malloca(size) \
	((size) > MM_STACK_ALLOC_THRESHOLD \
	 ? _xx_malloca_on_heap(size) \
	 : mm_aligned_alloca(2*MM_STK_ALIGN, (size)))

#endif /* XX_ALLOC_H */
