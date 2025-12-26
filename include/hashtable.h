#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stddef.h>
#include <stdint.h>

typedef struct hashtable_entry {
    char *key;
    void *value;
    struct hashtable_entry *next;
} hashtable_entry_t;

typedef struct {
    hashtable_entry_t **buckets;
    size_t size;
    size_t count;
} hashtable_t;

hashtable_t* hashtable_create(size_t size);
void hashtable_destroy(hashtable_t *table, void (*value_free)(void*));
int hashtable_put(hashtable_t *table, const char *key, void *value);
void* hashtable_get(hashtable_t *table, const char *key);
int hashtable_remove(hashtable_t *table, const char *key);
void hashtable_iterate(hashtable_t *table, void (*callback)(const char*, void*, void*), void *userdata);

#endif /* HASHTABLE_H */