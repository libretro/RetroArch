#ifndef _GX_SDK_DEFINES_H
#define _GX_SDK_DEFINES_H

#ifdef GEKKO

#ifndef OSInitMutex
#define OSThread lwp_t
#define OSCond lwpq_t
#define OSThreadQueue lwpq_t

#define OSInitMutex(mutex) LWP_MutexInit(mutex, 0)
#define OSLockMutex(mutex) LWP_MutexLock(mutex)
#define OSUnlockMutex(mutex) LWP_MutexUnlock(mutex)
#define OSTryLockMutex(mutex) LWP_MutexTryLock(mutex)

#define OSInitCond(cond) LWP_CondInit(cond)
#define OSSignalCond(cond) LWP_ThreadSignal(cond)
#define OSWaitCond(cond, mutex) LWP_CondWait(cond, mutex)

#define OSInitThreadQueue(queue) LWP_InitQueue(queue)
#define OSSleepThread(queue) LWP_ThreadSleep(queue)
#define OSJoinThread(thread, val) LWP_JoinThread(thread, val)

#define OSCreateThread(thread, func, intarg, ptrarg, stackbase, stacksize, priority, attrs) LWP_CreateThread(thread, func, ptrarg, stackbase, stacksize, priority)
#endif

#endif

#endif
