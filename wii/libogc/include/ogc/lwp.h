/*-------------------------------------------------------------

lwp.h -- Thread subsystem I

Copyright (C) 2004
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#ifndef __LWP_H__
#define __LWP_H__

/*! \file lwp.h
\brief Thread subsystem I

*/

#include <gctypes.h>

#define LWP_CLOSED					-1
#define LWP_SUCCESSFUL				0
#define LWP_ALREADY_SUSPENDED		1
#define LWP_NOT_SUSPENDED			2

#define LWP_PRIO_IDLE				0
#define LWP_PRIO_HIGHEST		  127

#define LWP_THREAD_NULL				0xffffffff
#define LWP_TQUEUE_NULL				0xffffffff

#ifdef __cplusplus
extern "C" {
#endif

/*! \typedef u32 lwp_t
\brief typedef for the thread context handle
*/
typedef u32 lwp_t;

/*! \typedef u32 lwpq_t
\brief typedef for the thread queue's context handle
*/
typedef u32 lwpq_t;

/*! \fn s32 LWP_CreateThread(lwp_t *thethread,void* (*entry)(void *),void *arg,void *stackbase,u32 stack_size,u8 prio)
\brief Spawn a new thread with the given parameters
\param[out] thethread pointer to a lwp_t handle
\param[in] entry pointer to the thread's entry function.
\param[in] arg pointer to an argument for the thread's entry function.
\param[in] stackbase pointer to the threads stackbase address. If NULL, the stack is allocated by the thread system.
\param[in] stack_size size of the provided stack. If 0, the default STACKSIZE of 8Kb is taken.
\param[in] prio priority on which the newly created thread runs.

\return 0 on success, <0 on error
*/
s32 LWP_CreateThread(lwp_t *thethread,void* (*entry)(void *),void *arg,void *stackbase,u32 stack_size,u8 prio);

/*! \fn s32 LWP_SuspendThread(lwp_t thethread)
\brief Suspend the given thread.
\param[in] thethread handle to the thread context which should be suspended.

\return 0 on success, <0 on error
*/
s32 LWP_SuspendThread(lwp_t thethread);

/*! \fn s32 LWP_ResumeThread(lwp_t thethread)
\brief Resume the given thread.
\param[in] thethread handle to the thread context which should be resumed.

\return 0 on success, <0 on error
*/
s32 LWP_ResumeThread(lwp_t thethread);

/*! \fn BOOL LWP_ThreadIsSuspended(lwp_t thethread)
\brief Test whether the given thread is suspended or not
\param[in] thethread handle to the thread context which should be tested.

\return TRUE or FALSE
*/
BOOL LWP_ThreadIsSuspended(lwp_t thethread);

/*! \fn lwp_t LWP_GetSelf()
\brief Return the handle to the current thread.

\return thread context handle
*/
lwp_t LWP_GetSelf();

/*! \fn void LWP_SetThreadPriority(lwp_t thethread,u32 prio)
\brief Set the priority of the given thread.
\param[in] thethread handle to the thread context whos priority should be changed. If NULL, the current thread will be taken.
\param[in] prio new priority to set

\return none
*/
void LWP_SetThreadPriority(lwp_t thethread,u32 prio);

/*! \fn void LWP_YieldThread()
\brief Yield the current thread to another one with higher priority or if not running at the same priority which state is runnable.

\return none
*/
void LWP_YieldThread();

/*! \fn void LWP_Reschedule(u32 prio)
\brief Reschedule all threads running at the given priority
\param[in] prio priority level to reschedule

\return none
*/
void LWP_Reschedule(u32 prio);

/*! \fn s32 LWP_JoinThread(lwp_t thethread,void **value_ptr)
\brief Join the given thread.
\param[in] thethread handle to the thread's context which should be joined to wait on termination.
\param[in] value_ptr pointer-pointer to a variable to receive the return code of the terminated thread.

\return 0 on success, <0 on error
*/
s32 LWP_JoinThread(lwp_t thethread,void **value_ptr);

/*! \fn void LWP_InitQueue(lwpq_t *thequeue)
\brief Initialize the thread synchronization queue
\param[in] thequeue pointer to a lwpq_t handle.

\return 0 on success, <0 on error
*/
s32 LWP_InitQueue(lwpq_t *thequeue);

/*! \fn void LWP_CloseQueue(lwpq_t thequeue)
\brief Close the thread synchronization queue and releas the handle
\param[in] thequeue handle to the thread's synchronization queue

\return none
*/
void LWP_CloseQueue(lwpq_t thequeue);

/*! \fn s32 LWP_ThreadSleep(lwpq_t thequeue)
\brief Pushes the current thread onto the given thread synchronization queue and sets the thread state to blocked.
\param[in] thequeue handle to the thread's synchronization queue to push the thread on

\return none
*/
s32 LWP_ThreadSleep(lwpq_t thequeue);

/*! \fn void LWP_ThreadSignal(lwpq_t thequeue)
\brief Signals one thread to be revmoved from the thread synchronization queue and sets it back to running state.
\param[in] thequeue handle to the thread's synchronization queue to pop the blocked thread off

\return none
*/
void LWP_ThreadSignal(lwpq_t thequeue);

/*! \fn void LWP_ThreadBroadcast(lwpq_t thequeue)
\brief Removes all blocked threads from the thread synchronization queue and sets them back to running state.
\param[in] thequeue handle to the thread's synchronization queue to pop the blocked threads off

\return none
*/
void LWP_ThreadBroadcast(lwpq_t thequeue);

#ifdef __cplusplus
	}
#endif

#endif
