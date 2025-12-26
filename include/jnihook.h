#ifndef JNIHOOK_H
#define JNIHOOK_H

#include <jni.h>
#include <jvmti.h>

#ifdef _WIN32
    #define JNIHOOK_API __declspec(dllexport)
#else
    #define JNIHOOK_API __attribute__((visibility("default")))
#endif

#define JNIHOOK_CALL

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum {
        JNIHOOK_OK = 0,
        JNIHOOK_ERR_ALREADY_INIT,
        JNIHOOK_ERR_NOT_INIT,
        JNIHOOK_ERR_GET_JVMTI,
        JNIHOOK_ERR_ADD_CAPS,
        JNIHOOK_ERR_SET_CALLBACKS,
        JNIHOOK_ERR_GET_JNI,
        JNIHOOK_ERR_ALLOC,
        JNIHOOK_ERR_JVMTI_OP,
        JNIHOOK_ERR_JNI_OP,
        JNIHOOK_ERR_INVALID_PARAM,
        JNIHOOK_ERR_HOOK_EXISTS,
        JNIHOOK_ERR_HOOK_NOT_FOUND,
        JNIHOOK_ERR_BYTECODE_PARSE,
        JNIHOOK_ERR_THREAD_OP
    } jnihook_result_t;

    /**
     * Initialize the JNI hooking library
     * @param jvm Pointer to the Java Virtual Machine
     * @return JNIHOOK_OK on success, error code otherwise
     */
    JNIHOOK_API jnihook_result_t JNIHOOK_CALL
    jnihook_init(JavaVM *jvm);

    /**
     * Attach a hook to a Java method
     * @param method The Java method to hook
     * @param hook_function Native function to call instead
     * @param out_original Optional pointer to receive original method reference
     * @return JNIHOOK_OK on success, error code otherwise
     */
    JNIHOOK_API jnihook_result_t JNIHOOK_CALL
    jnihook_attach(jmethodID method, void *hook_function, jmethodID *out_original);

    /**
     * Detach a hook from a Java method
     * @param method The Java method to unhook
     * @return JNIHOOK_OK on success, error code otherwise
     */
    JNIHOOK_API jnihook_result_t JNIHOOK_CALL
    jnihook_detach(jmethodID method);

    /**
     * Shutdown the library and cleanup all resources
     * @return JNIHOOK_OK on success, error code otherwise
     */
    JNIHOOK_API jnihook_result_t JNIHOOK_CALL
    jnihook_shutdown(void);

    /**
     * Get string description of error code
     * @param result Error code
     * @return Human-readable error string
     */
    JNIHOOK_API const char* JNIHOOK_CALL
    jnihook_error_string(jnihook_result_t result);

#ifdef __cplusplus
}
#endif

#endif /* JNIHOOK_H */