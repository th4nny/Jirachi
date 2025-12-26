#ifndef MUTEX_H
#define MUTEX_H

#ifdef _WIN32
    #include <windows.h>
    typedef CRITICAL_SECTION mutex_t;
#else
#include <pthread.h>
typedef pthread_mutex_t mutex_t;
#endif

int mutex_init(mutex_t *mutex);
void mutex_destroy(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);

#endif /* MUTEX_H */