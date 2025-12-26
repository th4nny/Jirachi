//
// Created by Lodingglue on 12/24/2025.
//

#include "mutex.h"

int mutex_init(mutex_t *mutex) {
    if (!mutex) return 0;
#ifdef _WIN32
    InitializeCriticalSection(mutex);
    return 1;
#else
    return pthread_mutex_init(mutex, NULL) == 0;
#endif
}

void mutex_destroy(mutex_t *mutex) {
    if (!mutex) return;
#ifdef _WIN32
    DeleteCriticalSection(mutex);
#else
    pthread_mutex_destroy(mutex);
#endif
}

void mutex_lock(mutex_t *mutex) {
    if (!mutex) return;
#ifdef _WIN32
    EnterCriticalSection(mutex);
#else
    pthread_mutex_lock(mutex);
#endif
}

void mutex_unlock(mutex_t *mutex) {
    if (!mutex) return;
#ifdef _WIN32
    LeaveCriticalSection(mutex);
#else
    pthread_mutex_unlock(mutex);
#endif
}