#include <jni.h>
#include <jvmti.h>
#include "jnihook.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    #define sleep_ms(ms) Sleep(ms)
#else
    #include <unistd.h>
    #define sleep_ms(ms) usleep((ms) * 1000)
#endif

#define UNUSED(x) (void)(x)

static JavaVM *g_jvm = NULL;
static jvmtiEnv *g_jvmti = NULL;
static int hooks_installed = 0;
static int installing_hooks = 0; /* Prevent re-entry */

/* Hook for getMessage() */
jstring JNICALL hooked_getMessage(JNIEnv *env, jobject thisObj) {
    UNUSED(thisObj);
    printf("[HOOK] getMessage() intercepted!\n");
    fflush(stdout);
    return (*env)->NewStringUTF(env, "Hooked Message!");
}

/* Hook for calculate() */
jint JNICALL hooked_calculate(JNIEnv *env, jobject thisObj, jint a, jint b) {
    UNUSED(env);
    UNUSED(thisObj);
    printf("[HOOK] calculate(%d, %d) intercepted! Changed behavior: multiply instead of add\n", a, b);
    fflush(stdout);
    return a * b;
}

/* Install hooks on TestClass */
static void install_hooks(JNIEnv *env, jclass test_class) {
    if (hooks_installed || installing_hooks) {
        return;
    }

    installing_hooks = 1; /* Set flag to prevent re-entry */

    printf("\n[AGENT] Installing hooks on TestClass...\n");
    printf("[AGENT] NOTE: Hooking both methods together to avoid class redefinition conflicts\n");
    fflush(stdout);

    /* Get getMessage method */
    jmethodID getMessage = (*env)->GetMethodID(
        env, test_class, "getMessage", "()Ljava/lang/String;");
    if (!getMessage) {
        fprintf(stderr, "[AGENT] ERROR: Failed to find getMessage method\n");
        (*env)->ExceptionDescribe(env);
        installing_hooks = 0;
        return;
    }

    /* Get calculate method */
    jmethodID calculate = (*env)->GetMethodID(
        env, test_class, "calculate", "(II)I");
    if (!calculate) {
        fprintf(stderr, "[AGENT] ERROR: Failed to find calculate method\n");
        (*env)->ExceptionDescribe(env);
        installing_hooks = 0;
        return;
    }

    printf("[AGENT] Attempting to hook getMessage (ID: %p)...\n", (void*)getMessage);
    fflush(stdout);

    /* Hook getMessage first */
    jnihook_result_t result = jnihook_attach(
        getMessage,
        (void*)hooked_getMessage,
        NULL
    );
    if (result != JNIHOOK_OK) {
        fprintf(stderr, "[AGENT] ERROR: Failed to hook getMessage: %s\n",
                jnihook_error_string(result));
        installing_hooks = 0;
        return;
    }
    printf("[AGENT] Successfully hooked getMessage() ✓\n");
    fflush(stdout);

    sleep_ms(100);

    printf("[AGENT] Attempting to hook calculate (ID: %p)...\n", (void*)calculate);
    fflush(stdout);

    result = jnihook_attach(
        calculate,
        (void*)hooked_calculate,
        NULL
    );
    if (result != JNIHOOK_OK) {
        fprintf(stderr, "[AGENT] ERROR: Failed to hook calculate: %s\n",
                jnihook_error_string(result));
        installing_hooks = 0;
        return;
    }
    printf("[AGENT] Successfully hooked calculate() ✓\n");
    printf("[AGENT] All hooks installed successfully!\n\n");
    fflush(stdout);

    hooks_installed = 1;
    installing_hooks = 0;
}

/* ClassFileLoadHook callback - intercept class loading */
static void JNICALL class_file_load_hook(jvmtiEnv *jvmti_env,
                                         JNIEnv *jni_env,
                                         jclass class_being_redefined,
                                         jobject loader,
                                         const char *name,
                                         jobject protection_domain,
                                         jint class_data_len,
                                         const unsigned char *class_data,
                                         jint *new_class_data_len,
                                         unsigned char **new_class_data) {
    UNUSED(jvmti_env);
    UNUSED(loader);
    UNUSED(protection_domain);
    UNUSED(class_data_len);
    UNUSED(class_data);
    UNUSED(new_class_data_len);
    UNUSED(new_class_data);

    /* Only hook if TestClass is being loaded for the first time (not redefined) */
    if (!hooks_installed && !installing_hooks &&
        !class_being_redefined && name && strcmp(name, "TestClass") == 0) {

        printf("[AGENT] TestClass detected during load hook\n");
        fflush(stdout);
    }
}

/* ClassPrepare callback - class is ready to use */
static void JNICALL class_prepare_callback(jvmtiEnv *jvmti, JNIEnv *env,
                                           jthread thread, jclass klass) {
    UNUSED(jvmti);
    UNUSED(thread);

    if (hooks_installed || installing_hooks) {
        return;
    }

    /* Get class signature */
    char *signature = NULL;
    if ((*jvmti)->GetClassSignature(jvmti, klass, &signature, NULL) != JVMTI_ERROR_NONE) {
        return;
    }

    /* Check if this is TestClass */
    if (signature && strcmp(signature, "LTestClass;") == 0) {
        printf("[AGENT] TestClass prepared, installing hooks...\n");
        fflush(stdout);

        /* Now we can safely install hooks */
        install_hooks(env, klass);
    }

    if (signature) {
        (*jvmti)->Deallocate(jvmti, (unsigned char*)signature);
    }
}

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) {
    UNUSED(options);
    UNUSED(reserved);

    printf("========================================\n");
    printf("JNIHook Test Agent Loading\n");
    printf("========================================\n");
    fflush(stdout);

    g_jvm = jvm;

    /* Get JVMTI environment first */
    if ((*jvm)->GetEnv(jvm, (void**)&g_jvmti, JVMTI_VERSION_1_2) != JNI_OK) {
        fprintf(stderr, "[AGENT] ERROR: Failed to get JVMTI environment\n");
        return JNI_ERR;
    }

    /* Add required capabilities */
    jvmtiCapabilities caps;
    memset(&caps, 0, sizeof(caps));
    caps.can_generate_all_class_hook_events = 1;

    if ((*g_jvmti)->AddCapabilities(g_jvmti, &caps) != JVMTI_ERROR_NONE) {
        fprintf(stderr, "[AGENT] ERROR: Failed to add capabilities\n");
        return JNI_ERR;
    }

    /* Initialize jnihook */
    jnihook_result_t result = jnihook_init(jvm);
    if (result != JNIHOOK_OK) {
        fprintf(stderr, "[AGENT] ERROR: Failed to initialize jnihook: %s\n",
                jnihook_error_string(result));
        return JNI_ERR;
    }
    printf("[AGENT] JNIHook library initialized\n");
    fflush(stdout);

    /* Set up callbacks */
    jvmtiEventCallbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.ClassFileLoadHook = class_file_load_hook;
    callbacks.ClassPrepare = class_prepare_callback;

    if ((*g_jvmti)->SetEventCallbacks(g_jvmti, &callbacks, sizeof(callbacks)) != JVMTI_ERROR_NONE) {
        fprintf(stderr, "[AGENT] ERROR: Failed to set event callbacks\n");
        return JNI_ERR;
    }

    /* Enable ClassFileLoadHook event */
    if ((*g_jvmti)->SetEventNotificationMode(g_jvmti, JVMTI_ENABLE,
                                             JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL) != JVMTI_ERROR_NONE) {
        fprintf(stderr, "[AGENT] ERROR: Failed to enable ClassFileLoadHook event\n");
        return JNI_ERR;
    }

    /* Enable ClassPrepare event */
    if ((*g_jvmti)->SetEventNotificationMode(g_jvmti, JVMTI_ENABLE,
                                             JVMTI_EVENT_CLASS_PREPARE, NULL) != JVMTI_ERROR_NONE) {
        fprintf(stderr, "[AGENT] ERROR: Failed to enable ClassPrepare event\n");
        return JNI_ERR;
    }

    printf("[AGENT] Waiting for TestClass to load...\n");
    fflush(stdout);
    return JNI_OK;
}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *jvm) {
    UNUSED(jvm);

    printf("\n========================================\n");
    printf("JNIHook Test Agent Unloading\n");
    printf("========================================\n");
    fflush(stdout);

    if (hooks_installed) {
        printf("[AGENT] Test completed successfully!\n");
        printf("[AGENT] Hooks were installed and executed\n");
    } else {
        printf("[AGENT] WARNING: Hooks were not installed\n");
    }
    fflush(stdout);

    jnihook_shutdown();
    printf("[AGENT] JNIHook library shut down\n");
    fflush(stdout);
}