/* The Horizon thread priority calls, over SCHED_FIFO.
 *
 * Horizon numbers priorities 0..63 with lower numbers scheduled first;
 * SCHED_FIFO numbers them the other way.  Both preempt strictly by
 * priority and leave a thread level with the running one waiting until
 * that one blocks, which is the property the lanes depend on.  Only
 * the calling thread's handle is supported, which is all the task
 * queue uses.
 */
#if defined(__linux__)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <pthread.h>
#include <sched.h>
#endif

#include <switch/kernel/svc.h>

/* Horizon 0x2C, where the main thread runs, lands on FIFO 36. */
#define FIFO_OF(prio) (80 - (int)(prio))

Result svcGetThreadPriority(s32 *priority, Handle handle)
{
#if defined(__linux__)
   struct sched_param sp;
   int policy;
   if (handle != CUR_THREAD_HANDLE
         || pthread_getschedparam(pthread_self(), &policy, &sp) != 0
         || policy != SCHED_FIFO)
      return 1;
   *priority = 80 - sp.sched_priority;
   return 0;
#else
   (void)priority; (void)handle;
   return 1;
#endif
}

Result svcSetThreadPriority(Handle handle, u32 priority)
{
#if defined(__linux__)
   struct sched_param sp;
   if (handle != CUR_THREAD_HANDLE || priority > 0x3F)
      return 1;
   sp.sched_priority = FIFO_OF(priority);
   return pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) == 0
      ? 0 : 1;
#else
   (void)handle; (void)priority;
   return 1;
#endif
}
