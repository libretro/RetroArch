/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_shared_eventcount.c).
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

/* struct timespec and syscall() are outside strict C89; asked for the
 * way retro_eventcount.c asks, for the same reasons. */
#if defined(__unix__) && !defined(__APPLE__) && !defined(__sun__)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309
#endif
#endif
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <stddef.h>

#include <rthreads/retro_shared_eventcount.h>

#if !defined(RETRO_ATOMIC_LOCK_FREE)
/* Atomics that are a lock or a compiler barrier do not reach another
 * process: nothing to build on */
#elif defined(__linux__) && !defined(ANDROID_NO_FUTEX)
#define RETRO_SEC_LINUX 1
#elif defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#define RETRO_SEC_WIN32 1
#endif

#if defined(RETRO_SEC_LINUX)
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#elif defined(RETRO_SEC_WIN32)
#include <windows.h>
#endif

/* The futex is handed the epoch as a bare 32-bit word */
typedef char retro_shared_eventcount_epoch_is_a_word_
   [(sizeof(retro_atomic_int_t) == sizeof(int)) ? 1 : -1];

void retro_shared_eventcount_init(retro_shared_eventcount_t *shared)
{
   retro_atomic_int_init(&shared->epoch, 0);
   retro_atomic_int_init(&shared->waiters, 0);
}

bool retro_shared_eventcount_attach(retro_shared_eventcount_handle_t *handle,
      retro_shared_eventcount_t *shared, const char *name)
{
   handle->shared = NULL;
   handle->os     = NULL;
#if defined(RETRO_SEC_LINUX)
   (void)name;
   handle->shared = shared;
   return true;
#elif defined(RETRO_SEC_WIN32)
   {
      /* Made by whichever process comes first, opened by the others */
      HANDLE semaphore = CreateSemaphoreA(NULL, 0, 0x7fffffff, name);
      if (!semaphore)
         return false;
      handle->shared = shared;
      handle->os     = semaphore;
      return true;
   }
#else
   (void)shared;
   (void)name;
   return false;
#endif
}

void retro_shared_eventcount_detach(retro_shared_eventcount_handle_t *handle)
{
#if defined(RETRO_SEC_WIN32)
   if (handle->os)
      CloseHandle((HANDLE)handle->os);
#endif
   handle->shared = NULL;
   handle->os     = NULL;
}

#if defined(RETRO_SEC_LINUX) || defined(RETRO_SEC_WIN32)

void retro_shared_eventcount_notify(retro_shared_eventcount_handle_t *handle)
{
   retro_shared_eventcount_t *shared = handle->shared;
   int waiters;
   /* The sequentially-consistent bump and load pair with prepare's
    * register-then-load: a notify cannot read no waiters while a
    * registering waiter reads the epoch from before it */
   retro_atomic_fetch_add_seq_cst_int(&shared->epoch, 1);
   if ((waiters = retro_atomic_load_seq_cst_int(&shared->waiters)) == 0)
      return;
#if defined(RETRO_SEC_LINUX)
   (void)waiters;
   syscall(SYS_futex, (void*)&shared->epoch, FUTEX_WAKE,
         INT_MAX, NULL, NULL, 0);
#else
   /* A permit per thread in a window.  One that then finds the epoch
    * moved and does not sleep leaves its permit behind, and a later
    * wait returns early on it, which the waiting loop allows for */
   ReleaseSemaphore((HANDLE)handle->os, waiters, NULL);
#endif
}

int retro_shared_eventcount_prepare_wait(
      retro_shared_eventcount_handle_t *handle)
{
   retro_atomic_fetch_add_seq_cst_int(&handle->shared->waiters, 1);
   return retro_atomic_load_seq_cst_int(&handle->shared->epoch);
}

void retro_shared_eventcount_cancel_wait(
      retro_shared_eventcount_handle_t *handle)
{
   retro_atomic_fetch_sub_int(&handle->shared->waiters, 1);
}

#if defined(RETRO_SEC_LINUX)
/* Nanoseconds left until @deadline on the monotonic clock */
static int64_t retro_sec_left(const struct timespec *deadline)
{
   struct timespec now;
   if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
      return 0;
   return ((int64_t)deadline->tv_sec - (int64_t)now.tv_sec) * 1000000000
      + (deadline->tv_nsec - now.tv_nsec);
}
#endif

void retro_shared_eventcount_commit_wait(
      retro_shared_eventcount_handle_t *handle, int key)
{
   retro_shared_eventcount_t *shared = handle->shared;
#if defined(RETRO_SEC_LINUX)
   /* The kernel sleeps only while the word still holds @key; a wake,
    * a signal or a spurious return all come back here */
   while (retro_atomic_load_acquire_int(&shared->epoch) == key)
      syscall(SYS_futex, (void*)&shared->epoch, FUTEX_WAIT,
            key, NULL, NULL, 0);
#else
   if (retro_atomic_load_acquire_int(&shared->epoch) == key)
      WaitForSingleObject((HANDLE)handle->os, INFINITE);
#endif
   retro_atomic_fetch_sub_int(&shared->waiters, 1);
}

bool retro_shared_eventcount_commit_wait_timeout(
      retro_shared_eventcount_handle_t *handle, int key, int64_t timeout_us)
{
   retro_shared_eventcount_t *shared = handle->shared;
   bool notified;
#if defined(RETRO_SEC_LINUX)
   struct timespec deadline;
   if (timeout_us > 0 && clock_gettime(CLOCK_MONOTONIC, &deadline) == 0)
   {
      deadline.tv_sec  += (time_t)(timeout_us / 1000000);
      deadline.tv_nsec += (long)(timeout_us % 1000000) * 1000;
      if (deadline.tv_nsec >= 1000000000L)
      {
         deadline.tv_sec++;
         deadline.tv_nsec -= 1000000000L;
      }
      while (retro_atomic_load_acquire_int(&shared->epoch) == key)
      {
         struct timespec wait;
         int64_t left = retro_sec_left(&deadline);
         if (left <= 0)
            break;
         wait.tv_sec  = (time_t)(left / 1000000000);
         wait.tv_nsec = (long)(left % 1000000000);
         syscall(SYS_futex, (void*)&shared->epoch, FUTEX_WAIT,
               key, &wait, NULL, 0);
      }
   }
#else
   if (     timeout_us > 0
         && retro_atomic_load_acquire_int(&shared->epoch) == key)
   {
      int64_t ms = (timeout_us + 999) / 1000;
      WaitForSingleObject((HANDLE)handle->os,
            ms >= (int64_t)INFINITE ? INFINITE - 1 : (DWORD)ms);
   }
#endif
   notified = retro_atomic_load_acquire_int(&shared->epoch) != key;
   retro_atomic_fetch_sub_int(&shared->waiters, 1);
   return notified;
}

#else

/* No backend: attach has failed, and these are never reached with a
 * handle it filled in.  Defined so that a caller links everywhere. */
void retro_shared_eventcount_notify(retro_shared_eventcount_handle_t *handle)
{
   (void)handle;
}

int retro_shared_eventcount_prepare_wait(
      retro_shared_eventcount_handle_t *handle)
{
   (void)handle;
   return 0;
}

void retro_shared_eventcount_cancel_wait(
      retro_shared_eventcount_handle_t *handle)
{
   (void)handle;
}

void retro_shared_eventcount_commit_wait(
      retro_shared_eventcount_handle_t *handle, int key)
{
   (void)handle;
   (void)key;
}

bool retro_shared_eventcount_commit_wait_timeout(
      retro_shared_eventcount_handle_t *handle, int key, int64_t timeout_us)
{
   (void)handle;
   (void)key;
   (void)timeout_us;
   return false;
}

#endif
