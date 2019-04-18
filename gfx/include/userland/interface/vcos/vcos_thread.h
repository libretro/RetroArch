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
VideoCore OS Abstraction Layer - public header file
=============================================================================*/

#ifndef VCOS_THREAD_H
#define VCOS_THREAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/vcos/vcos_types.h"
#include "vcos.h"

/**
 * \file vcos_thread.h
 *
 * \section thread Threads
 *
 * Under Nucleus, a thread is created by NU_Create_Task, passing in the stack
 * and various other parameters. To stop the thread, NU_Terminate_Thread() and
 * NU_Delete_Thread() are called.
 *
 * Unfortunately it's not possible to emulate this API under some fairly common
 * operating systems. Under Windows you can't pass in the stack, and you can't
 * safely terminate a thread.
 *
 * Therefore, an API which is similar to the pthreads API is used instead. This
 * API can (mostly) be emulated under all interesting operating systems.
 *
 * Obviously this makes the code somewhat more complicated on VideoCore than it
 * would otherwise be - we end up with an extra mutex per thread, and some code
 * that waits for it. The benefit is that we have a single way of creating
 * threads that works consistently on all platforms (apart from stack supplying).
 *
 * \subsection stack Stack
 *
 * It's still not possible to pass in the stack address, but this can be made
 * much more obvious in the API: the relevant function is missing and the
 * CPP symbol VCOS_CAN_SET_STACK_ADDR is zero rather than one.
 *
 * \subsection thr_create Creating a thread
 *
 * The simplest way to create a thread is with vcos_thread_create() passing in a
 * NULL thread parameter argument. To wait for the thread to exit, call
 * vcos_thread_join().
 *
 * \subsection back Backward compatibility
 *
 * To ease migration, a "classic" thread creation API is provided for code
 * that used to make use of Nucleus, vcos_thread_create_classic(). The
 * arguments are not exactly the same, as the PREEMPT parameter is dropped.
 *
 */

#define VCOS_AFFINITY_CPU0    _VCOS_AFFINITY_CPU0
#define VCOS_AFFINITY_CPU1    _VCOS_AFFINITY_CPU1
#define VCOS_AFFINITY_MASK    _VCOS_AFFINITY_MASK
#define VCOS_AFFINITY_DEFAULT _VCOS_AFFINITY_DEFAULT
#define VCOS_AFFINITY_THISCPU _VCOS_AFFINITY_THISCPU

/** Report whether or not we have an RTOS at all, and hence the ability to
  * create threads.
  */
VCOSPRE_ int VCOSPOST_ vcos_have_rtos(void);

/** Create a thread. It must be cleaned up by calling vcos_thread_join().
  *
  * @param thread   Filled in on return with thread
  * @param name     A name for the thread. May be the empty string.
  * @param attrs    Attributes; default attributes will be used if this is NULL.
  * @param entry    Entry point.
  * @param arg      Argument passed to the entry point.
  */
VCOSPRE_ VCOS_STATUS_T VCOSPOST_ vcos_thread_create(VCOS_THREAD_T *thread,
                                                    const char *name,
                                                    VCOS_THREAD_ATTR_T *attrs,
                                                    VCOS_THREAD_ENTRY_FN_T entry,
                                                    void *arg);

/** Exit the thread from within the thread function itself.
  * Resources must still be cleaned up via a call to thread_join().
  *
  * The thread can also be terminated by simply exiting the thread function.
  *
  * @param data Data passed to thread_join. May be NULL.
  */
VCOSPRE_ void VCOSPOST_ vcos_thread_exit(void *data);

/** Wait for a thread to terminate and then clean up its resources.
  *
  * @param thread Thread to wait for
  * @param pData  Updated to point at data provided in vcos_thread_exit or exit
  * code of thread function.
  */
VCOSPRE_ void VCOSPOST_ vcos_thread_join(VCOS_THREAD_T *thread,
                             void **pData);

/**
  * \brief Create a thread using an API similar to the one "traditionally"
  * used under Nucleus.
  *
  * This creates a thread which must be cleaned up by calling vcos_thread_join().
  * The thread cannot be simply terminated (as in Nucleus and ThreadX) as thread
  * termination is not universally supported.
  *
  * @param thread       Filled in with thread instance
  * @param name         An optional name for the thread. NULL or "" may be used (but
  *                     a name will aid in debugging).
  * @param entry        Entry point
  * @param arg          A single argument passed to the entry point function
  * @param stack        Pointer to stack address
  * @param stacksz      Size of stack in bytes
  * @param priaff       Priority of task, between VCOS_PRI_LOW and VCOS_PRI_HIGH, ORed with the CPU affinity
  * @param autostart    If non-zero the thread will start immediately.
  * @param timeslice    Timeslice (system ticks) for this thread.
  *
  * @sa vcos_thread_terminate vcos_thread_delete
  */
VCOSPRE_ VCOS_STATUS_T VCOSPOST_ vcos_thread_create_classic(VCOS_THREAD_T *thread,
                                                            const char *name,
                                                            void *(*entry)(void *arg),
                                                            void *arg,
                                                            void *stack,
                                                            VCOS_UNSIGNED stacksz,
                                                            VCOS_UNSIGNED priaff,
                                                            VCOS_UNSIGNED timeslice,
                                                            VCOS_UNSIGNED autostart);

/**
  * \brief Set a thread's priority
  *
  * Set the priority for a thread.
  *
  * @param thread  The thread
  * @param pri     Thread priority in VCOS_PRI_MASK bits; affinity in VCOS_AFFINITY_MASK bits.
  */
VCOS_INLINE_DECL
void vcos_thread_set_priority(VCOS_THREAD_T *thread, VCOS_UNSIGNED pri);

/**
  * \brief Return the currently executing thread.
  *
  */
VCOS_INLINE_DECL
VCOS_THREAD_T *vcos_thread_current(void);

/**
  * \brief Return the thread's priority.
  */
VCOS_INLINE_DECL
VCOS_UNSIGNED vcos_thread_get_priority(VCOS_THREAD_T *thread);

/**
  * \brief Return the thread's cpu affinity.
  */
VCOS_INLINE_DECL
VCOS_UNSIGNED vcos_thread_get_affinity(VCOS_THREAD_T *thread);

/**
  * \brief Set the thread's cpu affinity.
  */

VCOS_INLINE_DECL
void vcos_thread_set_affinity(VCOS_THREAD_T *thread, VCOS_UNSIGNED affinity);

/**
  * \brief Query whether we are in an interrupt.
  *
  * @return 1 if in interrupt context.
  */
VCOS_INLINE_DECL
int vcos_in_interrupt(void);

/**
  * \brief Sleep a while.
  *
  * @param ms Number of milliseconds to sleep for
  *
  * This may actually sleep a whole number of ticks.
  */
VCOS_INLINE_DECL
void vcos_sleep(uint32_t ms);

/**
  * \brief Return the value of the hardware microsecond counter.
  *
  */
VCOS_INLINE_DECL
uint32_t vcos_getmicrosecs(void);

VCOS_INLINE_DECL
uint64_t vcos_getmicrosecs64(void);

#define vcos_get_ms() (vcos_getmicrosecs()/1000)

/**
  * \brief Return a unique identifier for the current process
  *
  */
VCOS_INLINE_DECL
VCOS_UNSIGNED vcos_process_id_current(void);

/** Relinquish this time slice. */
VCOS_INLINE_DECL
void vcos_thread_relinquish(void);

/** Return the name of the given thread.
  */
VCOSPRE_ const char * VCOSPOST_ vcos_thread_get_name(const VCOS_THREAD_T *thread);

/** Change preemption. This is almost certainly not what you want, as it won't
  * work reliably in a multicore system: although you can affect the preemption
  * on *this* core, you won't affect what's happening on the other core(s).
  *
  * It's mainly here to ease migration. If you're using it in new code, you
  * probably need to think again.
  *
  * @param pe New preemption, VCOS_PREEMPT or VCOS_NO_PREEMPT
  * @return Old value of preemption.
  */
VCOS_INLINE_DECL
VCOS_UNSIGNED vcos_change_preemption(VCOS_UNSIGNED pe);

/** Is a thread still running, or has it exited?
  *
  * Note: this exists for some fairly scary code in the video codec tests. Don't
  * try to use it for anything else, as it may well not do what you expect.
  *
  * @param thread   thread to query
  * @return non-zero if thread is running, or zero if it has exited.
  */
VCOS_INLINE_DECL
int vcos_thread_running(VCOS_THREAD_T *thread);

/** Resume a thread.
  *
  * @param thread thread to resume
  */
VCOS_INLINE_DECL
void vcos_thread_resume(VCOS_THREAD_T *thread);

/*
 * Internal APIs - may not always be present and should not be used in
 * client code.
 */

extern void _vcos_task_timer_set(void (*pfn)(void*), void *, VCOS_UNSIGNED ms);
extern void _vcos_task_timer_cancel(void);

#ifdef __cplusplus
}
#endif
#endif
