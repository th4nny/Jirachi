#ifndef INTERNAL_H
#define INTERNAL_H

#include <jni.h>
#include <jvmti.h>
#include "hashtable.h"
#include "mutex.h"
#pragma once

#ifdef _WIN32
#define strdup _strdup
#endif

typedef struct {
    JavaVM *jvm;
    jvmtiEnv *jvmti;
    hashtable_t *hooks;
    hashtable_t *class_cache;
    mutex_t lock;
    int initialized;
} jnihook_state_t;

extern jnihook_state_t g_state;

char* get_class_name(jvmtiEnv *jvmti, jclass clazz);
char* get_method_name(jvmtiEnv *jvmti, jmethodID method);
char* get_method_signature(jvmtiEnv *jvmti, jmethodID method);
jint get_method_modifiers(jvmtiEnv *jvmti, jmethodID method);

#endif /* INTERNAL_H */


