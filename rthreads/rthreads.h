/* Copyright  (C) 2010-2014 The RetroArch team
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

#ifndef THREAD_H__
#define THREAD_H__

#include "../boolean.h"
#include <stdint.h>

#if defined(__cplusplus) && !defined(MSC_VER)
extern "C" {
#endif

/* Implements the bare minimum needed. */

typedef struct sthread sthread_t;

/* Threading */
sthread_t *sthread_create(void (*thread_func)(void*), void *userdata);

int sthread_detach(sthread_t *thread);

void sthread_join(sthread_t *thread);

/* Mutexes */
typedef struct slock slock_t;

slock_t *slock_new(void);

void slock_free(slock_t *lock);

void slock_lock(slock_t *lock);

void slock_unlock(slock_t *lock);

/* Condition variables. */
typedef struct scond scond_t;

scond_t *scond_new(void);

void scond_free(scond_t *cond);

void scond_wait(scond_t *cond, slock_t *lock);

bool scond_wait_timeout(scond_t *cond, slock_t *lock, int64_t timeout_us);

int scond_broadcast(scond_t *cond);

void scond_signal(scond_t *cond);

#ifndef RARCH_INTERNAL
#if defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)
#include <sys/timer.h>
#elif defined(XENON)
#include <time/time.h>
#elif defined(GEKKO) || defined(__PSL1GHT__) || defined(__QNX__)
#include <unistd.h>
#elif defined(PSP)
#include <pspthreadman.h>
#include <psputils.h>
#elif defined(_WIN32) && !defined(_XBOX)
#include <windows.h>
#elif defined(_XBOX)
#include <xtl.h>
#else
#include <time.h>
#endif

static inline void retro_sleep(unsigned msec)
{
#if defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)
   sys_timer_usleep(1000 * msec);
#elif defined(PSP)
   sceKernelDelayThread(1000 * msec);
#elif defined(_WIN32)
   Sleep(msec);
#elif defined(XENON)
   udelay(1000 * msec);
#elif defined(GEKKO) || defined(__PSL1GHT__) || defined(__QNX__)
   usleep(1000 * msec);
#else
   struct timespec tv = {0};
   tv.tv_sec = msec / 1000;
   tv.tv_nsec = (msec % 1000) * 1000000;
   nanosleep(&tv, NULL);
#endif
}
#endif

#if defined(__cplusplus) && !defined(MSC_VER)
}
#endif

#endif
