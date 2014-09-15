/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 *  Copyright (C) 2014      - Ali Bouhlel ( aliaspider@gmail.com )
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

// FIXME: unfinished, mutexes and condvars basically a stub.
#ifndef _PSP_PTHREAD_WRAP__
#define _PSP_PTHREAD_WRAP__

#include <pspkernel.h>
#include <pspthreadman.h>
#include <pspthreadman_kernel.h>
#include <stdio.h>

#define STACKSIZE (64 * 1024)

typedef SceUID pthread_t;
typedef SceUID pthread_mutex_t;
typedef void* pthread_mutexattr_t;
typedef int pthread_attr_t;
typedef SceUID pthread_cond_t;
typedef SceUID pthread_condattr_t;

/* Use pointer values to create unique names for threads/mutexes */
char name_buffer[256];

typedef void* (*sthreadEntry)(void *argp);

typedef struct
{
   void* arg;
   sthreadEntry start_routine;
} sthread_args_struct;


static int psp_thread_wrap(SceSize args, void *argp)
{
   sthread_args_struct* sthread_args = (sthread_args_struct*)argp;

   return (int)sthread_args->start_routine(sthread_args->arg);
}

static inline int pthread_create(pthread_t *thread,
      const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg)
{
   sprintf(name_buffer, "0x%08X", (uint32_t) thread);

   *thread = sceKernelCreateThread(name_buffer,
         psp_thread_wrap, 0x20, STACKSIZE, 0, NULL);

   sthread_args_struct sthread_args;
   sthread_args.arg = arg;
   sthread_args.start_routine = start_routine;

   return sceKernelStartThread(*thread, sizeof(sthread_args), &sthread_args);
}

static inline int pthread_mutex_init(pthread_mutex_t *mutex,
      const pthread_mutexattr_t *attr)
{
   sprintf(name_buffer, "0x%08X", (uint32_t) mutex);

   return *mutex = sceKernelCreateSema(name_buffer, 0, 1, 1, NULL);
}

static inline int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
   return sceKernelDeleteSema(*mutex);
}

static inline int pthread_mutex_lock(pthread_mutex_t *mutex)
{
   //FIXME: stub
   return 1;
}

static inline int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
   //FIXME: stub
   return 1;
}


static inline int pthread_join(pthread_t thread, void **retval)
{
   SceUInt timeout = (SceUInt)-1;
   sceKernelWaitThreadEnd(thread, &timeout);
   int exit_status = sceKernelGetThreadExitStatus(thread);
   sceKernelDeleteThread(thread);
   return exit_status;
}

static inline int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
   //FIXME: stub
   return 1;
}

static inline int pthread_cond_wait(pthread_cond_t *cond,
      pthread_mutex_t *mutex)
{
   sceKernelDelayThread(10000);
   return 1;
}

static inline int pthread_cond_timedwait(pthread_cond_t *cond,
      pthread_mutex_t *mutex, const struct timespec *abstime)
{
   //FIXME: stub
   return 1;
}

static inline int pthread_cond_init(pthread_cond_t *cond,
      const pthread_condattr_t *attr)
{
   //FIXME: stub
   return 1;
}

static inline int pthread_cond_signal(pthread_cond_t *cond)
{
   //FIXME: stub
   return 1;
}

static inline int pthread_cond_broadcast(pthread_cond_t *cond)
{
   //FIXME: stub
   return 1;
}

static inline int pthread_cond_destroy(pthread_cond_t *cond)
{
   //FIXME: stub
   return 1;
}


static inline int pthread_detach(pthread_t thread)
{
   return 1;
}

static inline void pthread_exit(void *retval)
{
   (void)retval;
}

#endif //_PSP_PTHREAD_WRAP__
