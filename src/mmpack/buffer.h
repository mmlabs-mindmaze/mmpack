/*
 * @mindmaze_header@
 */
#ifndef BUFFER_H
#define BUFFER_H

#include <stddef.h>

struct buffer {
	void* base;
	size_t size;
	size_t max_size;
};


void buffer_init(struct buffer* buf);
void buffer_deinit(struct buffer* buf);
void* buffer_reserve_data(struct buffer* buf, size_t sz);
size_t buffer_inc_size(struct buffer* buf, size_t sz);
void* buffer_dec_size(struct buffer* buf, size_t sz);
void buffer_push(struct buffer* buf, const void* data, size_t sz);
void buffer_pop(struct buffer* buf, void* data, size_t sz);
void* buffer_take_data_ownership(struct buffer* buf);

#endif /* BUFFER_H */
