/* Copyright  (C) 2010-2018 The RetroArch team
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

typedef struct sthread sthread_t;
typedef struct slock slock_t;
typedef struct scond scond_t;

#ifdef HAVE_THREAD_STORAGE
typedef unsigned sthread_tls_t;
#endif

/**
 * sthread_create:
 * @start_routine           : thread entry callback function
 * @userdata                : pointer to userdata that will be made
 *                            available in thread entry callback function
 *
 * Create a new thread.
 *
 * Returns: pointer to new thread if successful, otherwise NULL.
 */
sthread_t *sthread_create(void (*thread_func)(void*), void *userdata);

/**
 * sthread_create_with_priority:
 * @start_routine           : thread entry callback function
 * @userdata                : pointer to userdata that will be made
 *                            available in thread entry callback function
 * @thread_priority         : thread priority hint value from [1-100]
 *
 * Create a new thread. It is possible for the caller to give a hint
 * for the thread's priority from [1-100]. Any passed in @thread_priority
 * values that are outside of this range will cause sthread_create() to
 * create a new thread using the operating system's default thread
 * priority.
 *
 * Returns: pointer to new thread if successful, otherwise NULL.
 */
sthread_t *sthread_create_with_priority(void (*thread_func)(void*), void *userdata, int thread_priority);

/**
 * sthread_detach:
 * @thread                  : pointer to thread object
 *
 * Detach a thread. When a detached thread terminates, its
 * resource sare automatically released back to the system
 * without the need for another thread to join with the
 * terminated thread.
 *
 * Returns: 0 on success, otherwise it returns a non-zero error number.
 */
int sthread_detach(sthread_t *thread);

/**
 * sthread_join:
 * @thread                  : pointer to thread object
 *
 * Join with a terminated thread. Waits for the thread specified by
 * @thread to terminate. If that thread has already terminated, then
 * it will return immediately. The thread specified by @thread must
 * be joinable.
 *
 * Returns: 0 on success, otherwise it returns a non-zero error number.
 */
void sthread_join(sthread_t *thread);

/**
 * sthread_isself:
 * @thread                  : pointer to thread object
 *
 * Returns: true (1) if calling thread is the specified thread
 */
bool sthread_isself(sthread_t *thread);

/**
 * slock_new:
 *
 * Create and initialize a new mutex. Must be manually
 * freed.
 *
 * Returns: pointer to a new mutex if successful, otherwise NULL.
 **/
slock_t *slock_new(void);

/**
 * slock_free:
 * @lock                    : pointer to mutex object
 *
 * Frees a mutex.
 **/
void slock_free(slock_t *lock);

/**
 * slock_lock:
 * @lock                    : pointer to mutex object
 *
 * Locks a mutex. If a mutex is already locked by
 * another thread, the calling thread shall block until
 * the mutex becomes available.
**/
void slock_lock(slock_t *lock);

/**
 * slock_try_lock:
 * @lock                    : pointer to mutex object
 *
 * Attempts to lock a mutex. If a mutex is already locked by
 * another thread, return false.  If the lock is acquired, return true.
**/
bool slock_try_lock(slock_t *lock);

/**
 * slock_unlock:
 * @lock                    : pointer to mutex object
 *
 * Unlocks a mutex.
 **/
void slock_unlock(slock_t *lock);

/**
 * scond_new:
 *
 * Creates and initializes a condition variable. Must
 * be manually freed.
 *
 * Returns: pointer to new condition variable on success,
 * otherwise NULL.
 **/
scond_t *scond_new(void);

/**
 * scond_free:
 * @cond                    : pointer to condition variable object
 *
 * Frees a condition variable.
**/
void scond_free(scond_t *cond);

/**
 * scond_wait:
 * @cond                    : pointer to condition variable object
 * @lock                    : pointer to mutex object
 *
 * Block on a condition variable (i.e. wait on a condition).
 **/
void scond_wait(scond_t *cond, slock_t *lock);

/**
 * scond_wait_timeout:
 * @cond                    : pointer to condition variable object
 * @lock                    : pointer to mutex object
 * @timeout_us              : timeout (in microseconds)
 *
 * Try to block on a condition variable (i.e. wait on a condition) until
 * @timeout_us elapses.
 *
 * Returns: false (0) if timeout elapses before condition variable is
 * signaled or broadcast, otherwise true (1).
 **/
bool scond_wait_timeout(scond_t *cond, slock_t *lock, int64_t timeout_us);

/**
 * scond_broadcast:
 * @cond                    : pointer to condition variable object
 *
 * Broadcast a condition. Unblocks all threads currently blocked
 * on the specified condition variable @cond.
 **/
int scond_broadcast(scond_t *cond);

/**
 * scond_signal:
 * @cond                    : pointer to condition variable object
 *
 * Signal a condition. Unblocks at least one of the threads currently blocked
 * on the specified condition variable @cond.
 **/
void scond_signal(scond_t *cond);

#ifdef HAVE_THREAD_STORAGE
/**
 * @brief Creates a thread local storage key
 *
 * This function shall create thread-specific data key visible to all threads in
 * the process. The same key can be used by multiple threads to store
 * thread-local data.
 *
 * When the key is created NULL shall be associated with it in all active
 * threads. Whenever a new thread is spawned the all defined keys will be
 * associated with NULL on that thread.
 *
 * @param tls
 * @return whether the operation suceeded or not
 */
bool sthread_tls_create(sthread_tls_t *tls);

/**
 * @brief Deletes a thread local storage
 * @param tls
 * @return whether the operation suceeded or not
 */
bool sthread_tls_delete(sthread_tls_t *tls);

/**
 * @brief Retrieves thread specific data associated with a key
 *
 * There is no way to tell whether this function failed.
 *
 * @param tls
 * @return
 */
void *sthread_tls_get(sthread_tls_t *tls);

/**
 * @brief Binds thread specific data to a key
 * @param tls
 * @return Whether the operation suceeded or not
 */
bool sthread_tls_set(sthread_tls_t *tls, const void *data);
#endif

/*
 * @brief Get thread ID of specified thread
 * @param thread
 * @return The ID of the specified thread
 */
uintptr_t sthread_get_thread_id(sthread_t *thread);

/*
 * @brief Get thread ID of the current thread
 * @param 
 * @return The ID of the current thread
 */
uintptr_t sthread_get_current_thread_id(void);

RETRO_END_DECLS

#endif
