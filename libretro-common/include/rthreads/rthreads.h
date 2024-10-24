/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rthreads.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_RTHREADS_H__
#define __LIBRETRO_SDK_RTHREADS_H__

#include <retro_common_api.h>

#include <boolean.h>
#include <stdint.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>

RETRO_BEGIN_DECLS

/** Platform-agnostic handle to a thread. */
typedef struct sthread sthread_t;

/** Platform-agnostic handle to a mutex. */
typedef struct slock slock_t;

/** Platform-agnostic handle to a condition variable. */
typedef struct scond scond_t;

#ifdef HAVE_THREAD_STORAGE
/** Platform-agnostic handle to thread-local storage. */
typedef unsigned sthread_tls_t;
#endif

/**
 * Creates a new thread and starts running it.
 *
 * @param thread_func Function to run in the new thread.
 * Called with the value given in \c userdata as an argument.
 * @param userdata Pointer to anything (even \c NULL), passed directly to \c thread_func.
 * Must be cleaned up by the caller or the thread.
 * @return Pointer to the new thread,
 * or \c NULL if there was an error.
 * @warn Make sure that the thread can respond to cancellation requests,
 * especially if used in a core.
 * If a core-created thread isn't terminated by the time the core is unloaded,
 * it may leak into the frontend and cause undefined behavior
 * (especially if another session with the core is started).
 */
sthread_t *sthread_create(void (*thread_func)(void*), void *userdata);

/**
 * Creates a new thread with a specific priority hint and starts running it.
 *
 * @param thread_func Function to run in the new thread.
 * Called with the value given in \c userdata as an argument.
 * @param userdata Pointer to anything (even \c NULL), passed directly to \c thread_func.
 * Must be cleaned up by the caller or the thread.
 * @param thread_priority Priority hint for the new thread.
 * Threads with a higher number are more likely to be scheduled first.
 * Should be between 1 and 100, inclusive;
 * if not, the operating system will assign a default priority.
 * May be ignored.
 * @return Pointer to the new thread,
 * or \c NULL if there was an error.
 */
sthread_t *sthread_create_with_priority(void (*thread_func)(void*), void *userdata, int thread_priority);

/**
 * Detaches the given thread.
 *
 * When a detached thread terminates,
 * its resources are automatically released back to the operating system
 * without needing another thread to join with it.
 *
 * @param thread Thread to detach.
 * @return 0 on success, a non-zero error code on failure.
 * @warn Once a thread is detached, it cannot be joined.
 * @see sthread_join
 */
int sthread_detach(sthread_t *thread);

/**
 * Waits for the given thread to terminate.
 *
 * @param thread The thread to wait for.
 * Must be joinable.
 * Returns immediately if it's already terminated
 * or if it's \c NULL.
 */
void sthread_join(sthread_t *thread);

/**
 * Returns whether the given thread is the same as the calling thread.
 *
 * @param thread Thread to check.
 * @return \c true if \c thread is the same as the calling thread,
 * \c false if not or if it's \c NULL.
 * @note libretro does not have a notion of a "main" thread,
 * since the core may not be running on the same thread
 * that called \c main (or local equivalent).
 * @see sthread_get_thread_id
 */
bool sthread_isself(sthread_t *thread);

/**
 * Creates a new mutex (a.k.a. lock) that can be used to synchronize shared data.
 *
 * Must be manually freed with \c slock_free.
 *
 * @return Pointer to the new mutex,
 * or \c NULL if there was an error.
 */
slock_t *slock_new(void);

/**
 * Frees a mutex.
 *
 * Behavior is undefined if \c lock was previously freed.
 *
 * @param lock Pointer to the mutex to free.
 * May be \c NULL, in which this function does nothing.
 */
void slock_free(slock_t *lock);

/**
 * Locks a mutex, preventing other threads from claiming it until it's unlocked.
 *
 * If the mutex is already locked by another thread,
 * the calling thread will block until the mutex is unlocked.
 *
 * @param lock Pointer to the mutex to lock.
 * If \c NULL, will return without further action.
 * @see slock_try_lock
 */
void slock_lock(slock_t *lock);

/**
 * Tries to lock a mutex if it's not already locked by another thread.
 *
 * If the mutex is already in use by another thread,
 * returns immediately without waiting for it.
 *
 * @param lock The mutex to try to lock.
 * @return \c true if the mutex was successfully locked,
 * \c false if it was already locked by another thread or if \c lock is \c NULL.
 * @see slock_lock
 */
bool slock_try_lock(slock_t *lock);

/**
 * Unlocks a mutex, allowing other threads to claim it.
 *
 * @post The mutex is unlocked,
 * and another thread may lock it.
 * @param lock The mutex to unlock.
 * If \c NULL, this function is a no-op.
 */
void slock_unlock(slock_t *lock);

/**
 * Creates and initializes a condition variable.
 *
 * Must be manually freed with \c scond_free.
 *
 * @return Pointer to the new condition variable,
 * or \c NULL if there was an error.
 */
scond_t *scond_new(void);

/**
 * Frees a condition variable.
 *
 * @param cond Pointer to the condition variable to free.
 * If \c NULL, this function is a no-op.
 * Behavior is undefined if \c cond was previously freed.
 */
void scond_free(scond_t *cond);

/**
 * Blocks until the given condition variable is signaled or broadcast.
 *
 * @param cond Condition variable to wait on.
 * This function blocks until another thread
 * calls \c scond_signal or \c scond_broadcast with this condition variable.
 * @param lock Mutex to lock while waiting.
 *
 * @see scond_signal
 * @see scond_broadcast
 * @see scond_wait_timeout
 */
void scond_wait(scond_t *cond, slock_t *lock);

/**
 * Blocks until the given condition variable is signaled or broadcast,
 * or until the specified time has passed.
 *
 * @param cond Condition variable to wait on.
 * This function blocks until another thread
 * calls \c scond_signal or \c scond_broadcast with this condition variable.
 * @param lock Mutex to lock while waiting.
 * @param timeout_us Time to wait for a signal, in microseconds.
 *
 * @return \c false if \c timeout_us elapses
 * before \c cond is signaled or broadcast, otherwise \c true.
 */
bool scond_wait_timeout(scond_t *cond, slock_t *lock, int64_t timeout_us);

/**
 * Unblocks all threads waiting on the specified condition variable.
 *
 * @param cond Condition variable to broadcast.
 * @return 0 on success, non-zero on failure.
 */
int scond_broadcast(scond_t *cond);

/**
 * Unblocks at least one thread waiting on the specified condition variable.
 *
 * @param cond Condition variable to signal.
 */
void scond_signal(scond_t *cond);

#ifdef HAVE_THREAD_STORAGE
/**
 * Creates a thread-local storage key.
 *
 * Thread-local storage keys have a single value associated with them
 * that is unique to the thread that uses them.
 *
 * New thread-local storage keys have a value of \c NULL for all threads,
 * and new threads initialize all existing thread-local storage to \c NULL.
 *
 * @param tls[in,out] Pointer to the thread local storage key that will be initialized.
 * Must be cleaned up with \c sthread_tls_delete.
 * Behavior is undefined if \c NULL.
 * @return \c true if the operation succeeded, \c false otherwise.
 * @see sthread_tls_delete
 */
bool sthread_tls_create(sthread_tls_t *tls);

/**
 * Deletes a thread local storage key.
 *
 * The value must be cleaned up separately \em before calling this function,
 * if necessary.
 *
 * @param tls The thread local storage key to delete.
 * Behavior is undefined if \c NULL.
 * @return \c true if the operation succeeded, \c false otherwise.
 */
bool sthread_tls_delete(sthread_tls_t *tls);

/**
 * Gets the calling thread's local data for the given key.
 *
 * @param tls The thread-local storage key to get the data for.
 * @return The calling thread's local data associated with \c tls,
 * which should previously have been set with \c sthread_tls_set.
 * Will be \c NULL if this thread has not set a value or if there was an error.
 */
void *sthread_tls_get(sthread_tls_t *tls);

/**
 * Sets the calling thread's local data for the given key.
 *
 * @param tls The thread-local storage key to set the data for.
 * @param data Pointer to the data that will be associated with \c tls.
 * May be \c NULL.
 * @return \c true if \c data was successfully assigned to \c tls,
 * \c false if there was an error.
 */
bool sthread_tls_set(sthread_tls_t *tls, const void *data);
#endif

/**
 * Gets a thread's unique ID.
 *
 * @param thread The thread to get the ID of.
 * @return The provided thread's ID,
 * or 0 if it's \c NULL.
 */
uintptr_t sthread_get_thread_id(sthread_t *thread);

/**
 * Get the calling thread's unique ID.
 * @return The calling thread's ID.
 * @see sthread_get_thread_id
 */
uintptr_t sthread_get_current_thread_id(void);

RETRO_END_DECLS

#endif
