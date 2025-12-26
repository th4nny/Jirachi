//
// Created by Lodingglue on 12/25/2025.
//
#include "android_compat.h"

#ifdef JNIHOOK_ANDROID

#include <sys/system_properties.h>
#include <string.h>
#include <stdlib.h>

int android_get_api_level(void) {
    char sdk_ver_str[PROP_VALUE_MAX] = {0};

    if (__system_property_get("ro.build.version.sdk", sdk_ver_str) > 0) {
        return atoi(sdk_ver_str);
    }

    return 0;
}

int android_check_jvmti_support(void) {
    int api_level = android_get_api_level();


    if (api_level >= 26) {
        JNIHOOK_LOGI("Android API level %d - Full JVMTI support available", api_level);
        return 1;
    } else if (api_level > 0) {
        JNIHOOK_LOGE("Android API level %d - JVMTI not supported (requires API 26+)", api_level);
        return 0;
    }

    JNIHOOK_LOGW("Could not determine Android API level");
    return 0;
}

int android_check_capability(jvmtiEnv *jvmti, jvmtiCapabilities *caps) {
    if (!jvmti || !caps) return 0;

    jvmtiCapabilities available_caps;
    memset(&available_caps, 0, sizeof(available_caps));

    if ((*jvmti)->GetPotentialCapabilities(jvmti, &available_caps) != JVMTI_ERROR_NONE) {
        JNIHOOK_LOGE("Failed to get potential capabilities");
        return 0;
    }

    /* Check if requested capabilities are available */
    int all_available = 1;

    if (caps->can_redefine_classes && !available_caps.can_redefine_classes) {
        JNIHOOK_LOGW("Capability 'can_redefine_classes' not available");
        all_available = 0;
    }

    if (caps->can_retransform_classes && !available_caps.can_retransform_classes) {
        JNIHOOK_LOGW("Capability 'can_retransform_classes' not available");
        all_available = 0;
    }

    if (caps->can_suspend && !available_caps.can_suspend) {
        JNIHOOK_LOGW("Capability 'can_suspend' not available");
        all_available = 0;
    }

    return all_available;
}

int android_init_jvmti(JavaVM *jvm, jvmtiEnv **jvmti_out) {
    if (!jvm || !jvmti_out) return 0;

    /* Check Android version support */
    if (!android_check_jvmti_support()) {
        JNIHOOK_LOGE("JVMTI not supported on this Android version");
        return 0;
    }

    /* Get JVMTI environment */
    jvmtiEnv *jvmti = NULL;
    jint result = (*jvm)->GetEnv(jvm, (void**)&jvmti, JVMTI_VERSION_1_2);

    if (result != JNI_OK || !jvmti) {
        /* Try JVMTI 1.1 as fallback */
        result = (*jvm)->GetEnv(jvm, (void**)&jvmti, JVMTI_VERSION_1_1);
        if (result != JNI_OK || !jvmti) {
            JNIHOOK_LOGE("Failed to get JVMTI environment (error: %d)", result);
            return 0;
        }
        JNIHOOK_LOGW("Using JVMTI 1.1 (1.2 not available)");
    } else {
        JNIHOOK_LOGI("JVMTI 1.2 environment obtained");
    }

    *jvmti_out = jvmti;
    return 1;
}

#endif /* JNIHOOK_ANDROID */