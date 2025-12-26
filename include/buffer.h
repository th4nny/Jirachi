#ifndef BUFFER_H
#define BUFFER_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} buffer_t;

buffer_t* buffer_create(size_t initial_capacity);
void buffer_destroy(buffer_t *buf);
int buffer_append(buffer_t *buf, const void *data, size_t len);
int buffer_append_u1(buffer_t *buf, uint8_t val);
int buffer_append_u2_be(buffer_t *buf, uint16_t val);
int buffer_append_u4_be(buffer_t *buf, uint32_t val);
uint8_t* buffer_detach(buffer_t *buf, size_t *out_size);

#endif /* BUFFER_H */