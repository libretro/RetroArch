/* Copyright  (C) 2010-2016 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (psp_pthread.h).
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

/* FIXME: unfinished on PSP, mutexes and condition variables basically a stub. */
#ifndef _PSP_PTHREAD_WRAP__
#define _PSP_PTHREAD_WRAP__

#ifdef VITA
#include <psp2/kernel/threadmgr.h>
#else
#include <pspkernel.h>
#include <pspthreadman.h>
#include <pspthreadman_kernel.h>
#endif
#include <stdio.h>
#include <retro_inline.h>

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

static INLINE int pthread_create(pthread_t *thread,
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

static INLINE int pthread_mutex_init(pthread_mutex_t *mutex,
      const pthread_mutexattr_t *attr)
{
   sprintf(name_buffer, "0x%08X", (uint32_t) mutex);

#ifdef VITA
   return *mutex = sceKernelCreateMutex(name_buffer, 0, 0, 0);
#else
   return *mutex = sceKernelCreateSema(name_buffer, 0, 1, 1, NULL);
#endif
}

static INLINE int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
#ifdef VITA
   return sceKernelDeleteMutex(*mutex);
#else
   return sceKernelDeleteSema(*mutex);
#endif
}

static INLINE int pthread_mutex_lock(pthread_mutex_t *mutex)
{
#ifdef VITA
   return sceKernelLockMutex(*mutex, 1, 0);
#else
   /* FIXME: stub */
   return 1;
#endif
}

static INLINE int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
#ifdef VITA
   return sceKernelUnlockMutex(*mutex, 1);
#else
   /* FIXME: stub */
   return 1;
#endif
}


static INLINE int pthread_join(pthread_t thread, void **retval)
{
   int exit_status;
   SceUInt timeout = (SceUInt)-1;

   sceKernelWaitThreadEnd(thread, &timeout);
   exit_status = sceKernelGetThreadExitStatus(thread);
   sceKernelDeleteThread(thread);
   return exit_status;
}

static INLINE int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
#ifdef VITA
   return sceKernelTryLockMutex(*mutex, 1 /* not sure about this last param */);
#else
   /* FIXME: stub */
   return 1;
#endif
}

static INLINE int pthread_cond_wait(pthread_cond_t *cond,
      pthread_mutex_t *mutex)
{
   sceKernelDelayThread(10000);
   return 1;
}

static INLINE int pthread_cond_timedwait(pthread_cond_t *cond,
      pthread_mutex_t *mutex, const struct timespec *abstime)
{
   //FIXME: stub
   return 1;
}

static INLINE int pthread_cond_init(pthread_cond_t *cond,
      const pthread_condattr_t *attr)
{
   //FIXME: stub
   return 1;
}

static INLINE int pthread_cond_signal(pthread_cond_t *cond)
{
   //FIXME: stub
   return 1;
}

static INLINE int pthread_cond_broadcast(pthread_cond_t *cond)
{
   //FIXME: stub
   return 1;
}

static INLINE int pthread_cond_destroy(pthread_cond_t *cond)
{
   //FIXME: stub
   return 1;
}


static INLINE int pthread_detach(pthread_t thread)
{
   return 1;
}

static INLINE void pthread_exit(void *retval)
{
   (void)retval;
}

static INLINE pthread_t pthread_self(void)
{
   /* zero 20-mar-2016: untested */
   return sceKernelGetThreadId();
}


#endif //_PSP_PTHREAD_WRAP__
