//
// Created by Lodingglue on 12/25/2025.
//

/**
 * This will not work because Android does not use standard JVMTI.
 * Instead, it has its own implementation called ART TI.
 *
 * Even if ART TI is successfully initialized, it only works on
 * SDK 26 (Android 8.0) devices.
 *
 * Additionally, the library must be loaded as an agent in order
 * to function correctly, which requires it to be loaded before
 * the Application context is created.
 *
 * This can be achieved by using a ContentProvider to load the
 * library as an agent via the Android SDK Debug.java class.
 *
 * Although this approach should not normally work, but since it does,
 * it can enable a lot of powerful and interesting use cases. :p
 */



#ifndef ANDROID_COMPAT_H
#define ANDROID_COMPAT_H

#ifdef JNIHOOK_ANDROID

#include <jni.h>
#include <jvmti.h>


#include <android/log.h>
#define JNIHOOK_LOG_TAG "JNIHook"
#define JNIHOOK_LOGI(...) __android_log_print(ANDROID_LOG_INFO, JNIHOOK_LOG_TAG, __VA_ARGS__)
#define JNIHOOK_LOGW(...) __android_log_print(ANDROID_LOG_WARN, JNIHOOK_LOG_TAG, __VA_ARGS__)
#define JNIHOOK_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, JNIHOOK_LOG_TAG, __VA_ARGS__)
#define JNIHOOK_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, JNIHOOK_LOG_TAG, __VA_ARGS__)

/* Android ART limitations and workarounds */


/* Check if Android version supports JVMTI */
int android_check_jvmti_support(void);

/* Get Android API level */
int android_get_api_level(void);

/* Check if specific JVMTI capability is supported */
int android_check_capability(jvmtiEnv *jvmti, jvmtiCapabilities *caps);

/* Android-specific initialization */
int android_init_jvmti(JavaVM *jvm, jvmtiEnv **jvmti_out);

#else

#include <stdio.h>
#define JNIHOOK_LOGI(...) printf("[INFO] " __VA_ARGS__); printf("\n"); fflush(stdout)
#define JNIHOOK_LOGW(...) printf("[WARN] " __VA_ARGS__); printf("\n"); fflush(stdout)
#define JNIHOOK_LOGE(...) fprintf(stderr, "[ERROR] " __VA_ARGS__); fprintf(stderr, "\n"); fflush(stderr)
#define JNIHOOK_LOGD(...) printf("[DEBUG] " __VA_ARGS__); printf("\n"); fflush(stdout)

#endif /* JNIHOOK_ANDROID */

#endif /* ANDROID_COMPAT_H */