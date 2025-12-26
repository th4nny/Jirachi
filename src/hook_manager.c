//
// Created by Lodingglue on 12/24/2025.
//

#include "../include/hook_manager.h"
#include <stdlib.h>
#include <string.h>

hook_entry_t* hook_entry_create(const char *class_name, const char *method_name,
                                const char *signature, jint access_flags,
                                void *hook_func, jmethodID orig) {
    hook_entry_t *entry = (hook_entry_t*)calloc(1, sizeof(hook_entry_t));
    if (!entry) return NULL;

    entry->class_name = strdup(class_name);
    entry->method.name = strdup(method_name);
    entry->method.signature = strdup(signature);

    if (!entry->class_name || !entry->method.name || !entry->method.signature) {
        hook_entry_destroy(entry);
        return NULL;
    }

    entry->method.access_flags = access_flags;
    entry->hook_function = hook_func;
    entry->original_method = orig;

    return entry;
}

void hook_entry_destroy(hook_entry_t *entry) {
    if (!entry) return;
    free(entry->class_name);
    free(entry->method.name);
    free(entry->method.signature);
    free(entry);
}

class_cache_t* class_cache_create(const char *class_name, const uint8_t *bytecode,
                                  size_t len, jclass orig) {
    class_cache_t *cache = (class_cache_t*)calloc(1, sizeof(class_cache_t));
    if (!cache) return NULL;

    cache->class_name = strdup(class_name);
    cache->bytecode = (uint8_t*)malloc(len);

    if (!cache->class_name || !cache->bytecode) {
        class_cache_destroy(cache);
        return NULL;
    }

    memcpy(cache->bytecode, bytecode, len);
    cache->bytecode_len = len;
    cache->original_class = orig;

    return cache;
}

void class_cache_destroy(class_cache_t *cache) {
    if (!cache) return;
    free(cache->class_name);
    free(cache->bytecode);
    free(cache);
}