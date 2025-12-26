//
// Created by Lodingglue on 12/24/2025.
//
#include "hashtable.h"

#include <stdlib.h>
#include <string.h>
#include <internal.h>
static uint32_t hash_string(const char *str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

hashtable_t* hashtable_create(size_t size) {
    hashtable_t *table = (hashtable_t*)calloc(1, sizeof(hashtable_t));
    if (!table) return NULL;

    table->buckets = (hashtable_entry_t**)calloc(size, sizeof(hashtable_entry_t*));
    if (!table->buckets) {
        free(table);
        return NULL;
    }

    table->size = size;
    table->count = 0;
    return table;
}

void hashtable_destroy(hashtable_t *table, void (*value_free)(void*)) {
    if (!table) return;

    for (size_t i = 0; i < table->size; i++) {
        hashtable_entry_t *entry = table->buckets[i];
        while (entry) {
            hashtable_entry_t *next = entry->next;
            free(entry->key);
            if (value_free) value_free(entry->value);
            free(entry);
            entry = next;
        }
    }

    free(table->buckets);
    free(table);
}

int hashtable_put(hashtable_t *table, const char *key, void *value) {
    if (!table || !key) return 0;

    uint32_t hash = hash_string(key) % table->size;
    hashtable_entry_t *entry = table->buckets[hash];

    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            entry->value = value;
            return 1;
        }
        entry = entry->next;
    }

    entry = (hashtable_entry_t*)malloc(sizeof(hashtable_entry_t));
    if (!entry) return 0;

    entry->key = strdup(key);
    if (!entry->key) {
        free(entry);
        return 0;
    }

    entry->value = value;
    entry->next = table->buckets[hash];
    table->buckets[hash] = entry;
    table->count++;

    return 1;
}

void* hashtable_get(hashtable_t *table, const char *key) {
    if (!table || !key) return NULL;

    uint32_t hash = hash_string(key) % table->size;
    hashtable_entry_t *entry = table->buckets[hash];

    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }

    return NULL;
}

int hashtable_remove(hashtable_t *table, const char *key) {
    if (!table || !key) return 0;

    uint32_t hash = hash_string(key) % table->size;
    hashtable_entry_t **entry_ptr = &table->buckets[hash];

    while (*entry_ptr) {
        hashtable_entry_t *entry = *entry_ptr;
        if (strcmp(entry->key, key) == 0) {
            *entry_ptr = entry->next;
            free(entry->key);
            free(entry);
            table->count--;
            return 1;
        }
        entry_ptr = &entry->next;
    }

    return 0;
}

void hashtable_iterate(hashtable_t *table, void (*callback)(const char*, void*, void*), void *userdata) {
    if (!table || !callback) return;

    for (size_t i = 0; i < table->size; i++) {
        hashtable_entry_t *entry = table->buckets[i];
        while (entry) {
            callback(entry->key, entry->value, userdata);
            entry = entry->next;
        }
    }
}