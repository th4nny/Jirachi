#ifndef HOOK_MANAGER_H
#define HOOK_MANAGER_H

#include <jni.h>
#include "hashtable.h"

typedef struct {
    char *name;
    char *signature;
    jint access_flags;
} method_info_t;

typedef struct {
    char *class_name;
    method_info_t method;
    void *hook_function;
    jmethodID original_method;
} hook_entry_t;

typedef struct {
    char *class_name;
    uint8_t *bytecode;
    size_t bytecode_len;
    jclass original_class;
} class_cache_t;

hook_entry_t* hook_entry_create(const char *class_name, const char *method_name,
                                const char *signature, jint access_flags,
                                void *hook_func, jmethodID orig);
void hook_entry_destroy(hook_entry_t *entry);

class_cache_t* class_cache_create(const char *class_name, const uint8_t *bytecode,
                                  size_t len, jclass orig);
void class_cache_destroy(class_cache_t *cache);

#endif /* HOOK_MANAGER_H */