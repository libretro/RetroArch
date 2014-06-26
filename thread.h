/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel de Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef THREAD_H__
#define THREAD_H__

#include "boolean.h"
#include <stdint.h>

#if defined(__cplusplus) && !defined(MSC_VER)
extern "C" {
#endif

// Implements the bare minimum needed for RetroArch. :)

typedef struct sthread sthread_t;

// Threading
sthread_t *sthread_create(void (*thread_func)(void*), void *userdata);
int sthread_detach(sthread_t *thread);
void sthread_join(sthread_t *thread);

// Mutexes
typedef struct slock slock_t;

slock_t *slock_new(void);
void slock_free(slock_t *lock);

void slock_lock(slock_t *lock);
void slock_unlock(slock_t *lock);

// Condition variables.
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
