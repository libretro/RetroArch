#pragma once

#include "coreinit/cache.h"
#include <coreinit/mutex.h>

class MutexWrapper {
public:
    MutexWrapper() = default;

    void init(const char *name) {
        OSInitMutexEx(&mutex, name);
    }

    void lock() {
        OSLockMutex(&mutex);
    }

    void unlock() {
        OSUnlockMutex(&mutex);
        OSMemoryBarrier();
    }

private:
    OSMutex mutex{};
};
