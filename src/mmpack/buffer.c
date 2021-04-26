/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <string.h>

#include "buffer.h"
#include "common.h"
#include "xx-alloc.h"


/**
 * buffer_init() - initialize buffer structure
 * @buf: buffer structure to initialize
 *
 * To be cleansed by calling buffer_deinit()
 */
LOCAL_SYMBOL
void buffer_init(struct buffer* buf)
{
	*buf = (struct buffer) {0};
}


/**
 * buffer_deinit() - cleanup buffer structure
 * @buf: buffer structure to cleanse
 */
LOCAL_SYMBOL
void buffer_deinit(struct buffer* buf)
{
	free(buf->base);
	*buf = (struct buffer) {0};
}


/**
 * buffer_reserve_data() - ensure data buffer can welcome a new amount
 * @buff:       struct buffer to modify
 * @need_size:  data size to accommodate in addition to the current size
 *
 * Return: pointer to data allocated next to content previously added to
 * @buff. You are guaranteed that up to @need_size can be written to the
 * returned data.
 */
LOCAL_SYMBOL
void* buffer_reserve_data(struct buffer* buff, size_t need_size)
{
	if (buff->size + need_size > buff->max_size) {
		buff->max_size = next_pow2_u64(buff->size + need_size);
		buff->base = xx_realloc(buff->base, buff->max_size);
	}

	return (char*)buff->base + buff->size;
}


/**
 * buffer_inc_size() - increase memory use in the buffer
 * @buff:       struct buffer to modify
 * @size:       number of byte to add to @buff->size
 *
 * Return: the new size used by @buff.
 */
LOCAL_SYMBOL
size_t buffer_inc_size(struct buffer* buff, size_t size)
{
	buff->size += size;
	return buff->size;
}


/**
 * buffer_dec_size() - reduce size used of a buffer
 * @buf:      struct buffer to modify
 * @sz:       number of byte to reduce
 *
 * This function is the exact opposite of buffer_inc_size(). It reduces the
 * size "used" by the buffer by @size number of bytes. The memory
 * corresponding to the size reduction memory is still allocated by @buff
 * and can be accessed until overwritten explicitly.
 *
 * Return: the pointer to new top of buffer.
 */
LOCAL_SYMBOL
void* buffer_dec_size(struct buffer* buf, size_t sz)
{
	buf->size -= sz;
	return (char*)buf->base + buf->size;
}


/**
 * buffer_push() - push data on top of buffer
 * @buf: initialized buffer structure
 * @data: pointer to the data to push
 * @sz: size of @data
 */
LOCAL_SYMBOL
void buffer_push(struct buffer* buf, const void* data, size_t sz)
{
	memcpy(buffer_reserve_data(buf, sz), data, sz);
	buffer_inc_size(buf, sz);
}


/**
 * buffer_pop() - pop data from buffer
 * @buf: initialized buffer structure
 * @data: pointer to receive the data to pop
 * @sz: size to pop from the buffer
 */
LOCAL_SYMBOL
void buffer_pop(struct buffer* buf, void* data, size_t sz)
{
	memcpy(data, buffer_dec_size(buf, sz), sz);
}


/**
 * buffer_take_data_ownership() - steal ownership of the internal data
 * @buff:       struct buffer to modify
 *
 * This function steal ownership of the internal data buffer of @buff.
 * After this call, @buff will not touch this buffer anymore, later call to
 * buffer_*() will operate on a new internal data buffer.
 *
 * Return: the pointer to the internal buffer. Call free() on it once you
 * are done with it.
 */
LOCAL_SYMBOL
void* buffer_take_data_ownership(struct buffer* buff)
{
	void* data = buff->base;

	buffer_init(buff);

	return data;
}
