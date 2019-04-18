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
VideoCore OS Abstraction Layer - low level thread support
=============================================================================*/

#ifndef VCOS_LOWLEVEL_THREAD_H
#define VCOS_LOWLEVEL_THREAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/vcos/vcos_types.h"
#ifndef VCOS_PLATFORM_H
#include "vcos.h"
#endif

/**
 * \file
 *
 * This defines a low level thread API that is supported by *some* operating systems
 * and can be used to construct the regular "joinable thread" API on those operating
 * systems.
 *
 * Most clients will not need to use this code.
 *
 * \sa vcos_joinable_thread.h
 */

/**
  * \brief Create a thread.
  *
  * This creates a thread which can be stopped either by returning from the
  * entry point function or by calling vcos_llthread_exit from within the entry
  * point function. The thread must be cleaned up by calling
  * vcos_llthread_delete. vcos_llthread_delete may or may not terminate the
  * thread.
  *
  * The preemptible parameter familiar from Nucleus is removed, as it is unused in
  *  VideoCore code. Affinity is added, since we do use this.
  *
  * @param thread       Filled in with thread instance
  * @param name         An optional name for the thread. "" may be used (but
  *                     a name will aid in debugging).
  * @param entry        Entry point
  * @param arg          A single argument passed to the entry point function
  * @param stack        Pointer to stack address
  * @param stacksz      Size of stack in bytes
  * @param priority     Priority of task, between VCOS_PRI_LOW and VCOS_PRI_HIGH
  * @param affinity     CPU affinity
  *
  * @sa vcos_llthread_terminate vcos_llthread_delete
  */
VCOSPRE_ VCOS_STATUS_T VCOSPOST_ vcos_llthread_create(VCOS_LLTHREAD_T *thread,
                                                      const char *name,
                                                      VCOS_LLTHREAD_ENTRY_FN_T entry,
                                                      void *arg,
                                                      void *stack,
                                                      VCOS_UNSIGNED stacksz,
                                                      VCOS_UNSIGNED priority,
                                                      VCOS_UNSIGNED affinity,
                                                      VCOS_UNSIGNED timeslice,
                                                      VCOS_UNSIGNED autostart);

/**
  * \brief Exits the current thread.
  */
VCOSPRE_ void VCOSPOST_ vcos_llthread_exit(void);

/**
  * \brief Delete a thread. This must be called to cleanup after
  * vcos_llthread_create. This may or may not terminate the thread.
  * It does not clean up any resources that may have been
  * allocated by the thread.
  */
VCOSPRE_ void VCOSPOST_ vcos_llthread_delete(VCOS_LLTHREAD_T *thread);

/**
  * \brief Return current lowlevel thread pointer.
  */
VCOS_INLINE_DECL
VCOS_LLTHREAD_T *vcos_llthread_current(void);

/**
  * Resume a thread.
  */
VCOS_INLINE_DECL
void vcos_llthread_resume(VCOS_LLTHREAD_T *thread);

VCOSPRE_ int VCOSPOST_ vcos_llthread_running(VCOS_LLTHREAD_T *thread);

/**
  * \brief Create a VCOS_LLTHREAD_T for the current thread. This is so we can
  * have VCOS_LLTHREAD_Ts even for threads not originally created by VCOS (eg
  * the thread that calls vcos_init).
  */
extern VCOS_STATUS_T _vcos_llthread_create_attach(VCOS_LLTHREAD_T *thread);

#ifdef __cplusplus
}
#endif
#endif
