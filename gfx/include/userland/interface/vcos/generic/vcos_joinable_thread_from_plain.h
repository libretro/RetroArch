/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*=============================================================================
VideoCore OS Abstraction Layer - implementation: joinable thread from plain
=============================================================================*/

/** \file
  *
  * Header file for platforms creating the joinable thread from a lowlevel
  * thread.
  *
  * In addition to the actual thread, the following are also created:
  *
  * - a semaphore to wait on when joining the thread
  * - a semaphore to support counted suspend/resume (used by event group)
  * - a per-thread timer (used by event group, but could be removed)
  */

#ifndef VCOS_JOINABLE_THREAD_FROM_PLAIN_H
#define VCOS_JOINABLE_THREAD_FROM_PLAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef VCOS_SEMAPHORE_H
#include "interface/vcos/vcos_semaphore.h"
#endif
#ifndef VCOS_LOWLEVEL_THREAD_H
#include "interface/vcos/vcos_lowlevel_thread.h"
#endif
#ifndef VCOS_TIMER_H
#include "interface/vcos/vcos_timer.h"
#endif

#ifdef VCOS_WANT_TLS_EMULATION
#include "interface/vcos/generic/vcos_generic_tls.h"
#endif

#define VCOS_THREAD_MAGIC 0x56436a74

#define VCOS_THREAD_VALID(t) (t->magic == VCOS_THREAD_MAGIC)
#define VCOS_HAVE_THREAD_AT_EXIT        1

/** Thread attribute structure. Clients should not manipulate this directly, but
  * should instead use the provided functions.
  */
typedef struct VCOS_THREAD_ATTR_T
{
   void *ta_stackaddr;
   VCOS_UNSIGNED ta_stacksz;
   VCOS_UNSIGNED ta_priority;
   VCOS_UNSIGNED ta_affinity;
   VCOS_UNSIGNED ta_timeslice;
   VCOS_UNSIGNED legacy;
   VCOS_UNSIGNED ta_autostart;
} VCOS_THREAD_ATTR_T;

/** Each thread gets a timer, which is for internal VCOS use.
  */
typedef struct _VCOS_THREAD_TIMER_T
{
   VCOS_TIMER_T timer;
   void (*pfn)(void *);
   void *cxt;
} _VCOS_THREAD_TIMER_T;

typedef void (*VCOS_THREAD_EXIT_HANDLER_T)(void *);
/** Called at thread exit.
  */
typedef struct VCOS_THREAD_EXIT_T
{
   VCOS_THREAD_EXIT_HANDLER_T pfn;
   void *cxt;
} VCOS_THREAD_EXIT_T;
#define VCOS_MAX_EXIT_HANDLERS  8

/* The name field isn't used for anything, so we can just copy the
 * the pointer. Nucleus makes its own copy.
 */
typedef const char *          VCOS_LLTHREAD_T_NAME;
#define _VCOS_LLTHREAD_NAME(dst,src) (dst)=(src)

/*
 * Simulated TLS support
 */


/** Thread structure.
  *
  * \warning Do not access the members of this structure directly!
  */
typedef struct VCOS_THREAD_T
{
   VCOS_LLTHREAD_T  thread;      /**< The underlying thread */
   char name[16];                /**< The name */
   unsigned int     magic;       /**< For debug */
   void            *exit_data;   /**< Exit data passed out in vcos_joinable_thread_exit() */
   void            *stack;       /**< Stack, if not supplied by caller */
   VCOS_SEMAPHORE_T wait;        /**< Semaphore to wait on at join */
   VCOS_SEMAPHORE_T suspend;     /**< Semaphore to wait on for counted suspend */
   int16_t          joined;      /**< Joined yet? For debug. */
   VCOS_UNSIGNED    legacy;      /**< Use (argc,argv) for entry point arguments */
   void *(*entry)(void*);        /**< Entry point */
   void             *arg;        /**< Argument passed to entry point */
   void *(*term)(void*);         /**< Termination function, used by reaper */
   void             *term_arg;   /**< Argument passed to termination function */
   _VCOS_THREAD_TIMER_T _timer;  /**< Internal timer, mainly for event groups */
#ifdef VCOS_WANT_TLS_EMULATION
   VCOS_TLS_THREAD_T   _tls;     /**< TLS data when native TLS not available, or NULL */
#endif
   /** Array of functions to call at thread exit */
   VCOS_THREAD_EXIT_T at_exit[VCOS_MAX_EXIT_HANDLERS];

   struct VCOS_THREAD_T *next;   /**< For linked lists of threads */
} VCOS_THREAD_T;

#if defined(VCOS_INLINE_BODIES)

VCOS_INLINE_IMPL
void vcos_thread_attr_setstack(VCOS_THREAD_ATTR_T *attrs, void *addr, VCOS_UNSIGNED stacksz) {
   attrs->ta_stackaddr = addr;
   attrs->ta_stacksz = stacksz;
}

VCOS_INLINE_IMPL
void vcos_thread_attr_setstacksize(VCOS_THREAD_ATTR_T *attrs, VCOS_UNSIGNED stacksz) {
   attrs->ta_stacksz = stacksz;
}

VCOS_INLINE_IMPL
void vcos_thread_attr_setpriority(VCOS_THREAD_ATTR_T *attrs, VCOS_UNSIGNED pri) {
   attrs->ta_priority = pri;
}

VCOS_INLINE_IMPL
void vcos_thread_attr_setaffinity(VCOS_THREAD_ATTR_T *attrs, VCOS_UNSIGNED affinity) {
   attrs->ta_affinity = affinity;
}

VCOS_INLINE_IMPL
void vcos_thread_attr_settimeslice(VCOS_THREAD_ATTR_T *attrs, VCOS_UNSIGNED ts) {
   attrs->ta_timeslice = ts;
}

VCOS_INLINE_IMPL
void _vcos_thread_attr_setlegacyapi(VCOS_THREAD_ATTR_T *attrs, VCOS_UNSIGNED legacy) {
   attrs->legacy = legacy;
}

VCOS_INLINE_IMPL
void vcos_thread_attr_setautostart(VCOS_THREAD_ATTR_T *attrs, VCOS_UNSIGNED autostart) {
   attrs->ta_autostart = autostart;
}

VCOS_INLINE_IMPL
VCOS_THREAD_T *vcos_thread_current(void) {
   VCOS_THREAD_T *ret =  (VCOS_THREAD_T*)vcos_llthread_current();
   /*If we're called from a non-vcos thread, this assert will fail.
    *XXX FIXME why is this commented out?
    *vcos_assert(ret->magic == VCOS_THREAD_MAGIC);
    */
   return ret;
}

VCOS_INLINE_IMPL
int vcos_thread_running(VCOS_THREAD_T *thread) {
   return vcos_llthread_running(&thread->thread);
}

VCOS_INLINE_IMPL
void vcos_thread_resume(VCOS_THREAD_T *thread) {
   vcos_llthread_resume(&thread->thread);
}

#endif /* VCOS_INLINE_BODIES */

/**
  * \brief Create a VCOS_THREAD_T for the current thread. This is so we can have
  * VCOS_THREAD_Ts even for threads not originally created by VCOS (eg the
  * thread that calls vcos_init)
  */
extern VCOS_STATUS_T _vcos_thread_create_attach(VCOS_THREAD_T *thread,
                                                const char *name);

/**
  * \brief Deletes the VCOS_THREAD_T, but does not wait for the underlying
  * thread to exit. This will cleanup everything created by
  * _vcos_thread_create_attach
  */
extern void _vcos_thread_delete(VCOS_THREAD_T *thread);

/** Register a function to be called when the current thread exits.
  */
extern VCOS_STATUS_T vcos_thread_at_exit(void (*pfn)(void*), void *cxt);

/** Deregister a previously registered at-exit function.
  */
extern void vcos_thread_deregister_at_exit(void (*pfn)(void*), void *cxt);

#ifdef __cplusplus
}
#endif
#endif /* VCOS_JOINABLE_THREAD_FROM_PLAIN_H */
