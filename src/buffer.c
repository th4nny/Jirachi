//
// Created by Lodingglue on 12/24/2025.
//
#include "buffer.h"
#include <stdlib.h>
#include <string.h>

buffer_t* buffer_create(size_t initial_capacity) {
    buffer_t *buf = (buffer_t*)malloc(sizeof(buffer_t));
    if (!buf) return NULL;

    buf->data = (uint8_t*)malloc(initial_capacity);
    if (!buf->data) {
        free(buf);
        return NULL;
    }

    buf->size = 0;
    buf->capacity = initial_capacity;
    return buf;
}

void buffer_destroy(buffer_t *buf) {
    if (buf) {
        free(buf->data);
        free(buf);
    }
}

int buffer_append(buffer_t *buf, const void *data, size_t len) {
    if (!buf || !data) return 0;

    if (buf->size + len > buf->capacity) {
        size_t new_cap = buf->capacity * 2;
        if (new_cap < buf->size + len) new_cap = buf->size + len;

        uint8_t *new_data = (uint8_t*)realloc(buf->data, new_cap);
        if (!new_data) return 0;

        buf->data = new_data;
        buf->capacity = new_cap;
    }

    memcpy(buf->data + buf->size, data, len);
    buf->size += len;
    return 1;
}

int buffer_append_u1(buffer_t *buf, uint8_t val) {
    return buffer_append(buf, &val, 1);
}

int buffer_append_u2_be(buffer_t *buf, uint16_t val) {
    uint8_t bytes[2] = { (val >> 8) & 0xFF, val & 0xFF };
    return buffer_append(buf, bytes, 2);
}

int buffer_append_u4_be(buffer_t *buf, uint32_t val) {
    uint8_t bytes[4] = {
        (val >> 24) & 0xFF,
        (val >> 16) & 0xFF,
        (val >> 8) & 0xFF,
        val & 0xFF
    };
    return buffer_append(buf, bytes, 4);
}

uint8_t* buffer_detach(buffer_t *buf, size_t *out_size) {
    if (!buf) return NULL;
    uint8_t *data = buf->data;
    if (out_size) *out_size = buf->size;
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
    return data;
}