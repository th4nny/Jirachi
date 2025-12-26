//
// Created by Lodingglue on 12/24/2025.
//
#include "jnihook.h"
#include "internal.h"
#include "hook_manager.h"
#include "classfile.h"
#include "android_compat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

jnihook_state_t g_state = {0};

char* get_class_name(jvmtiEnv *jvmti, jclass clazz) {
    char *sig;

    if ((*jvmti)->GetClassSignature(jvmti, clazz, &sig, NULL) != JVMTI_ERROR_NONE) {
        return NULL;
    }

    /* Convert L<classname>; to <classname> */
    size_t len = strlen(sig);
    if (len < 3 || sig[0] != 'L' || sig[len-1] != ';') {
        (*jvmti)->Deallocate(jvmti, (unsigned char*)sig);
        return NULL;
    }

    char *class_name = strdup(sig + 1);
    if (class_name) {
        class_name[len - 2] = '\0';
    }

    (*jvmti)->Deallocate(jvmti, (unsigned char*)sig);
    return class_name;
}

char* get_method_name(jvmtiEnv *jvmti, jmethodID method) {
    char *name;

    if ((*jvmti)->GetMethodName(jvmti, method, &name, NULL, NULL) != JVMTI_ERROR_NONE) {
        return NULL;
    }

    char *result = strdup(name);
    (*jvmti)->Deallocate(jvmti, (unsigned char*)name);

    return result;
}

char* get_method_signature(jvmtiEnv *jvmti, jmethodID method) {
    char *sig;

    if ((*jvmti)->GetMethodName(jvmti, method, NULL, &sig, NULL) != JVMTI_ERROR_NONE) {
        return NULL;
    }

    char *result = strdup(sig);
    (*jvmti)->Deallocate(jvmti, (unsigned char*)sig);

    return result;
}

jint get_method_modifiers(jvmtiEnv *jvmti, jmethodID method) {
    jint mods;

    if ((*jvmti)->GetMethodModifiers(jvmti, method, &mods) != JVMTI_ERROR_NONE) {
        return 0;
    }

    return mods;
}

static void create_hook_key(char *buf, size_t buf_size, const char *class_name,
                           const char *method_name, const char *signature) {
    snprintf(buf, buf_size, "%s::%s%s", class_name, method_name, signature);
}

static void JNICALL class_file_load_hook(jvmtiEnv *jvmti, JNIEnv *jni,
                                         jclass class_being_redefined,
                                         jobject loader, const char *name,
                                         jobject protection_domain,
                                         jint class_data_len,
                                         const unsigned char *class_data,
                                         jint *new_class_data_len,
                                         unsigned char **new_class_data) {
    (void)jni;
    (void)class_being_redefined;
    (void)loader;
    (void)protection_domain;
    (void)new_class_data_len;
    (void)new_class_data;

    if (!name) return;

    mutex_lock(&g_state.lock);

    /* Check if class needs caching */
    class_cache_t *cached = (class_cache_t*)hashtable_get(g_state.class_cache, name);
    if (!cached) {
        cached = class_cache_create(name, class_data, class_data_len, NULL);
        if (cached) {
            hashtable_put(g_state.class_cache, name, cached);
        }
    }

    mutex_unlock(&g_state.lock);
}

/* Context for collecting hooks */
typedef struct {
    const char *target_class_name;
    JNINativeMethod *methods;
    int count;
    int capacity;
} reregister_context_t;

/* Callback for collecting native methods to register */
static void collect_native_method(const char *key, void *value, void *userdata) {
    (void)key;
    hook_entry_t *hook = (hook_entry_t*)value;
    reregister_context_t *ctx = (reregister_context_t*)userdata;

    if (strcmp(hook->class_name, ctx->target_class_name) == 0) {
        if (ctx->count < ctx->capacity) {
            ctx->methods[ctx->count].name = hook->method.name;
            ctx->methods[ctx->count].signature = hook->method.signature;
            ctx->methods[ctx->count].fnPtr = hook->hook_function;
            JNIHOOK_LOGD("  - %s%s -> %p",
                   ctx->methods[ctx->count].name,
                   ctx->methods[ctx->count].signature,
                   ctx->methods[ctx->count].fnPtr);
            ctx->count++;
        }
    }
}

/* Count hooks for a class */
static void count_class_hook(const char *key, void *value, void *userdata) {
    (void)key;
    hook_entry_t *hook = (hook_entry_t*)value;
    reregister_context_t *ctx = (reregister_context_t*)userdata;

    if (strcmp(hook->class_name, ctx->target_class_name) == 0) {
        ctx->count++;
    }
}

/* Re-register all native methods for a given class */
static jnihook_result_t reregister_class_natives(JNIEnv *env, const char *class_name, jclass clazz) {
    /* Count methods for this class */
    reregister_context_t ctx = {0};
    ctx.target_class_name = class_name;
    ctx.count = 0;

    hashtable_iterate(g_state.hooks, count_class_hook, &ctx);

    if (ctx.count == 0) {
        return JNIHOOK_OK;
    }

    JNIHOOK_LOGI("Re-registering %d native method(s) for class %s", ctx.count, class_name);

    /* Allocate array */
    JNINativeMethod *methods = (JNINativeMethod*)malloc(sizeof(JNINativeMethod) * ctx.count);
    if (!methods) {
        return JNIHOOK_ERR_ALLOC;
    }

    /* Collect methods */
    ctx.methods = methods;
    ctx.capacity = ctx.count;
    ctx.count = 0;

    hashtable_iterate(g_state.hooks, collect_native_method, &ctx);

    /* Register all */
    if ((*env)->RegisterNatives(env, clazz, methods, ctx.capacity) < 0) {
        JNIHOOK_LOGE("RegisterNatives failed");
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        free(methods);
        return JNIHOOK_ERR_JNI_OP;
    }

    JNIHOOK_LOGI("All native methods registered successfully");

    free(methods);
    return JNIHOOK_OK;
}

/* Context for collecting patch targets */
typedef struct {
    const char *target_class_name;
    hook_target_t *targets;
    int count;
    int capacity;
} collect_targets_context_t;

/* Callback for collecting patch targets */
static void collect_patch_target(const char *key, void *value, void *userdata) {
    (void)key;
    hook_entry_t *hook = (hook_entry_t*)value;
    collect_targets_context_t *ctx = (collect_targets_context_t*)userdata;

    if (strcmp(hook->class_name, ctx->target_class_name) == 0) {
        if (ctx->count < ctx->capacity) {
            ctx->targets[ctx->count].method_name = hook->method.name;
            ctx->targets[ctx->count].method_signature = hook->method.signature;
            ctx->count++;
        }
    }
}

/* Count targets for collection */
static void count_patch_target(const char *key, void *value, void *userdata) {
    (void)key;
    hook_entry_t *hook = (hook_entry_t*)value;
    collect_targets_context_t *ctx = (collect_targets_context_t*)userdata;

    if (strcmp(hook->class_name, ctx->target_class_name) == 0) {
        ctx->count++;
    }
}

/* Collect all hooks for a class and create patch targets */
static int collect_class_hooks(const char *class_name, hook_target_t **targets_out) {
    /* Count targets */
    collect_targets_context_t ctx = {0};
    ctx.target_class_name = class_name;
    ctx.count = 0;

    hashtable_iterate(g_state.hooks, count_patch_target, &ctx);

    if (ctx.count == 0) {
        *targets_out = NULL;
        return 0;
    }

    /* Allocate array */
    hook_target_t *targets = (hook_target_t*)malloc(sizeof(hook_target_t) * ctx.count);
    if (!targets) {
        *targets_out = NULL;
        return -1;
    }

    /* Collect targets */
    ctx.targets = targets;
    ctx.capacity = ctx.count;
    ctx.count = 0;

    hashtable_iterate(g_state.hooks, collect_patch_target, &ctx);

    *targets_out = targets;
    return ctx.capacity;
}

JNIHOOK_API jnihook_result_t JNIHOOK_CALL
jnihook_init(JavaVM *jvm) {
    if (g_state.initialized) {
        return JNIHOOK_ERR_ALREADY_INIT;
    }

    if (!jvm) {
        return JNIHOOK_ERR_INVALID_PARAM;
    }

    memset(&g_state, 0, sizeof(g_state));

#ifdef JNIHOOK_ANDROID
    /* Android-specific initialization */
    if (!android_init_jvmti(jvm, &g_state.jvmti)) {
        JNIHOOK_LOGE("Failed to initialize JVMTI for Android");
        return JNIHOOK_ERR_GET_JVMTI;
    }
    JNIHOOK_LOGI("JVMTI initialized for Android");
#else
    /* Standard JVM */
    if ((*jvm)->GetEnv(jvm, (void**)&g_state.jvmti, JVMTI_VERSION_1_2) != JNI_OK) {
        JNIHOOK_LOGE("Failed to get JVMTI environment");
        return JNIHOOK_ERR_GET_JVMTI;
    }
    JNIHOOK_LOGI("JVMTI environment obtained");
#endif

    /* Add capabilities */
    jvmtiCapabilities caps;
    memset(&caps, 0, sizeof(caps));
    caps.can_redefine_classes = 1;
    caps.can_retransform_classes = 1;
    caps.can_suspend = 1;

#ifdef JNIHOOK_ANDROID
    /* Check if capabilities are available on Android */
    if (!android_check_capability(g_state.jvmti, &caps)) {
        JNIHOOK_LOGW("Some JVMTI capabilities may not be available on this device");
        /* Continue anyway, some features may still work */
    }
#endif

    if ((*g_state.jvmti)->AddCapabilities(g_state.jvmti, &caps) != JVMTI_ERROR_NONE) {
        JNIHOOK_LOGE("Failed to add JVMTI capabilities");
        return JNIHOOK_ERR_ADD_CAPS;
    }

    /* Set callbacks */
    jvmtiEventCallbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.ClassFileLoadHook = class_file_load_hook;

    if ((*g_state.jvmti)->SetEventCallbacks(g_state.jvmti, &callbacks,
                                            sizeof(callbacks)) != JVMTI_ERROR_NONE) {
        JNIHOOK_LOGE("Failed to set event callbacks");
        return JNIHOOK_ERR_SET_CALLBACKS;
    }

    /* Initialize data structures */
    g_state.hooks = hashtable_create(256);
    g_state.class_cache = hashtable_create(128);

    if (!g_state.hooks || !g_state.class_cache) {
        hashtable_destroy(g_state.hooks, (void(*)(void*))hook_entry_destroy);
        hashtable_destroy(g_state.class_cache, (void(*)(void*))class_cache_destroy);
        JNIHOOK_LOGE("Failed to allocate internal data structures");
        return JNIHOOK_ERR_ALLOC;
    }

    if (!mutex_init(&g_state.lock)) {
        hashtable_destroy(g_state.hooks, (void(*)(void*))hook_entry_destroy);
        hashtable_destroy(g_state.class_cache, (void(*)(void*))class_cache_destroy);
        JNIHOOK_LOGE("Failed to initialize mutex");
        return JNIHOOK_ERR_THREAD_OP;
    }

    g_state.jvm = jvm;
    g_state.initialized = 1;

    JNIHOOK_LOGI("JNIHook library initialized successfully");
    return JNIHOOK_OK;
}

JNIHOOK_API jnihook_result_t JNIHOOK_CALL
jnihook_attach(jmethodID method, void *hook_function, jmethodID *out_original) {
    if (!g_state.initialized) {
        return JNIHOOK_ERR_NOT_INIT;
    }

    if (!method || !hook_function) {
        return JNIHOOK_ERR_INVALID_PARAM;
    }

    JNIEnv *env;
    if ((*g_state.jvm)->GetEnv(g_state.jvm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        JNIHOOK_LOGE("Failed to get JNI environment");
        return JNIHOOK_ERR_GET_JNI;
    }

    mutex_lock(&g_state.lock);

    /* Get method info */
    jclass clazz;
    if ((*g_state.jvmti)->GetMethodDeclaringClass(g_state.jvmti, method, &clazz) != JVMTI_ERROR_NONE) {
        mutex_unlock(&g_state.lock);
        JNIHOOK_LOGE("Failed to get method declaring class");
        return JNIHOOK_ERR_JVMTI_OP;
    }

    char *class_name = get_class_name(g_state.jvmti, clazz);
    char *method_name = get_method_name(g_state.jvmti, method);
    char *method_sig = get_method_signature(g_state.jvmti, method);
    jint access_flags = get_method_modifiers(g_state.jvmti, method);

    if (!class_name || !method_name || !method_sig) {
        free(class_name);
        free(method_name);
        free(method_sig);
        mutex_unlock(&g_state.lock);
        JNIHOOK_LOGE("Failed to get method information");
        return JNIHOOK_ERR_JVMTI_OP;
    }

    JNIHOOK_LOGI("Hooking %s.%s%s", class_name, method_name, method_sig);

    /* Create hook key */
    char hook_key[512];
    create_hook_key(hook_key, sizeof(hook_key), class_name, method_name, method_sig);

    /* Check if already hooked */
    if (hashtable_get(g_state.hooks, hook_key)) {
        free(class_name);
        free(method_name);
        free(method_sig);
        mutex_unlock(&g_state.lock);
        JNIHOOK_LOGW("Hook already exists for %s.%s%s", class_name, method_name, method_sig);
        return JNIHOOK_ERR_HOOK_EXISTS;
    }

    /* Create hook entry and store it BEFORE patching */
    hook_entry_t *entry = hook_entry_create(class_name, method_name, method_sig,
                                           access_flags, hook_function, method);

    if (!entry) {
        free(class_name);
        free(method_name);
        free(method_sig);
        mutex_unlock(&g_state.lock);
        JNIHOOK_LOGE("Failed to create hook entry");
        return JNIHOOK_ERR_ALLOC;
    }

    /* Store hook NOW so it will be included in the patch */
    hashtable_put(g_state.hooks, hook_key, entry);

    /* Cache class if not cached */
    class_cache_t *cached = (class_cache_t*)hashtable_get(g_state.class_cache, class_name);
    if (!cached) {
        JNIHOOK_LOGD("Caching class bytecode for %s", class_name);

        /* Enable class file load hook */
        (*g_state.jvmti)->SetEventNotificationMode(g_state.jvmti, JVMTI_ENABLE,
                                                    JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL);

        /* Trigger retransform to cache */
        jvmtiError err = (*g_state.jvmti)->RetransformClasses(g_state.jvmti, 1, &clazz);

        /* Disable hook */
        (*g_state.jvmti)->SetEventNotificationMode(g_state.jvmti, JVMTI_DISABLE,
                                                    JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL);

        if (err != JVMTI_ERROR_NONE) {
            JNIHOOK_LOGW("RetransformClasses returned error %d, but continuing...", err);
        }

        cached = (class_cache_t*)hashtable_get(g_state.class_cache, class_name);
        if (!cached) {
            hashtable_remove(g_state.hooks, hook_key);
            hook_entry_destroy(entry);
            free(class_name);
            free(method_name);
            free(method_sig);
            mutex_unlock(&g_state.lock);
            JNIHOOK_LOGE("Failed to cache class bytecode");
            return JNIHOOK_ERR_BYTECODE_PARSE;
        }
    }

    /* Parse and patch classfile */
    classfile_t *cf = classfile_parse(cached->bytecode, cached->bytecode_len);
    if (!cf) {
        hashtable_remove(g_state.hooks, hook_key);
        hook_entry_destroy(entry);
        free(class_name);
        free(method_name);
        free(method_sig);
        mutex_unlock(&g_state.lock);
        JNIHOOK_LOGE("Failed to parse classfile");
        return JNIHOOK_ERR_BYTECODE_PARSE;
    }

    /* Collect ALL hooks for this class (including the new one we just added) */
    hook_target_t *targets = NULL;
    int target_count = collect_class_hooks(class_name, &targets);

    if (target_count < 0) {
        classfile_destroy(cf);
        hashtable_remove(g_state.hooks, hook_key);
        hook_entry_destroy(entry);
        free(class_name);
        free(method_name);
        free(method_sig);
        mutex_unlock(&g_state.lock);
        JNIHOOK_LOGE("Failed to collect class hooks");
        return JNIHOOK_ERR_ALLOC;
    }

    JNIHOOK_LOGI("Patching %d method(s) in class %s", target_count, class_name);

    /* Patch ALL methods */
    if (!classfile_patch_methods(cf, targets, target_count)) {
        free(targets);
        classfile_destroy(cf);
        hashtable_remove(g_state.hooks, hook_key);
        hook_entry_destroy(entry);
        free(class_name);
        free(method_name);
        free(method_sig);
        mutex_unlock(&g_state.lock);
        JNIHOOK_LOGE("Failed to patch methods");
        return JNIHOOK_ERR_BYTECODE_PARSE;
    }

    free(targets);

    /* Generate new bytecode */
    size_t new_len;
    uint8_t *new_bytecode = classfile_generate(cf, &new_len);
    classfile_destroy(cf);

    if (!new_bytecode) {
        hashtable_remove(g_state.hooks, hook_key);
        hook_entry_destroy(entry);
        free(class_name);
        free(method_name);
        free(method_sig);
        mutex_unlock(&g_state.lock);
        JNIHOOK_LOGE("Failed to generate patched bytecode");
        return JNIHOOK_ERR_BYTECODE_PARSE;
    }

    JNIHOOK_LOGI("Redefining class with patched bytecode...");

    /* Redefine class */
    jvmtiClassDefinition class_def;
    class_def.klass = clazz;
    class_def.class_byte_count = (jint)new_len;
    class_def.class_bytes = new_bytecode;

    jvmtiError err = (*g_state.jvmti)->RedefineClasses(g_state.jvmti, 1, &class_def);
    free(new_bytecode);

    if (err != JVMTI_ERROR_NONE) {
        JNIHOOK_LOGE("RedefineClasses failed with error %d", err);
        hashtable_remove(g_state.hooks, hook_key);
        hook_entry_destroy(entry);
        free(class_name);
        free(method_name);
        free(method_sig);
        mutex_unlock(&g_state.lock);
        return JNIHOOK_ERR_JVMTI_OP;
    }

    JNIHOOK_LOGI("Class redefined successfully");

    /* Re-register ALL native methods for this class */
    jclass global_clazz = (jclass)(*env)->NewGlobalRef(env, clazz);
    jnihook_result_t rereg_result = reregister_class_natives(env, class_name, global_clazz);
    (*env)->DeleteGlobalRef(env, global_clazz);

    if (rereg_result != JNIHOOK_OK) {
        hashtable_remove(g_state.hooks, hook_key);
        hook_entry_destroy(entry);
        free(class_name);
        free(method_name);
        free(method_sig);
        mutex_unlock(&g_state.lock);
        JNIHOOK_LOGE("Failed to re-register native methods");
        return rereg_result;
    }

    if (out_original) {
        *out_original = method;
    }

    JNIHOOK_LOGI("Hook successfully installed for %s.%s%s", class_name, method_name, method_sig);

    free(class_name);
    free(method_name);
    free(method_sig);
    mutex_unlock(&g_state.lock);

    return JNIHOOK_OK;
}

JNIHOOK_API jnihook_result_t JNIHOOK_CALL
jnihook_detach(jmethodID method) {
    if (!g_state.initialized) {
        return JNIHOOK_ERR_NOT_INIT;
    }

    if (!method) {
        return JNIHOOK_ERR_INVALID_PARAM;
    }

    mutex_lock(&g_state.lock);

    /* Get method info */
    jclass clazz;
    if ((*g_state.jvmti)->GetMethodDeclaringClass(g_state.jvmti, method, &clazz) != JVMTI_ERROR_NONE) {
        mutex_unlock(&g_state.lock);
        JNIHOOK_LOGE("Failed to get method declaring class");
        return JNIHOOK_ERR_JVMTI_OP;
    }

    char *class_name = get_class_name(g_state.jvmti, clazz);
    char *method_name = get_method_name(g_state.jvmti, method);
    char *method_sig = get_method_signature(g_state.jvmti, method);

    if (!class_name || !method_name || !method_sig) {
        free(class_name);
        free(method_name);
        free(method_sig);
        mutex_unlock(&g_state.lock);
        JNIHOOK_LOGE("Failed to get method information");
        return JNIHOOK_ERR_JVMTI_OP;
    }

    JNIHOOK_LOGI("Detaching hook from %s.%s%s", class_name, method_name, method_sig);

    /* Create hook key */
    char hook_key[512];
    create_hook_key(hook_key, sizeof(hook_key), class_name, method_name, method_sig);

    /* Remove hook */
    hook_entry_t *entry = (hook_entry_t*)hashtable_get(g_state.hooks, hook_key);
    if (!entry) {
        free(class_name);
        free(method_name);
        free(method_sig);
        mutex_unlock(&g_state.lock);
        JNIHOOK_LOGW("Hook not found for %s.%s%s", class_name, method_name, method_sig);
        return JNIHOOK_ERR_HOOK_NOT_FOUND;
    }

    hashtable_remove(g_state.hooks, hook_key);

    /* Restore original class */
    class_cache_t *cached = (class_cache_t*)hashtable_get(g_state.class_cache, class_name);
    if (cached) {
        JNIHOOK_LOGD("Restoring original class bytecode");

        jvmtiClassDefinition class_def;
        class_def.klass = clazz;
        class_def.class_byte_count = (jint)cached->bytecode_len;
        class_def.class_bytes = cached->bytecode;

        jvmtiError err = (*g_state.jvmti)->RedefineClasses(g_state.jvmti, 1, &class_def);
        if (err != JVMTI_ERROR_NONE) {
            JNIHOOK_LOGW("Failed to restore original class (error %d)", err);
        } else {
            JNIHOOK_LOGI("Original class restored successfully");
        }
    }

    hook_entry_destroy(entry);
    free(class_name);
    free(method_name);
    free(method_sig);
    mutex_unlock(&g_state.lock);

    return JNIHOOK_OK;
}

JNIHOOK_API jnihook_result_t JNIHOOK_CALL
jnihook_shutdown(void) {
    if (!g_state.initialized) {
        return JNIHOOK_ERR_NOT_INIT;
    }

    JNIHOOK_LOGI("Shutting down JNIHook library");

    mutex_lock(&g_state.lock);

    hashtable_destroy(g_state.hooks, (void(*)(void*))hook_entry_destroy);
    hashtable_destroy(g_state.class_cache, (void(*)(void*))class_cache_destroy);

    g_state.initialized = 0;

    mutex_unlock(&g_state.lock);
    mutex_destroy(&g_state.lock);

    memset(&g_state, 0, sizeof(g_state));

    JNIHOOK_LOGI("JNIHook library shut down successfully");
    return JNIHOOK_OK;
}

JNIHOOK_API const char* JNIHOOK_CALL
jnihook_error_string(jnihook_result_t result) {
    switch (result) {
        case JNIHOOK_OK: return "Success";
        case JNIHOOK_ERR_ALREADY_INIT: return "Already initialized";
        case JNIHOOK_ERR_NOT_INIT: return "Not initialized";
        case JNIHOOK_ERR_GET_JVMTI: return "Failed to get JVMTI environment";
        case JNIHOOK_ERR_ADD_CAPS: return "Failed to add JVMTI capabilities";
        case JNIHOOK_ERR_SET_CALLBACKS: return "Failed to set callbacks";
        case JNIHOOK_ERR_GET_JNI: return "Failed to get JNI environment";
        case JNIHOOK_ERR_ALLOC: return "Memory allocation failed";
        case JNIHOOK_ERR_JVMTI_OP: return "JVMTI operation failed";
        case JNIHOOK_ERR_JNI_OP: return "JNI operation failed";
        case JNIHOOK_ERR_INVALID_PARAM: return "Invalid parameter";
        case JNIHOOK_ERR_HOOK_EXISTS: return "Hook already exists";
        case JNIHOOK_ERR_HOOK_NOT_FOUND: return "Hook not found";
        case JNIHOOK_ERR_BYTECODE_PARSE: return "Bytecode parsing failed";
        case JNIHOOK_ERR_THREAD_OP: return "Thread operation failed";
        default: return "Unknown error";
    }
}