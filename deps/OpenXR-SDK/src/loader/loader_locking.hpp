// Copyright (c) 2026 libretro contributors
//
// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// libretro-common threading for the loader.  The loader used the C++11
// threading library (std::mutex, std::shared_timed_mutex) only; these
// helpers replace it with rthreads slocks plus retro_atomic, so the whole
// tree sits on one threading layer and the loader stops pulling in the
// libstdc++ thread machinery.

#pragma once

#include <rthreads/rthreads.h>
#include <retro_atomic.h>

// Stand-in for a function-local static std::mutex: zero-initialized at
// static storage duration (the atomic slot's trivial default construction
// leaves the zero-fill intact, so no magic-static guard is emitted),
// created on first use, published with a CAS, and - like the original -
// never destroyed.
struct LoaderLazySlock {
    retro_atomic_ptr_t slot;

    slock_t* Get() {
        slock_t* lock = static_cast<slock_t*>(retro_atomic_load_acquire_ptr(&slot));
        if (lock) {
            return lock;
        }
        lock = slock_new();
        if (retro_atomic_cas_ptr(&slot, NULL, lock)) {
            return lock;
        }
        slock_free(lock);
        return static_cast<slock_t*>(retro_atomic_load_acquire_ptr(&slot));
    }
};

// RAII guard over an slock, standing in for std::unique_lock /
// std::scoped_lock over a std::mutex.
class LoaderScopedSlock {
   public:
    explicit LoaderScopedSlock(slock_t* lock) : lock_(lock) { slock_lock(lock_); }
    ~LoaderScopedSlock() { slock_unlock(lock_); }

    LoaderScopedSlock(const LoaderScopedSlock&) = delete;
    LoaderScopedSlock& operator=(const LoaderScopedSlock&) = delete;

   private:
    slock_t* lock_;
};
