/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rthreads.c).
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

#ifdef __unix__
#ifndef __sun__
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309
#endif
#endif
#endif

#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <rthreads/rthreads.h>
#include <retro_atomic.h>

/* with RETRO_WIN32_USE_PTHREADS, pthreads can be used even on win32.
 * Maybe only supported in MSVC>=2005 */

#if defined(_WIN32) && !defined(RETRO_WIN32_USE_PTHREADS)
#define USE_WIN32_THREADS
#ifdef _XBOX
#include <xtl.h>
#else
#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500 /*_WIN32_WINNT_WIN2K */
#endif
#include <windows.h>
#endif
#elif defined(GEKKO)
#define USE_GX_THREADS
#include <gccore.h>
#include <ogc/lwp.h>
#include <ogc/mutex.h>
#include <ogc/cond.h>
#define STACKSIZE (8 * 1024)
#elif defined(_3DS)
#define USE_CTR_THREADS
#include <3ds/thread.h>
#include <3ds/synchronization.h>
#include <3ds/svc.h>
#include <3ds/services/apt.h>
#include <retro_inline.h>
#define STACKSIZE (32 * 1024)
#elif defined(PSP)
#define USE_PSP_THREADS
#include <pspkernel.h>
#include <pspthreadman.h>
#define STACKSIZE (32 * 1024)
#elif defined(WIIU)
#define USE_WIIU_THREADS
#include <malloc.h>
#include <string.h>
#if defined(__has_include)
#if __has_include(<wiiu/os/thread.h>)
#define RTHREADS_WIIU_VENDORED
#endif
#endif
#ifdef RTHREADS_WIIU_VENDORED
#include <wiiu/os/thread.h>
#include <wiiu/os/fastmutex.h>
#include <wiiu/os/event.h>
#include <wiiu/os/time.h>
#include <wiiu/os/systeminfo.h>
#else
#include <coreinit/thread.h>
#include <coreinit/fastmutex.h>
#include <coreinit/event.h>
#include <coreinit/time.h>
#include <coreinit/systeminfo.h>
#endif
#ifndef OSMicroseconds
#define OSMicroseconds(us) OSMicrosecondsToTicks(us)
#endif
#define STACKSIZE (128 * 1024)
#elif defined(PS2)
#define USE_PS2_THREADS
#include <kernel.h>
#include <timer.h>
#include <timer_alarm.h>
#include <malloc.h>
#include <string.h>
#define STACKSIZE (32 * 1024)
#elif defined(__SWITCH__)
#define USE_SWITCH_THREADS
#include <switch.h>
#define STACKSIZE (128 * 1024)
#elif defined(VITA)
#define USE_VITA_THREADS
#include <psp2/kernel/threadmgr.h>
#ifdef DEBUG
#include <retro_assert.h>
#endif
#define STACKSIZE (64 * 1024)
#else
#include <pthread.h>
#include <time.h>
#endif

#if defined(USE_CTR_THREADS) && !defined(USE_CTRULIB_2)
/* Backported CondVar API from libctru 2.0, and under its license:
   https://github.com/devkitPro/libctru
   Slightly modified for compatibility with older libctru. */

typedef s32 CondVar;

static INLINE Result syncArbitrateAddress(s32* addr, ArbitrationType type, s32 value)
{
   return svcArbitrateAddress(__sync_get_arbiter(), (u32)addr, type, value, 0);
}

static INLINE Result syncArbitrateAddressWithTimeout(s32* addr, ArbitrationType type, s32 value, s64 timeout_ns)
{
   return svcArbitrateAddress(__sync_get_arbiter(), (u32)addr, type, value, timeout_ns);
}

static INLINE void __dmb(void)
{
	__asm__ __volatile__("mcr p15, 0, %[val], c7, c10, 5" :: [val] "r" (0) : "memory");
}

static INLINE void CondVar_BeginWait(CondVar* cv, LightLock* lock)
{
	s32 val;
	do
		val = __ldrex(cv) - 1;
	while (__strex(cv, val));
	LightLock_Unlock(lock);
}

static INLINE bool CondVar_EndWait(CondVar* cv, s32 num_threads)
{
	bool hasWaiters;
	s32 val;

	do {
		val = __ldrex(cv);
		hasWaiters = val < 0;
		if (hasWaiters)
		{
			if (num_threads < 0)
				val = 0;
			else if (val <= -num_threads)
				val += num_threads;
			else
				val = 0;
		}
	} while (__strex(cv, val));

	return hasWaiters;
}

static INLINE void CondVar_Init(CondVar* cv)
{
	*cv = 0;
}

static INLINE void CondVar_Wait(CondVar* cv, LightLock* lock)
{
	CondVar_BeginWait(cv, lock);
	syncArbitrateAddress(cv, ARBITRATION_WAIT_IF_LESS_THAN, 0);
	LightLock_Lock(lock);
}

static INLINE int CondVar_WaitTimeout(CondVar* cv, LightLock* lock, s64 timeout_ns)
{
	CondVar_BeginWait(cv, lock);

	bool timedOut = false;
	Result rc = syncArbitrateAddressWithTimeout(cv, ARBITRATION_WAIT_IF_LESS_THAN_TIMEOUT, 0, timeout_ns);
	if (R_DESCRIPTION(rc) == RD_TIMEOUT)
	{
		timedOut = CondVar_EndWait(cv, 1);
		__dmb();
	}

	LightLock_Lock(lock);
	return timedOut;
}

static INLINE void CondVar_WakeUp(CondVar* cv, s32 num_threads)
{
	__dmb();
	if (CondVar_EndWait(cv, num_threads))
		syncArbitrateAddress(cv, ARBITRATION_SIGNAL, num_threads);
	else
		__dmb();
}

static INLINE void CondVar_Signal(CondVar* cv)
{
	CondVar_WakeUp(cv, 1);
}

static INLINE void CondVar_Broadcast(CondVar* cv)
{
	CondVar_WakeUp(cv, ARBITRATION_SIGNAL_ALL);
}
/* End libctru 2.0 backport */
#endif


#if defined(BSD) || defined(ORBIS)
#include <sys/time.h>
#endif

/* sthread_setname */
#if defined(__APPLE__)
#include <dlfcn.h>
#endif
#if defined(__linux__) && !defined(USE_WIN32_THREADS)
#include <sys/prctl.h>
#endif

#if defined(__ANDROID__)
#include <sys/resource.h>
#include <unistd.h>
#endif

/* Android: scond goes straight to the futex. Bionic's condvar issues
 * the wake syscall on every signal whether or not anyone is waiting,
 * so a waiter count in front of it makes the common empty signal
 * free. The constants are spelled out here because they are ABI
 * facts, not header facts: the same values from 2.6-era kernels on. */
#if defined(__ANDROID__) && !defined(USE_WIN32_THREADS)
#include <sys/syscall.h>
#include <errno.h>
#define RTHREADS_FUTEX_SCOND 1
#define RTHREADS_FUTEX_WAIT_BITSET_PRIVATE  (9 | 128)
#define RTHREADS_FUTEX_WAKE_PRIVATE         (1 | 128)
#define RTHREADS_FUTEX_MATCH_ANY            0xffffffffu
#endif

/* sthread_prefer_fast_cores: Linux and Android, through the raw
 * affinity syscalls so that no particular libc vintage is assumed. */
#if defined(__linux__) && !defined(USE_WIN32_THREADS)
#include <sys/syscall.h>
#include <stdio.h>
#include <unistd.h>
/* The prototype every Linux libc uses; spelled out so a strict C89
 * build, where glibc hides it, sees the same one. */
extern long syscall(long number, ...);
#define RTHREADS_HAVE_AFFINITY 1
#ifndef RTHREADS_CPU_SYSFS
#define RTHREADS_CPU_SYSFS "/sys/devices/system/cpu"
#endif
#endif

#if (defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) \
      || defined(__NetBSD__) || defined(__OpenBSD__)) \
      && !defined(USE_WIN32_THREADS) && !defined(__ANDROID__)
#include <sched.h>
#define RTHREADS_HAVE_SCHEDPARAM 1
#endif

#if defined(__MACH__) && defined(__APPLE__)
#include <mach/clock.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#include <TargetConditionals.h>
#include <AvailabilityMacros.h> /* MAC_OS_X_VERSION_MIN_REQUIRED (since 10.2) */
/* The pthread QoS override API (pthread_override_qos_class_start_np, used by
 * sthread_priority_override_*) exists only on macOS 10.10+ / iOS 8.0+, and
 * RetroArch still ships deployment targets below that (OS X 10.5, iOS 6)
 * where the symbol is absent in both SDK and runtime. Gate on the
 * deployment-target version. TARGET_OS_* keeps the macOS check from firing
 * on iOS; numeric literals are used because the MAC_OS_X_VERSION_10_10 /
 * __IPHONE_8_0 constants are undefined on old SDKs (and would expand to 0). */
#if (TARGET_OS_OSX && defined(MAC_OS_X_VERSION_MIN_REQUIRED) && MAC_OS_X_VERSION_MIN_REQUIRED >= 101000) || \
    (TARGET_OS_IPHONE && defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 80000)
#define RTHREADS_HAVE_QOS_OVERRIDE 1
#include <pthread/qos.h>
#endif
/* clock_gettime() arrived in macOS 10.12 / iOS 10.0 / tvOS 10.0. Below that
 * the Mach clock service is the only option; see the note on
 * rthreads_calendar_clock below for why it must not be re-acquired per call.
 * Same literal-constant rationale as above. */
#if (TARGET_OS_OSX && defined(MAC_OS_X_VERSION_MIN_REQUIRED) && MAC_OS_X_VERSION_MIN_REQUIRED >= 101200) || \
    (TARGET_OS_IPHONE && defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 100000)
#define RTHREADS_HAVE_CLOCK_GETTIME 1
#endif
#endif

#if defined(__MACH__) && defined(__APPLE__) && !defined(RTHREADS_HAVE_CLOCK_GETTIME)
/* Acquired once for the lifetime of the process.
 *
 * The previous code called host_get_clock_service(mach_host_self(), ...)
 * followed by mach_port_deallocate() on every scond_wait_timeout(). That is
 * wrong twice over:
 *
 *   - the send right returned by mach_host_self() was never deallocated, so
 *     every call leaked a user reference on the host port;
 *   - host_get_clock_service() allocates a fresh port *name* in the task IPC
 *     space which is then immediately freed, so a hot caller (the CoreAudio
 *     write path, the task queue worker, autosave) churns the task's port
 *     name space continuously for the whole session.
 *
 * Neither is acceptable in a function called at audio-buffer rate. */
static clock_serv_t   rthreads_calendar_clock;
static pthread_once_t rthreads_calendar_clock_once = PTHREAD_ONCE_INIT;

static void rthreads_calendar_clock_init(void)
{
   mach_port_t host = mach_host_self();
   host_get_clock_service(host, CALENDAR_CLOCK, &rthreads_calendar_clock);
   mach_port_deallocate(mach_task_self(), host);
}
#endif

struct thread_data
{
   void (*func)(void*);
   void *userdata;
};

#if defined(USE_PSP_THREADS) || defined(USE_VITA_THREADS)
#define RTHREADS_SCE_START
#endif

/* Backends that carry the entry function and its argument in the
 * sthread itself rather than in a separate thread_data block. */
#if defined(RTHREADS_SCE_START) || defined(USE_PS2_THREADS)
#define RTHREADS_NO_THREAD_DATA
#endif

#ifdef RTHREADS_SCE_START
#define PSP_THREAD_DONE     1
#define PSP_THREAD_DETACHED 2

struct psp_thread_start
{
   void (*func)(void*);
   void *userdata;
   retro_atomic_int_t state;
};
#endif

struct sthread
{
#if defined(USE_WIN32_THREADS)
   HANDLE thread;
   DWORD id;
#elif defined(USE_GX_THREADS)
   lwp_t id;
#elif defined(USE_CTR_THREADS)
   Thread id;
#elif defined(RTHREADS_SCE_START)
   struct psp_thread_start *start;
   SceUID id;
#elif defined(USE_WIIU_THREADS)
   OSThread *id;   /* head of one allocation that also holds the stack */
#elif defined(USE_SWITCH_THREADS)
   struct sthread *next;   /* reap list link once detached */
   Thread t;
#elif defined(USE_PS2_THREADS)
   void (*func)(void*);
   void *userdata;
   void *stack;
   struct sthread *next;   /* reap list link once detached */
   s32 id;
   s32 done;               /* semaphore the exit path signals for a joiner */
   retro_atomic_int_t state;
#else
   pthread_t id;
#endif
};

struct slock
{
#if defined(USE_WIN32_THREADS)
   CRITICAL_SECTION lock;
#elif defined(USE_GX_THREADS)
   mutex_t lock;
#elif defined(USE_CTR_THREADS)
   LightLock lock;
#elif defined(USE_PSP_THREADS)
   SceUID lock;
#elif defined(USE_VITA_THREADS)
   SceKernelLwMutexWork lock;
#elif defined(USE_WIIU_THREADS)
   OSFastMutex lock;
#elif defined(USE_SWITCH_THREADS)
   Mutex lock;
#elif defined(USE_PS2_THREADS)
   s32 lock;   /* binary semaphore */
#else
   pthread_mutex_t lock;
#endif
};

#ifdef USE_WIN32_THREADS
/* Win32 condition variable.
 *
 * The same shape as the one in ntdll: the scond is one word holding a
 * lock-free list of waiter blocks that live on the waiting threads'
 * stacks, each block with a flag word of its own. A wait pushes its
 * block with a single compare-and-swap, releases the caller's lock,
 * spins for a bounded time on its own flag word, and only then blocks
 * in the kernel; a wake takes the block off the list and flips the flag,
 * and issues a kernel wake only for a waiter that had committed to
 * blocking, which is what makes a back-to-back wake cost no syscall at
 * all. The spin is the hardware wait where there is one - mwaitx on AMD
 * and umwait on Intel park the core on the flag word's cache line with
 * a TSC deadline, so a sibling hyperthread is not disturbed - and a
 * pause loop elsewhere; a single-processor machine does not spin.
 *
 * The kernel wait is chosen once per process from what ntdll exports:
 * NtWaitForAlertByThreadId / NtAlertThreadByThreadId (Windows 8 and
 * later), the keyed event pair (XP and later), and an auto-reset event
 * per thread where neither exists (the Xbox targets). RTHREADS_SCOND in
 * the environment picks one by name (alert, keyed, event),
 * RTHREADS_SCOND_SPIN a spin mode (none, pause, mwaitx, umwait) and
 * RTHREADS_SCOND_SPIN_CYCLES the TSC bound, for measurement.
 *
 * The spin defaults to the pause loop even where mwaitx or umwait exist:
 * measured against the native variable at 600 dispatches a frame on a
 * 9950X3D, pause is ahead of mwaitx at every thread count, because the
 * hardware wait's exit latency is on the order of the whole interval
 * between dispatches. The hardware waits remain selectable.
 *
 * Bit 0 of the scond word is a lock that wakers and timed-out waiters
 * take to edit the list; pushes never take it. Signal wakes the oldest
 * waiter, broadcast takes the whole list with one compare-and-swap. */

#define SCOND_HEAD_LOCK ((uintptr_t)1)
#define SCOND_HEAD_MASK (~(uintptr_t)1)

#define SCOND_W_WOKEN   1  /* a waker has taken this block */
#define SCOND_W_ASLEEP  2  /* the waiter committed to the kernel wait */

struct scond_waiter
{
   struct scond_waiter *next;
   HANDLE event;               /* event fallback only */
   retro_atomic_int_t flags;
   DWORD tid;
};

enum scond_sleep_kind
{
   SCOND_SLEEP_ALERT = 1,
   SCOND_SLEEP_KEYED,
   SCOND_SLEEP_EVENT
};

enum scond_spin_kind
{
   SCOND_SPIN_NONE = 0,
   SCOND_SPIN_PAUSE,
   SCOND_SPIN_MWAITX,
   SCOND_SPIN_UMWAIT
};

typedef LONG (NTAPI *scond_nt_wait_alert_t)(void *hint, LARGE_INTEGER *timeout);
typedef LONG (NTAPI *scond_nt_alert_tid_t)(HANDLE tid);
typedef LONG (NTAPI *scond_nt_keyed_t)(HANDLE h, void *key, BOOLEAN alertable,
      LARGE_INTEGER *timeout);
typedef LONG (NTAPI *scond_nt_create_keyed_t)(HANDLE *h, ULONG access,
      void *attr, ULONG flags);

static struct
{
   scond_nt_wait_alert_t wait_alert;
   scond_nt_alert_tid_t alert_tid;
   scond_nt_keyed_t wait_keyed;
   scond_nt_keyed_t release_keyed;
   HANDLE keyed;
   retro_atomic_int_t state;   /* 0 unresolved, 1 resolving, 2 ready */
   int sleep;
   int spin;
   unsigned spin_cycles;       /* TSC bound for the hardware waits */
   unsigned spin_iters;        /* iteration bound for the pause loop */
   DWORD tls_event;
} scond_g;

static void scond_global_init(void);
#endif

#ifdef USE_WIIU_THREADS
/* One node per blocked waiter, living on that waiter's stack for
 * exactly the duration of the wait. An auto-reset event both parks
 * the waiter and carries the wake, so a signal issued between
 * enqueue and OSWaitEvent latches instead of getting lost. */
struct wiiu_cond_waiter
{
   struct wiiu_cond_waiter *next;
   OSEvent ev;
};
#endif

#ifdef USE_PS2_THREADS
/* One node per blocked waiter, on that waiter's stack for the
 * duration of the wait. The EE kernel counts wakeups per thread, so
 * a WakeupThread issued between enqueue and SleepThread is kept
 * rather than lost; the timed variant arms a timer alarm whose
 * handler, in interrupt context, marks the node and wakes the
 * thread. The lists are guarded by disabling interrupts: the kernel
 * never time-slices, so on a single core that is the only way a
 * critical section this short can be entered twice. */
struct ps2_cond_waiter
{
   struct ps2_cond_waiter *next;
   s32 tid;
   volatile int fired;
};
#endif

struct scond
{
#if defined(USE_WIN32_THREADS)
   retro_atomic_ptr_t head;   /* struct scond_waiter * | SCOND_HEAD_LOCK */
#elif defined(USE_GX_THREADS)
   cond_t cond;
#elif defined(USE_CTR_THREADS)
   CondVar cond;
#elif defined(USE_PSP_THREADS)
   SceUID gate;   /* binary semaphore guarding waiters */
   SceUID sema;   /* wait tokens, one credit per wake  */
   int waiters;
#elif defined(USE_VITA_THREADS)
   SceKernelLwCondWork work;
   SceKernelLwMutexWork *assoc;   /* the lock this condvar is bound to */
   retro_atomic_int_t bound;      /* nonzero once work exists */
#elif defined(USE_WIIU_THREADS)
   struct wiiu_cond_waiter *head; /* FIFO of parked waiters */
   struct wiiu_cond_waiter *tail;
   OSFastMutex gate;
#elif defined(USE_SWITCH_THREADS)
   CondVar cond;
#elif defined(USE_PS2_THREADS)
   struct ps2_cond_waiter *head;  /* FIFO of sleeping waiters */
   struct ps2_cond_waiter *tail;
#elif defined(RTHREADS_FUTEX_SCOND)
   retro_atomic_int_t seq;        /* bumped by every signal; futex word */
   retro_atomic_int_t waiters;    /* threads between enqueue and wake */
#else
   pthread_cond_t cond;
#endif
};

#ifdef USE_PS2_THREADS
#define PS2_THREAD_DONE     1
#define PS2_THREAD_DETACHED 2

static sthread_t *ps2_reap_list;

/* Detached threads park here until their DONE bit is set, since the
 * EE kernel frees only the control block on exit and the stack is
 * ours to release from another thread. */
static void ps2_reap(void)
{
   sthread_t *done = NULL, **pp;
   DIntr();
   pp = &ps2_reap_list;
   while (*pp)
   {
      sthread_t *z = *pp;
      if (retro_atomic_load_acquire_int(&z->state) & PS2_THREAD_DONE)
      {
         *pp     = z->next;
         z->next = done;
         done    = z;
      }
      else
         pp      = &z->next;
   }
   EIntr();
   while (done)
   {
      sthread_t *z = done;
      done         = z->next;
      DeleteSema(z->done);
      free(z->stack);
      free(z);
   }
}

static void ps2_thread_wrap(void *arg)
{
   sthread_t *t = (sthread_t*)arg;
   s32 done     = t->done;
   t->func(t->userdata);
   /* From here to the exit syscall nothing may run in this thread's
    * place: the joiner or the reaper frees the stack it is still
    * standing on. Priority 0 is the top of the band and the EE
    * kernel never time-slices equal priorities, so signaling below
    * readies the joiner without switching to it. Nothing in t is
    * touched once DONE is published. */
   ChangeThreadPriority(GetThreadId(), 0);
   if (!(retro_atomic_fetch_or_int(&t->state, PS2_THREAD_DONE)
            & PS2_THREAD_DETACHED))
      SignalSema(done);
   ExitDeleteThread();
}

static u64 ps2_cond_alarm(s32 id, u64 scheduled, u64 actual,
      void *arg, void *pc)
{
   struct ps2_cond_waiter *w = (struct ps2_cond_waiter*)arg;
   (void)id;
   (void)scheduled;
   (void)actual;
   (void)pc;
   w->fired = 1;
   iWakeupThread(w->tid);
   return 0;   /* one-shot */
}

static void ps2_cond_enqueue(scond_t *cond, struct ps2_cond_waiter *w)
{
   w->next  = NULL;
   w->fired = 0;
   w->tid   = GetThreadId();
   DIntr();
   if (cond->tail)
      cond->tail->next = w;
   else
      cond->head       = w;
   cond->tail          = w;
   EIntr();
}

/* Removes w if it is still queued; returns whether it was. */
static bool ps2_cond_withdraw(scond_t *cond, struct ps2_cond_waiter *w)
{
   struct ps2_cond_waiter *cur, *prev = NULL;
   DIntr();
   cur = cond->head;
   while (cur && cur != w)
   {
      prev = cur;
      cur  = cur->next;
   }
   if (cur)
   {
      if (prev)
         prev->next = w->next;
      else
         cond->head = w->next;
      if (cond->tail == w)
         cond->tail = prev;
   }
   EIntr();
   return cur != NULL;
}
#endif

#ifdef USE_SWITCH_THREADS
/* libnx has no detach: a Thread stays live until threadClose, and a
 * thread cannot close itself, since that unmaps the stack it is
 * running on. Detached threads park here and are reaped by the next
 * create or detach once their handle has signaled, which bounds what
 * lingers to the detached threads that finished since the last such
 * call. */
static Mutex     switch_reap_lock;
static sthread_t *switch_reap_list;

static void switch_reap(void)
{
   sthread_t **pp;
   mutexLock(&switch_reap_lock);
   pp = &switch_reap_list;
   while (*pp)
   {
      sthread_t *z = *pp;
      if (svcWaitSynchronizationSingle(z->t.handle, 0) == 0)
      {
         *pp = z->next;
         threadClose(&z->t);
         free(z);
      }
      else
         pp = &z->next;
   }
   mutexUnlock(&switch_reap_lock);
}
#endif

#if defined(RTHREADS_SCE_START)
/* Joinable threads finish into the dormant state and are reaped by
 * sthread_join. A detached thread reaps itself: whichever of the
 * finishing thread and sthread_detach runs second sees the other's
 * bit and frees the start block, and self-reaping also removes the
 * kernel thread so nothing stays dormant. */
static int psp_thread_wrap(SceSize args_size, void *argp)
{
   struct psp_thread_start *s = *(struct psp_thread_start**)argp;
   (void)args_size;
   s->func(s->userdata);
   if (retro_atomic_fetch_or_int(&s->state, PSP_THREAD_DONE)
         & PSP_THREAD_DETACHED)
   {
      free(s);
      sceKernelExitDeleteThread(0);
   }
   return 0;
}
#elif defined(USE_CTR_THREADS) || defined(USE_SWITCH_THREADS)
static void thread_wrap(void *data_)
{
   struct thread_data *data = (struct thread_data*)data_;
   if (!data)
      return;
   data->func(data->userdata);
   free(data);
}
#elif defined(USE_WIIU_THREADS)
static int thread_wrap(int argc, const char **argv)
{
   struct thread_data *data = (struct thread_data*)argv;
   (void)argc;
   if (!data)
      return 0;
   data->func(data->userdata);
   free(data);
   return 0;
}

/* Runs on the scheduler once the thread is fully dead - after a join,
 * or after exit for a detached thread - so the control block and the
 * stack are never freed while the thread could still touch them. The
 * OSThread heads the allocation. */
static void wiiu_thread_dealloc(OSThread *thread, void *stack)
{
   (void)stack;
   free(thread);
}
#elif defined(USE_PS2_THREADS)
/* ps2_thread_wrap above takes the sthread itself as its argument. */
#else
#ifdef USE_WIN32_THREADS
static DWORD CALLBACK thread_wrap(void *data_)
#else
static void *thread_wrap(void *data_)
#endif
{
   struct thread_data *data = (struct thread_data*)data_;
   if (!data)
      return 0;
   data->func(data->userdata);
   free(data);
   return 0;
}
#endif

sthread_t *sthread_create(void (*thread_func)(void*), void *userdata)
{
   return sthread_create_with_priority(thread_func, userdata, 0);
}

#if !defined(USE_WIN32_THREADS) && !defined(USE_GX_THREADS) \
      && !defined(USE_CTR_THREADS) && !defined(USE_PSP_THREADS) \
      && !defined(USE_VITA_THREADS) && !defined(USE_WIIU_THREADS) \
      && !defined(USE_SWITCH_THREADS) && !defined(USE_PS2_THREADS) \
      && !defined(__HAIKU__) && !defined(__EMSCRIPTEN__)
#define HAVE_THREAD_ATTR
#endif

sthread_t *sthread_create_with_priority(void (*thread_func)(void*), void *userdata, int thread_priority)
{
#ifdef HAVE_THREAD_ATTR
   pthread_attr_t thread_attr;
   bool thread_attr_needed  = false;
#endif
   bool thread_created      = false;
#ifndef RTHREADS_NO_THREAD_DATA
   struct thread_data *data = NULL;
#endif
   sthread_t *thread        = (sthread_t*)malloc(sizeof(*thread));

   if (!thread)
      return NULL;

#ifndef RTHREADS_NO_THREAD_DATA
   if (!(data = (struct thread_data*)malloc(sizeof(*data))))
   {
      free(thread);
      return NULL;
   }

   data->func               = thread_func;
   data->userdata           = userdata;
#endif

#if defined(USE_WIN32_THREADS)
   thread->id               = 0;
   thread->thread           = CreateThread(NULL, 0, thread_wrap,
         data, 0, &thread->id);
   thread_created           = !!thread->thread;
#elif defined(USE_GX_THREADS)
   {
      /* LWP priorities run 0..127 with higher numbers scheduled
       * first; workers default to the middle of the band. */
      u8 prio        = 64;
      if (thread_priority >= 1 && thread_priority <= 100)
         prio        = (u8)(1 + ((thread_priority - 1) * 126) / 99);
      thread->id     = LWP_THREAD_NULL;
      thread_created = LWP_CreateThread(&thread->id, thread_wrap,
            data, NULL, STACKSIZE, prio) == 0;
   }
#elif defined(USE_CTR_THREADS)
   {
      /* Userland priorities run 0x18..0x3F with lower numbers
       * scheduled first. A worker sits one step above its creator so
       * the work it is handed is acted on promptly; an explicit
       * rthreads priority maps across the whole band instead. New3DS
       * gets workers on the third core, which the application owns
       * outright; elsewhere the exheader's default core is used. */
      s32 prio       = 0x30;
      int core_id    = -2;
      bool new3ds    = false;

      APT_CheckNew3DS(&new3ds);
      if (new3ds)
         core_id     = 2;

      svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
      if (thread_priority >= 1 && thread_priority <= 100)
         prio        = 0x3F - ((thread_priority - 1) * (0x3F - 0x18)) / 99;
      else
         prio        = prio - 1;
      if (prio < 0x18)
         prio        = 0x18;
      if (prio > 0x3F)
         prio        = 0x3F;

      thread->id     = threadCreate(thread_wrap, data,
            STACKSIZE, prio, core_id, false);
      thread_created = !!thread->id;
   }
#elif defined(USE_PSP_THREADS)
   {
      /* Priorities run 0x10..0x6F with lower numbers scheduled first.
       * Workers inherit the creator's priority; an explicit rthreads
       * priority maps across the whole band instead. The kernel copies
       * the argument block when the thread starts, so the pointer to
       * the start block travels by value. */
      struct psp_thread_start *start = (struct psp_thread_start*)
            malloc(sizeof(*start));
      int prio;

      if (!start)
      {
         free(thread);
         return NULL;
      }

      start->func     = thread_func;
      start->userdata = userdata;
      retro_atomic_int_init(&start->state, 0);

      prio            = sceKernelGetThreadCurrentPriority();
      if (thread_priority >= 1 && thread_priority <= 100)
         prio         = 0x6F - ((thread_priority - 1) * (0x6F - 0x10)) / 99;
      if (prio < 0x10)
         prio         = 0x10;
      if (prio > 0x6F)
         prio         = 0x6F;

      thread->start   = start;
      thread->id      = sceKernelCreateThread("rarch_thread",
            psp_thread_wrap, prio, STACKSIZE,
            PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU, NULL);
      if (thread->id >= 0)
      {
         if (sceKernelStartThread(thread->id, sizeof(start), &start) >= 0)
            thread_created = true;
         else
            sceKernelDeleteThread(thread->id);
      }
      if (!thread_created)
         free(start);
   }
#elif defined(USE_PS2_THREADS)
   {
      /* Priorities run 0..127 with lower numbers scheduled first and
       * no time-slicing within a level. Workers inherit the creator's
       * priority; an explicit rthreads priority maps across 1..127.
       * The stack is ours, handed to the kernel with its size, and
       * the sthread itself is the start argument. */
      ee_thread_t        th;
      ee_sema_t          sema;
      ee_thread_status_t st;
      int prio;

      ps2_reap();
      thread->func     = thread_func;
      thread->userdata = userdata;
      thread->next     = NULL;
      thread->id       = -1;
      thread->done     = -1;
      retro_atomic_int_init(&thread->state, 0);
      thread->stack    = memalign(16, STACKSIZE);
      if (!thread->stack)
      {
         free(thread);
         return NULL;
      }

      memset(&sema, 0, sizeof(sema));
      sema.init_count  = 0;
      sema.max_count   = 1;
      thread->done     = CreateSema(&sema);

      memset(&st, 0, sizeof(st));
      ReferThreadStatus(GetThreadId(), &st);
      prio             = st.current_priority;
      if (thread_priority >= 1 && thread_priority <= 100)
         prio          = 127 - ((thread_priority - 1) * 126) / 99;
      if (prio < 0)
         prio          = 0;
      if (prio > 127)
         prio          = 127;

      memset(&th, 0, sizeof(th));
      th.func             = (void*)ps2_thread_wrap;
      th.stack            = thread->stack;
      th.stack_size       = STACKSIZE;
      th.gp_reg           = &_gp;
      th.initial_priority = prio;
      if (thread->done >= 0)
         thread->id       = CreateThread(&th);
      if (thread->id >= 0)
      {
         if (StartThread(thread->id, thread) >= 0)
            thread_created = true;
         else
            DeleteThread(thread->id);
      }
      if (!thread_created)
      {
         if (thread->done >= 0)
            DeleteSema(thread->done);
         free(thread->stack);
      }
   }
#elif defined(USE_SWITCH_THREADS)
   {
      /* Priorities run 0x00..0x3F with lower numbers scheduled first,
       * within whatever band the process is allowed; 0x2C is where
       * the main thread runs. Workers inherit the creator's priority,
       * and an explicit rthreads priority maps across 0x1C..0x3F,
       * falling back to inheriting if the kernel refuses it. libnx
       * allocates and maps the stack itself, so the thread lands on
       * the process's default core. */
      s32 prio = 0x2C;
      Result rc;

      switch_reap();
      svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
      if (thread_priority >= 1 && thread_priority <= 100)
      {
         s32 want = 0x3F - ((thread_priority - 1) * (0x3F - 0x1C)) / 99;
         rc       = threadCreate(&thread->t, thread_wrap, data,
               NULL, STACKSIZE, want, -2);
         if (R_FAILED(rc))
            rc    = threadCreate(&thread->t, thread_wrap, data,
                  NULL, STACKSIZE, prio, -2);
      }
      else
         rc       = threadCreate(&thread->t, thread_wrap, data,
               NULL, STACKSIZE, prio, -2);

      if (R_SUCCEEDED(rc))
      {
         if (R_SUCCEEDED(threadStart(&thread->t)))
            thread_created = true;
         else
            threadClose(&thread->t);
      }
   }
#elif defined(USE_WIIU_THREADS)
   {
      /* Priorities run 0..31 with lower numbers scheduled first;
       * workers inherit the creator's priority, and an explicit
       * rthreads priority maps across the whole band. The control
       * block and the stack are one caller-owned allocation, handed
       * back by the deallocator; the stack pointer passed in is its
       * top, since PowerPC stacks grow down. */
      int32_t prio;
      uint8_t *block = (uint8_t*)memalign(16,
            sizeof(OSThread) + STACKSIZE);

      if (!block)
      {
         free(data);
         free(thread);
         return NULL;
      }
      memset(block, 0, sizeof(OSThread));

      prio = OSGetThreadPriority(OSGetCurrentThread());
      if (thread_priority >= 1 && thread_priority <= 100)
         prio = 31 - ((thread_priority - 1) * 31) / 99;
      if (prio < 0)
         prio = 0;
      if (prio > 31)
         prio = 31;

      thread->id = (OSThread*)block;
      if (OSCreateThread(thread->id, thread_wrap, 0, (char*)data,
            block + sizeof(OSThread) + STACKSIZE, STACKSIZE, prio,
            OS_THREAD_ATTRIB_AFFINITY_ANY))
      {
         OSSetThreadDeallocator(thread->id, wiiu_thread_dealloc);
         OSResumeThread(thread->id);
         thread_created = true;
      }
      else
         free(block);
   }
#elif defined(USE_VITA_THREADS)
   {
      /* Priorities run 64..191 with lower numbers scheduled first.
       * Workers inherit the creator's priority; an explicit rthreads
       * priority maps across the whole band instead. The kernel
       * copies the argument block when the thread starts, so the
       * pointer to the start block travels by value. */
      struct psp_thread_start *start = (struct psp_thread_start*)
            malloc(sizeof(*start));
      int prio;

      if (!start)
      {
         free(thread);
         return NULL;
      }

      start->func     = thread_func;
      start->userdata = userdata;
      retro_atomic_int_init(&start->state, 0);

      prio            = sceKernelGetThreadCurrentPriority();
      if (thread_priority >= 1 && thread_priority <= 100)
         prio         = 191 - ((thread_priority - 1) * (191 - 64)) / 99;
      if (prio < 64)
         prio         = 64;
      if (prio > 191)
         prio         = 191;

      thread->start   = start;
      thread->id      = sceKernelCreateThread("rarch_thread",
            psp_thread_wrap, prio, STACKSIZE, 0, 0, NULL);
      if (thread->id >= 0)
      {
         if (sceKernelStartThread(thread->id, sizeof(start), &start) >= 0)
            thread_created = true;
         else
            sceKernelDeleteThread(thread->id);
      }
      if (!thread_created)
         free(start);
   }
#else
   thread->id               = 0;
#ifdef HAVE_THREAD_ATTR
   pthread_attr_init(&thread_attr);

   if ((thread_priority >= 1) && (thread_priority <= 100))
   {
      struct sched_param sp;
      memset(&sp, 0, sizeof(struct sched_param));
      sp.sched_priority = thread_priority;
      pthread_attr_setschedpolicy(&thread_attr, SCHED_RR);
      pthread_attr_setschedparam(&thread_attr, &sp);

      thread_attr_needed = true;
   }

#if defined(__APPLE__)
   /* Default stack size on Apple is 512Kb;
    * for PS2 disc scanning and other reasons, we'd like 2MB. */
   pthread_attr_setstacksize(&thread_attr , 0x200000 );
   thread_attr_needed = true;
#endif

   if (thread_attr_needed)
      thread_created = pthread_create(&thread->id, &thread_attr, thread_wrap, data) == 0;
   else
      thread_created = pthread_create(&thread->id, NULL, thread_wrap, data) == 0;

   pthread_attr_destroy(&thread_attr);
#else
   thread_created    = pthread_create(&thread->id, NULL, thread_wrap, data) == 0;
#endif
#endif

   if (thread_created)
      return thread;
#ifndef RTHREADS_NO_THREAD_DATA
   free(data);
#endif
   free(thread);
   return NULL;
}

#ifdef RTHREADS_HAVE_AFFINITY
/* The set of CPUs whose maximum frequency matches the highest in the
 * system, intersected with what this thread may already run on.
 * Computed once; on a homogeneous part it comes out equal to the
 * allowed set and the pin is skipped. */
static unsigned long rthreads_fast_mask[4];
static int           rthreads_fast_state; /* 0 unknown, 1 pin, -1 no-op */

static void rthreads_find_fast_cores(void)
{
   unsigned long allowed[4];
   unsigned long best = 0;
   unsigned      i;
   int           ncpu = (int)(sizeof(allowed) * 8);

   memset(allowed, 0, sizeof(allowed));
   if (syscall(__NR_sched_getaffinity, 0, sizeof(allowed), allowed) <= 0)
   {
      rthreads_fast_state = -1;
      return;
   }
   memset(rthreads_fast_mask, 0, sizeof(rthreads_fast_mask));
   for (i = 0; i < (unsigned)ncpu; i++)
   {
      char path[96];
      FILE *f;
      unsigned long khz = 0;
      if (!(allowed[i / (8 * sizeof(unsigned long))]
               & (1ul << (i % (8 * sizeof(unsigned long))))))
         continue;
      sprintf(path, RTHREADS_CPU_SYSFS "/cpu%u/cpufreq/cpuinfo_max_freq", i);
      f = fopen(path, "r");
      if (!f)
         continue;
      if (fscanf(f, "%lu", &khz) != 1)
         khz = 0;
      fclose(f);
      if (khz > best)
      {
         best = khz;
         memset(rthreads_fast_mask, 0, sizeof(rthreads_fast_mask));
      }
      if (khz && khz == best)
         rthreads_fast_mask[i / (8 * sizeof(unsigned long))]
            |= 1ul << (i % (8 * sizeof(unsigned long)));
   }
   /* Nothing readable, or every allowed CPU is a fast one: leave the
    * thread where the scheduler puts it. */
   if (!best || !memcmp(rthreads_fast_mask, allowed, sizeof(allowed)))
      rthreads_fast_state = -1;
   else
      rthreads_fast_state = 1;
}
#endif

#ifdef USE_WIN32_THREADS
/* Windows 10 1607+ describes every logical CPU through
 * GetSystemCpuSetInformation, whose EfficiencyClass rises with
 * performance, and SetThreadSelectedCpuSets is a soft preference the
 * scheduler may still override under pressure - the right strength
 * for "prefer". Both are resolved at run time so builds that still
 * target older Windows keep linking and simply report false there,
 * and the records are read by offset since older SDKs lack the type. */
typedef BOOL (WINAPI *rthreads_get_cpusets_t)(void*, ULONG, ULONG*,
      HANDLE, ULONG);
typedef BOOL (WINAPI *rthreads_set_cpusets_t)(HANDLE, const ULONG*, ULONG);

static ULONG  rthreads_fast_ids[64];
static ULONG  rthreads_fast_count;
static int    rthreads_fast_state; /* 0 unknown, 1 pin, -1 no-op */
static rthreads_set_cpusets_t rthreads_set_cpusets;

static void rthreads_find_fast_cores(void)
{
   HMODULE k32                    = GetModuleHandleA("kernel32.dll");
   rthreads_get_cpusets_t getinfo = NULL;
   unsigned char *buf             = NULL;
   ULONG len                      = 0;
   ULONG off;
   BYTE  best                     = 0;
   ULONG total                    = 0;

   rthreads_fast_state = -1;
   if (!k32)
      return;
   getinfo              = (rthreads_get_cpusets_t)(void (*)(void))
      GetProcAddress(k32, "GetSystemCpuSetInformation");
   rthreads_set_cpusets = (rthreads_set_cpusets_t)(void (*)(void))
      GetProcAddress(k32, "SetThreadSelectedCpuSets");
   if (!getinfo || !rthreads_set_cpusets)
      return;
   getinfo(NULL, 0, &len, GetCurrentProcess(), 0);
   if (!len || !(buf = (unsigned char*)malloc(len)))
      return;
   if (!getinfo(buf, len, &len, GetCurrentProcess(), 0))
   {
      free(buf);
      return;
   }
   /* SYSTEM_CPU_SET_INFORMATION: Size at 0, Type at 4 (0 = CpuSet),
    * CpuSet.Id at 8, CpuSet.EfficiencyClass at 18. */
   for (off = 0; off + 20 <= len; )
   {
      DWORD size = *(DWORD*)(buf + off);
      if (size < 20)
         break;
      if (*(DWORD*)(buf + off + 4) == 0)
      {
         BYTE cls = buf[off + 18];
         total++;
         if (cls > best)
         {
            best                = cls;
            rthreads_fast_count = 0;
         }
         if (cls == best && rthreads_fast_count
               < sizeof(rthreads_fast_ids) / sizeof(rthreads_fast_ids[0]))
            rthreads_fast_ids[rthreads_fast_count++]
               = *(DWORD*)(buf + off + 8);
      }
      off += size;
   }
   free(buf);
   /* Every CPU in the top class means a homogeneous part. */
   if (rthreads_fast_count && rthreads_fast_count < total)
      rthreads_fast_state = 1;
}
#endif

bool sthread_prefer_fast_cores(void)
{
#if defined(RTHREADS_HAVE_AFFINITY)
   if (!rthreads_fast_state)
      rthreads_find_fast_cores();
   if (rthreads_fast_state < 0)
      return false;
   return syscall(__NR_sched_setaffinity, 0,
         sizeof(rthreads_fast_mask), rthreads_fast_mask) == 0;
#elif defined(USE_WIN32_THREADS)
   if (!rthreads_fast_state)
      rthreads_find_fast_cores();
   if (rthreads_fast_state < 0)
      return false;
   return rthreads_set_cpusets(GetCurrentThread(),
         rthreads_fast_ids, rthreads_fast_count) != 0;
#elif defined(RTHREADS_HAVE_QOS_OVERRIDE)
   /* Apple silicon offers no affinity; quality of service is what
    * steers a thread onto the performance cores. */
   return pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0) == 0;
#elif defined(USE_WIIU_THREADS)
   /* Three identical cores, but core 1 carries a 2 MiB L2 against
    * 512 KiB on the others, which is why Cafe OS keeps the main
    * application thread there. */
   return OSSetThreadAffinity(OSGetCurrentThread(),
         OS_THREAD_ATTRIB_AFFINITY_CPU1) != FALSE;
#else
   return false;
#endif
}

bool sthread_raise_current_priority(void)
{
#if defined(USE_WIN32_THREADS)
   return SetThreadPriority(GetCurrentThread(),
         THREAD_PRIORITY_TIME_CRITICAL) != 0;
#elif defined(USE_GX_THREADS)
   /* Into the upper quarter of the band: above the workers this file
    * creates, below anything the system parks at the very top. */
   LWP_SetThreadPriority(LWP_GetSelf(), 100);
   return true;
#elif defined(USE_CTR_THREADS)
   {
      s32 prio = 0x30;
      svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
      prio -= 2;
      if (prio < 0x18)
         prio  = 0x18;
      return svcSetThreadPriority(CUR_THREAD_HANDLE, prio) == 0;
   }
#elif defined(USE_PSP_THREADS)
   {
      int prio = sceKernelGetThreadCurrentPriority() - 4;
      if (prio < 0x10)
         prio  = 0x10;
      return sceKernelChangeThreadPriority(
            sceKernelGetThreadId(), prio) == 0;
   }
#elif defined(USE_PS2_THREADS)
   {
      ee_thread_status_t st;
      int prio;
      memset(&st, 0, sizeof(st));
      ReferThreadStatus(GetThreadId(), &st);
      prio = st.current_priority - 4;
      if (prio < 0)
         prio = 0;
      return ChangeThreadPriority(GetThreadId(), prio) >= 0;
   }
#elif defined(USE_SWITCH_THREADS)
   {
      s32 prio = 0x2C;
      svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
      prio -= 2;
      if (prio < 0x1C)
         prio  = 0x1C;
      return R_SUCCEEDED(svcSetThreadPriority(CUR_THREAD_HANDLE, prio));
   }
#elif defined(USE_WIIU_THREADS)
   {
      OSThread *self = OSGetCurrentThread();
      int32_t prio   = OSGetThreadPriority(self) - 4;
      if (prio < 0)
         prio        = 0;
      return OSSetThreadPriority(self, prio) != FALSE;
   }
#elif defined(USE_VITA_THREADS)
   {
      int prio = sceKernelGetThreadCurrentPriority() - 8;
      if (prio < 64)
         prio  = 64;
      return sceKernelChangeThreadPriority(
            sceKernelGetThreadId(), prio) == 0;
   }
#elif defined(__ANDROID__)
   /* Bionic lets an app move its own threads into the audio band
    * without privilege; -16 is ANDROID_PRIORITY_AUDIO. */
   return setpriority(PRIO_PROCESS, gettid(), -16) == 0;
#elif defined(__MACH__) && defined(__APPLE__)
   /* The time-constraint policy is what makes a thread real-time on
    * Darwin: pthread_setschedparam() only moves it within the
    * time-shared band. A period close to a small audio IO cycle with
    * a fraction of it as the budget is what the HAL's own IO thread
    * runs under; a thread that overruns its budget is demoted by the
    * kernel, not killed. Needs no privilege. */
   {
      struct thread_time_constraint_policy policy;
      mach_timebase_info_data_t tb;
      double ns_per_tick;
      if (mach_timebase_info(&tb) != KERN_SUCCESS || !tb.denom)
         return false;
      ns_per_tick         = (double)tb.numer / (double)tb.denom;
      policy.period       = (uint32_t)(2900000.0 / ns_per_tick);
      policy.computation  = (uint32_t)( 750000.0 / ns_per_tick);
      policy.constraint   = (uint32_t)(2900000.0 / ns_per_tick);
      policy.preemptible  = TRUE;
      return thread_policy_set(pthread_mach_thread_np(pthread_self()),
            THREAD_TIME_CONSTRAINT_POLICY, (thread_policy_t)&policy,
            THREAD_TIME_CONSTRAINT_POLICY_COUNT) == KERN_SUCCESS;
   }
#elif defined(RTHREADS_HAVE_SCHEDPARAM)
   /* Real-time round-robin at a middling priority: above every
    * time-shared thread, below anything the system runs at the top of
    * the band. Distributions that grant the audio group an rtprio
    * limit allow this without root; where it is refused the thread
    * simply keeps its default, which is the caller's contract. */
   struct sched_param sp;
   int lo  = sched_get_priority_min(SCHED_RR);
   int hi  = sched_get_priority_max(SCHED_RR);
   memset(&sp, 0, sizeof(sp));
   if (lo < 0 || hi < lo)
      return false;
   sp.sched_priority = lo + (hi - lo) / 2;
   return pthread_setschedparam(pthread_self(), SCHED_RR, &sp) == 0;
#else
   return false;
#endif
}

void sthread_setname(const char *name)
{
#if defined(__linux__) && !defined(USE_WIN32_THREADS)
   /* prctl rather than pthread_setname_np: it is available on every
    * bionic and glibc version we build against, and takes the name as
    * a plain buffer, so the caller cannot be rejected outright for
    * overrunning the kernel's 16-byte limit. Copied rather than passed
    * through so an over-long name truncates instead of failing. */
   char buf[16];
   size_t i;
   if (!name)
      return;
   for (i = 0; i < sizeof(buf) - 1 && name[i]; i++)
      buf[i] = name[i];
   buf[i] = '\0';
   prctl(PR_SET_NAME, buf, 0, 0, 0);
#elif defined(__APPLE__)
   /* pthread_setname_np is 10.6; it is looked up at run time so one
    * binary builds against, and runs on, 10.5 as well.  The pointer is
    * kept once found: it cannot come and go while the process runs. */
   static int (*setname)(const char*) = NULL;
   static int looked_up               = 0;
   if (!name)
      return;
   if (!looked_up)
   {
      *(void**)&setname = dlsym(RTLD_DEFAULT, "pthread_setname_np");
      looked_up         = 1;
   }
   if (setname)
      setname(name);
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
   if (!name)
      return;
   pthread_set_name_np(pthread_self(), name);
#elif defined(__NetBSD__)
   if (!name)
      return;
   pthread_setname_np(pthread_self(), "%s", (void*)name);
#else
   (void)name;
#endif
}

int sthread_detach(sthread_t *thread)
{
#if defined(USE_WIN32_THREADS)
   if (!thread)
      return 0;
   CloseHandle(thread->thread);
   free(thread);
   return 0;
#elif defined(USE_GX_THREADS)
   /* LWP has no detach: a thread that is never joined keeps its
    * control block and stack allocated for the life of the process.
    * Long-lived workers are unaffected; anything churning through
    * short-lived threads should join them on this platform. */
   if (!thread)
      return 0;
   free(thread);
   return 0;
#elif defined(USE_CTR_THREADS)
   if (!thread)
      return 0;
   threadDetach(thread->id);
   free(thread);
   return 0;
#elif defined(USE_PSP_THREADS)
   if (!thread)
      return 0;
   if (retro_atomic_fetch_or_int(&thread->start->state,
            PSP_THREAD_DETACHED) & PSP_THREAD_DONE)
   {
      /* Already finished: it is dormant (or about to be) and can no
       * longer reap itself, so reap it here. */
      sceKernelWaitThreadEnd(thread->id, NULL);
      sceKernelDeleteThread(thread->id);
      free(thread->start);
   }
   free(thread);
   return 0;
#elif defined(USE_VITA_THREADS)
   if (!thread)
      return 0;
   if (retro_atomic_fetch_or_int(&thread->start->state,
            PSP_THREAD_DETACHED) & PSP_THREAD_DONE)
   {
      /* Already finished: it is dormant (or about to be) and can no
       * longer reap itself, so reap it here. */
      sceKernelWaitThreadEnd(thread->id, NULL, NULL);
      sceKernelDeleteThread(thread->id);
      free(thread->start);
   }
   free(thread);
   return 0;
#elif defined(USE_WIIU_THREADS)
   if (!thread)
      return 0;
   /* The deallocator runs once the thread exits and frees its block. */
   OSDetachThread(thread->id);
   free(thread);
   return 0;
#elif defined(USE_PS2_THREADS)
   if (!thread)
      return 0;
   ps2_reap();
   if (retro_atomic_fetch_or_int(&thread->state, PS2_THREAD_DETACHED)
         & PS2_THREAD_DONE)
   {
      /* Already exited: nothing left running on that stack. */
      DeleteSema(thread->done);
      free(thread->stack);
      free(thread);
      return 0;
   }
   DIntr();
   thread->next  = ps2_reap_list;
   ps2_reap_list = thread;
   EIntr();
   return 0;
#elif defined(USE_SWITCH_THREADS)
   if (!thread)
      return 0;
   switch_reap();
   mutexLock(&switch_reap_lock);
   thread->next     = switch_reap_list;
   switch_reap_list = thread;
   mutexUnlock(&switch_reap_lock);
   return 0;
#else
   int ret;
   if (!thread)
      return 0;
   ret = pthread_detach(thread->id);
   free(thread);
   return ret;
#endif
}

void sthread_join(sthread_t *thread)
{
   if (!thread)
      return;
#if defined(USE_WIN32_THREADS)
   WaitForSingleObject(thread->thread, INFINITE);
   CloseHandle(thread->thread);
#elif defined(USE_GX_THREADS)
   LWP_JoinThread(thread->id, NULL);
#elif defined(USE_CTR_THREADS)
   threadJoin(thread->id, UINT64_MAX);
   threadFree(thread->id);
#elif defined(USE_PSP_THREADS)
   sceKernelWaitThreadEnd(thread->id, NULL);
   sceKernelDeleteThread(thread->id);
   free(thread->start);
#elif defined(USE_VITA_THREADS)
   sceKernelWaitThreadEnd(thread->id, NULL, NULL);
   sceKernelDeleteThread(thread->id);
   free(thread->start);
#elif defined(USE_WIIU_THREADS)
   /* The deallocator frees the control block and stack. */
   OSJoinThread(thread->id, NULL);
#elif defined(USE_SWITCH_THREADS)
   threadWaitForExit(&thread->t);
   threadClose(&thread->t);
#elif defined(USE_PS2_THREADS)
   WaitSema(thread->done);
   DeleteSema(thread->done);
   free(thread->stack);
#else
   pthread_join(thread->id, NULL);
#endif
   free(thread);
}

bool sthread_isself(sthread_t *thread)
{
#if defined(USE_WIN32_THREADS)
   return thread ? GetCurrentThreadId() == thread->id        : false;
#elif defined(USE_GX_THREADS)
   return thread ? LWP_GetSelf() == thread->id               : false;
#elif defined(USE_CTR_THREADS)
   return thread ? threadGetCurrent() == thread->id          : false;
#elif defined(RTHREADS_SCE_START)
   return thread ? sceKernelGetThreadId() == thread->id      : false;
#elif defined(USE_WIIU_THREADS)
   return thread ? OSGetCurrentThread() == thread->id        : false;
#elif defined(USE_SWITCH_THREADS)
   return thread ? threadGetSelf() == &thread->t             : false;
#elif defined(USE_PS2_THREADS)
   return thread ? GetThreadId() == thread->id               : false;
#else
   return thread ? pthread_equal(pthread_self(), thread->id) : false;
#endif
}

slock_t *slock_new(void)
{
   slock_t      *lock = (slock_t*)calloc(1, sizeof(*lock));
   if (!lock)
      return NULL;
#if defined(USE_WIN32_THREADS)
   InitializeCriticalSection(&lock->lock);
#elif defined(USE_GX_THREADS)
   if (LWP_MutexInit(&lock->lock, false) != 0)
   {
      free(lock);
      return NULL;
   }
#elif defined(USE_CTR_THREADS)
   LightLock_Init(&lock->lock);
#elif defined(USE_PSP_THREADS)
   if ((lock->lock = sceKernelCreateSema("rarch_lock", 0, 1, 1, NULL)) < 0)
   {
      free(lock);
      return NULL;
   }
#elif defined(USE_VITA_THREADS)
   if (sceKernelCreateLwMutex(&lock->lock, "rarch_lock", 0, 0, NULL) < 0)
   {
      free(lock);
      return NULL;
   }
#elif defined(USE_WIIU_THREADS)
   OSFastMutex_Init(&lock->lock, "rarch_lock");
#elif defined(USE_SWITCH_THREADS)
   mutexInit(&lock->lock);
#elif defined(USE_PS2_THREADS)
   {
      ee_sema_t sema;
      memset(&sema, 0, sizeof(sema));
      sema.init_count = 1;
      sema.max_count  = 1;
      if ((lock->lock = CreateSema(&sema)) < 0)
      {
         free(lock);
         return NULL;
      }
   }
#else
   if (pthread_mutex_init(&lock->lock, NULL) != 0)
   {
      free(lock);
      return NULL;
   }
#endif
   return lock;
}

void slock_free(slock_t *lock)
{
   if (!lock)
      return;

#if defined(USE_WIN32_THREADS)
   DeleteCriticalSection(&lock->lock);
#elif defined(USE_GX_THREADS)
   LWP_MutexDestroy(lock->lock);
#elif defined(USE_CTR_THREADS)
   /* nothing to destroy */
#elif defined(USE_PSP_THREADS)
   sceKernelDeleteSema(lock->lock);
#elif defined(USE_VITA_THREADS)
   sceKernelDeleteLwMutex(&lock->lock);
#elif defined(USE_WIIU_THREADS) || defined(USE_SWITCH_THREADS)
   /* nothing to destroy */
#elif defined(USE_PS2_THREADS)
   DeleteSema(lock->lock);
#else
   pthread_mutex_destroy(&lock->lock);
#endif
   free(lock);
}

void slock_lock(slock_t *lock)
{
   if (!lock)
      return;
#if defined(USE_WIN32_THREADS)
   EnterCriticalSection(&lock->lock);
#elif defined(USE_GX_THREADS)
   LWP_MutexLock(lock->lock);
#elif defined(USE_CTR_THREADS)
   LightLock_Lock(&lock->lock);
#elif defined(USE_PSP_THREADS)
   sceKernelWaitSema(lock->lock, 1, NULL);
#elif defined(USE_VITA_THREADS)
   sceKernelLockLwMutex(&lock->lock, 1, NULL);
#elif defined(USE_WIIU_THREADS)
   OSFastMutex_Lock(&lock->lock);
#elif defined(USE_SWITCH_THREADS)
   mutexLock(&lock->lock);
#elif defined(USE_PS2_THREADS)
   WaitSema(lock->lock);
#else
   pthread_mutex_lock(&lock->lock);
#endif
}

bool slock_try_lock(slock_t *lock)
{
#if defined(USE_WIN32_THREADS)
   return lock && TryEnterCriticalSection(&lock->lock);
#elif defined(USE_GX_THREADS)
   return lock && (LWP_MutexTryLock(lock->lock) == 0);
#elif defined(USE_CTR_THREADS)
   return lock && (LightLock_TryLock(&lock->lock) == 0);
#elif defined(USE_PSP_THREADS)
   return lock && (sceKernelPollSema(lock->lock, 1) == 0);
#elif defined(USE_VITA_THREADS)
   return lock && (sceKernelTryLockLwMutex(&lock->lock, 1) == 0);
#elif defined(USE_WIIU_THREADS)
   return lock && OSFastMutex_TryLock(&lock->lock);
#elif defined(USE_SWITCH_THREADS)
   return lock && mutexTryLock(&lock->lock);
#elif defined(USE_PS2_THREADS)
   return lock && (PollSema(lock->lock) >= 0);
#else
   return lock && (pthread_mutex_trylock(&lock->lock) == 0);
#endif
}

void slock_unlock(slock_t *lock)
{
   if (!lock)
      return;
#if defined(USE_WIN32_THREADS)
   LeaveCriticalSection(&lock->lock);
#elif defined(USE_GX_THREADS)
   LWP_MutexUnlock(lock->lock);
#elif defined(USE_CTR_THREADS)
   LightLock_Unlock(&lock->lock);
#elif defined(USE_PSP_THREADS)
   sceKernelSignalSema(lock->lock, 1);
#elif defined(USE_VITA_THREADS)
   sceKernelUnlockLwMutex(&lock->lock, 1);
#elif defined(USE_WIIU_THREADS)
   OSFastMutex_Unlock(&lock->lock);
#elif defined(USE_SWITCH_THREADS)
   mutexUnlock(&lock->lock);
#elif defined(USE_PS2_THREADS)
   SignalSema(lock->lock);
#else
   pthread_mutex_unlock(&lock->lock);
#endif
}

#ifdef USE_VITA_THREADS
/* A LwCond is created bound to one LwMutex, and scond only learns its
 * lock at the first wait, so binding happens there. The caller holds
 * the lock at that point, so concurrent first waits are serialized by
 * the lock itself; a signal that still observes the condvar as
 * unbound corresponds to a moment with no blocked waiter, where
 * dropping the signal is what a condvar does anyway. Waiting on one
 * condvar with two different locks is as undefined here as it is for
 * pthreads. */
static void scond_bind_vita(scond_t *cond, slock_t *lock)
{
   if (!retro_atomic_load_acquire_int(&cond->bound))
   {
      cond->assoc = &lock->lock;
      sceKernelCreateLwCond(&cond->work, "rarch_cond", 0,
            &lock->lock, NULL);
      retro_atomic_store_release_int(&cond->bound, 1);
   }
#ifdef DEBUG
   retro_assert(cond->assoc == &lock->lock);
#endif
}
#endif

scond_t *scond_new(void)
{
   scond_t      *cond = (scond_t*)calloc(1, sizeof(*cond));

   if (!cond)
      return NULL;

#if defined(USE_WIN32_THREADS)
   scond_global_init();
   retro_atomic_ptr_init(&cond->head, NULL);
#elif defined(USE_GX_THREADS)
   if (LWP_CondInit(&cond->cond) != 0)
   {
      free(cond);
      return NULL;
   }
#elif defined(USE_CTR_THREADS)
   CondVar_Init(&cond->cond);
#elif defined(USE_PSP_THREADS)
   cond->waiters = 0;
   cond->gate    = sceKernelCreateSema("rarch_cond_gate", 0, 1, 1, NULL);
   cond->sema    = sceKernelCreateSema("rarch_cond", 0, 0, 0x7FFFFFFF, NULL);
   if (cond->gate < 0 || cond->sema < 0)
   {
      if (cond->gate >= 0)
         sceKernelDeleteSema(cond->gate);
      if (cond->sema >= 0)
         sceKernelDeleteSema(cond->sema);
      free(cond);
      return NULL;
   }
#elif defined(USE_VITA_THREADS)
   cond->assoc = NULL;
   retro_atomic_int_init(&cond->bound, 0);
#elif defined(USE_WIIU_THREADS)
   OSFastMutex_Init(&cond->gate, "rarch_cond");
#elif defined(USE_SWITCH_THREADS)
   condvarInit(&cond->cond);
#elif defined(USE_PS2_THREADS)
   /* head and tail start NULL from calloc */
#elif defined(RTHREADS_FUTEX_SCOND)
   retro_atomic_int_init(&cond->seq, 0);
   retro_atomic_int_init(&cond->waiters, 0);
#else
   if (pthread_cond_init(&cond->cond, NULL) != 0)
   {
      free(cond);
      return NULL;
   }
#endif

   return cond;
}

void scond_free(scond_t *cond)
{
   if (!cond)
      return;

#if defined(USE_WIN32_THREADS)
   /* nothing is owned: the waiter blocks live on their threads' stacks */
#elif defined(USE_GX_THREADS)
   LWP_CondDestroy(cond->cond);
#elif defined(USE_CTR_THREADS)
   /* nothing to destroy */
#elif defined(USE_PSP_THREADS)
   sceKernelDeleteSema(cond->sema);
   sceKernelDeleteSema(cond->gate);
#elif defined(USE_VITA_THREADS)
   if (retro_atomic_load_acquire_int(&cond->bound))
      sceKernelDeleteLwCond(&cond->work);
#elif defined(USE_WIIU_THREADS)
   /* nothing to destroy: waiter nodes live on their threads' stacks */
#elif defined(USE_SWITCH_THREADS) || defined(USE_PS2_THREADS) \
      || defined(RTHREADS_FUTEX_SCOND)
   /* nothing to destroy */
#else
   pthread_cond_destroy(&cond->cond);
#endif
   free(cond);
}

#ifdef USE_WIN32_THREADS

/* ---- processor probes ---------------------------------------------- */

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64)) && _MSC_VER >= 1500
#include <intrin.h>
#define SCOND_HAVE_X86_INTRIN 1
#if _MSC_VER >= 1912
#include <immintrin.h>
#define SCOND_HAVE_MWAITX 1
#endif
#if _MSC_VER >= 1924
#define SCOND_HAVE_UMWAIT 1
#endif
#elif defined(__GNUC__) && defined(__x86_64__)
#define SCOND_HAVE_X86_ASM 1
#define SCOND_HAVE_MWAITX 1
#define SCOND_HAVE_UMWAIT 1
#endif

#if defined(SCOND_HAVE_X86_INTRIN) || defined(SCOND_HAVE_X86_ASM)
#define SCOND_HAVE_X86 1
#endif

#if defined(SCOND_HAVE_X86)
static void scond_cpuid(unsigned leaf, unsigned sub, unsigned r[4])
{
#if defined(SCOND_HAVE_X86_INTRIN)
   int v[4];
   __cpuidex(v, (int)leaf, (int)sub);
   r[0] = (unsigned)v[0]; r[1] = (unsigned)v[1];
   r[2] = (unsigned)v[2]; r[3] = (unsigned)v[3];
#else
   __asm__ __volatile__("cpuid"
         : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3])
         : "a"(leaf), "c"(sub));
#endif
}

static INLINE unsigned __int64 scond_tsc(void)
{
#if defined(SCOND_HAVE_X86_INTRIN)
   return __rdtsc();
#else
   unsigned lo, hi;
   __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
   return ((unsigned __int64)hi << 32) | lo;
#endif
}

static INLINE void scond_pause(void)
{
#if defined(SCOND_HAVE_X86_INTRIN)
   _mm_pause();
#else
   __asm__ __volatile__("pause" ::: "memory");
#endif
}

#if defined(SCOND_HAVE_MWAITX)
/* AMD: park on the line holding *addr until it is written or ticks
 * TSC cycles pass (ECX bit 1 enables the timer) */
static INLINE void scond_monitorx(const void *addr)
{
#if defined(SCOND_HAVE_X86_INTRIN)
   _mm_monitorx((void*)addr, 0, 0);
#else
   __asm__ __volatile__(".byte 0x0f, 0x01, 0xfa"      /* monitorx */
         :: "a"(addr), "c"(0), "d"(0) : "memory");
#endif
}

static INLINE void scond_mwaitx(unsigned ticks)
{
#if defined(SCOND_HAVE_X86_INTRIN)
   _mm_mwaitx(0, 2, ticks);
#else
   __asm__ __volatile__(".byte 0x0f, 0x01, 0xfb"      /* mwaitx */
         :: "a"(0), "b"(ticks), "c"(2) : "memory");
#endif
}
#endif

#if defined(SCOND_HAVE_UMWAIT)
/* Intel WAITPKG: park on the line until written or the absolute TSC
 * deadline; control 1 asks for the lighter C0.1 state */
static INLINE void scond_umonitor(const void *addr)
{
#if defined(SCOND_HAVE_X86_INTRIN)
   _umonitor((void*)addr);
#else
   __asm__ __volatile__(".byte 0xf3, 0x0f, 0xae, 0xf7" /* umonitor rdi */
         :: "D"(addr) : "memory");
#endif
}

static INLINE void scond_umwait(unsigned __int64 deadline)
{
#if defined(SCOND_HAVE_X86_INTRIN)
   _umwait(1, deadline);
#else
   __asm__ __volatile__(".byte 0xf2, 0x0f, 0xae, 0xf1" /* umwait ecx */
         :: "c"(1), "a"((unsigned)deadline), "d"((unsigned)(deadline >> 32))
         : "memory", "cc");
#endif
}
#endif
#endif /* SCOND_HAVE_X86 */

/* the spin polls the waiter's own flag word: a plain load on x86 (the
 * word is written by one waker with a locked op), an acquire elsewhere.
 *
 * The plain load has to be spelled per backend rather than by casting the
 * field.  retro_atomic_int_t is a real atomic type on the C11, C++11 and
 * __atomic backends -- mingw picks C11, so the field is an atomic_int --
 * and reading one through a volatile LONG lvalue is a type pun that both
 * breaks strict aliasing and is undefined besides.  Those three backends
 * go through the API instead, which costs nothing here: their acquire
 * load is a plain MOV on x86.  The MSVC, Apple, __sync and volatile
 * backends type the field as a plain volatile integer, so it can just be
 * read -- same type, no cast -- and that matters for them, because their
 * acquire load is a locked RMW (InterlockedCompareExchangeAcquire,
 * OSAtomicAdd32Barrier, __sync_fetch_and_add(p, 0)), far too heavy to run
 * every iteration against a line one waker writes. */
#if defined(SCOND_HAVE_X86) && (defined(RETRO_ATOMIC_BACKEND_MSVC)  \
      || defined(RETRO_ATOMIC_BACKEND_APPLE)                        \
      || defined(RETRO_ATOMIC_BACKEND_SYNC)                         \
      || defined(RETRO_ATOMIC_BACKEND_VOLATILE))
#define scond_flags_peek(w) ((w)->flags)
#else
#define scond_flags_peek(w) retro_atomic_load_acquire_int(&(w)->flags)
#endif

/* ---- once-per-process setup ---------------------------------------- */

/* TSC cycles a waiter spins on its flag word before blocking. Measured
 * on a 9950X3D at 600 dispatches a frame: below about 32k the spin no
 * longer bridges the interval between dispatches and every one of them
 * pays a kernel wait, while above 64k nothing is gained at four or
 * eight threads and an oversubscribed pool loses steadily, its spinners
 * taking cycles from the sibling threads doing the work. ntdll spins
 * 0x90b40, an order of magnitude longer, which suits a general-purpose
 * wait rather than one dispatch arriving every few microseconds. */
#define SCOND_DEFAULT_SPIN_CYCLES 64000u

static void scond_global_resolve(void)
{
#if !defined(_XBOX)
   SYSTEM_INFO si;
#endif
   const char *env;
   unsigned cycles = SCOND_DEFAULT_SPIN_CYCLES;

   scond_g.sleep = SCOND_SLEEP_EVENT;
#if !defined(_XBOX)
   {
      HMODULE nt = GetModuleHandleA("ntdll.dll");
      if (nt)
      {
         scond_nt_create_keyed_t create_keyed;
         scond_g.wait_alert    = (scond_nt_wait_alert_t)(void (*)(void))
            GetProcAddress(nt, "NtWaitForAlertByThreadId");
         scond_g.alert_tid     = (scond_nt_alert_tid_t)(void (*)(void))
            GetProcAddress(nt, "NtAlertThreadByThreadId");
         scond_g.wait_keyed    = (scond_nt_keyed_t)(void (*)(void))
            GetProcAddress(nt, "NtWaitForKeyedEvent");
         scond_g.release_keyed = (scond_nt_keyed_t)(void (*)(void))
            GetProcAddress(nt, "NtReleaseKeyedEvent");
         create_keyed          = (scond_nt_create_keyed_t)(void (*)(void))
            GetProcAddress(nt, "NtCreateKeyedEvent");
         env = getenv("RTHREADS_SCOND");
         if (scond_g.wait_alert && scond_g.alert_tid
               && !(env && strcmp(env, "alert")))
            scond_g.sleep = SCOND_SLEEP_ALERT;
         else if (scond_g.wait_keyed && scond_g.release_keyed && create_keyed
               && !(env && strcmp(env, "keyed"))
               && create_keyed(&scond_g.keyed, 0x1f0003 /* EVENT_ALL_ACCESS */, NULL, 0) == 0)
            scond_g.sleep = SCOND_SLEEP_KEYED;
      }
   }
#endif
   if (scond_g.sleep == SCOND_SLEEP_EVENT)
      scond_g.tls_event = TlsAlloc();

#if defined(_XBOX)
   /* the 360 has six hardware threads, the original Xbox one core */
#if defined(_M_PPC) || defined(_XENON)
   scond_g.spin = SCOND_SPIN_PAUSE;
#else
   scond_g.spin = SCOND_SPIN_NONE;
#endif
#else
#if defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
   GetNativeSystemInfo(&si);
#else
   GetSystemInfo(&si);
#endif
   scond_g.spin = si.dwNumberOfProcessors > 1 ? SCOND_SPIN_PAUSE : SCOND_SPIN_NONE;
#endif
   env = getenv("RTHREADS_SCOND_SPIN");
   if (env)
   {
      if (!strcmp(env, "none"))
         scond_g.spin = SCOND_SPIN_NONE;
      else if (!strcmp(env, "pause"))
         scond_g.spin = SCOND_SPIN_PAUSE;
#if defined(SCOND_HAVE_X86)
      else if (!strcmp(env, "mwaitx") || !strcmp(env, "umwait"))
      {
         /* only where the processor has it */
         unsigned r[4];
         scond_cpuid(0, 0, r);
#if defined(SCOND_HAVE_UMWAIT)
         if (env[0] == 'u' && r[0] >= 7)
         {
            scond_cpuid(7, 0, r);
            if (r[2] & (1u << 5))
               scond_g.spin = SCOND_SPIN_UMWAIT;
         }
#endif
#if defined(SCOND_HAVE_MWAITX)
         if (env[0] == 'm')
         {
            scond_cpuid(0x80000000u, 0, r);
            if (r[0] >= 0x80000001u)
            {
               scond_cpuid(0x80000001u, 0, r);
               if (r[2] & (1u << 29))
                  scond_g.spin = SCOND_SPIN_MWAITX;
            }
         }
#endif
      }
#endif
   }
   env = getenv("RTHREADS_SCOND_SPIN_CYCLES");
   if (env && env[0] >= '0' && env[0] <= '9')
      cycles = (unsigned)strtoul(env, NULL, 0);
   scond_g.spin_cycles = cycles;
   /* a pause is on the order of a hundred cycles */
   scond_g.spin_iters  = cycles / 128u;
   if (!scond_g.spin_iters)
      scond_g.spin_iters = 1;
}

static void scond_global_init(void)
{
   if (retro_atomic_load_acquire_int(&scond_g.state) == 2)
      return;
   if (retro_atomic_cas_int(&scond_g.state, 0, 1))
   {
      scond_global_resolve();
      retro_atomic_store_release_int(&scond_g.state, 2);
      return;
   }
   while (retro_atomic_load_acquire_int(&scond_g.state) != 2)
      Sleep(0);
}

/* ---- kernel wait and wake ------------------------------------------ */

#define SCOND_STATUS_TIMEOUT 0x102

/* block until woken or the timeout (NULL = never) passes; false on timeout */
static bool scond_sleep(struct scond_waiter *w, LARGE_INTEGER *timeout)
{
   switch (scond_g.sleep)
   {
      case SCOND_SLEEP_ALERT:
         return scond_g.wait_alert(&w->flags, timeout) != SCOND_STATUS_TIMEOUT;
      case SCOND_SLEEP_KEYED:
         return scond_g.wait_keyed(scond_g.keyed, w, FALSE, timeout)
            != SCOND_STATUS_TIMEOUT;
      default:
         {
            DWORD ms = timeout
               ? (DWORD)((-timeout->QuadPart + 9999) / 10000) : INFINITE;
            return WaitForSingleObject(w->event, ms) != WAIT_TIMEOUT;
         }
   }
}

static void scond_wake_one(struct scond_waiter *w)
{
   /* copies taken first: the waiter may leave as soon as it sees WOKEN */
   DWORD  tid   = w->tid;
   HANDLE event = w->event;
   int prev     = retro_atomic_fetch_or_int(&w->flags, SCOND_W_WOKEN);
   if (!(prev & SCOND_W_ASLEEP))
      return;   /* still spinning: it sees the flag, no syscall */
   switch (scond_g.sleep)
   {
      case SCOND_SLEEP_ALERT:
         scond_g.alert_tid((HANDLE)(uintptr_t)tid);
         break;
      case SCOND_SLEEP_KEYED:
         scond_g.release_keyed(scond_g.keyed, w, FALSE, NULL);
         break;
      default:
         SetEvent(event);
         break;
   }
}

/* ---- the list ------------------------------------------------------ */

static INLINE uintptr_t scond_head(scond_t *cond)
{
   return (uintptr_t)retro_atomic_load_acquire_ptr(&cond->head);
}

static void scond_lock(scond_t *cond)
{
   for (;;)
   {
      uintptr_t old = scond_head(cond);
      if (!(old & SCOND_HEAD_LOCK)
            && retro_atomic_cas_ptr(&cond->head, (void*)old,
               (void*)(old | SCOND_HEAD_LOCK)))
         return;
#if defined(SCOND_HAVE_X86)
      scond_pause();
#else
      Sleep(0);
#endif
   }
}

static void scond_unlock(scond_t *cond)
{
   for (;;)
   {
      uintptr_t old = scond_head(cond);
      if (retro_atomic_cas_ptr(&cond->head, (void*)old,
               (void*)(old & SCOND_HEAD_MASK)))
         return;
   }
}

/* unlink w if it is still on the list; the lock must be held. A block
 * that is not there any more has been taken by a waker. */
static bool scond_unlink(scond_t *cond, struct scond_waiter *w)
{
   for (;;)
   {
      uintptr_t old = scond_head(cond);
      struct scond_waiter *n = (struct scond_waiter*)(old & SCOND_HEAD_MASK);
      if (n == w)
      {
         if (retro_atomic_cas_ptr(&cond->head, (void*)old,
                  (void*)((uintptr_t)w->next | SCOND_HEAD_LOCK)))
            return true;
         continue;   /* a push landed in front of it: look again */
      }
      while (n && n->next != w)
         n = n->next;
      if (!n)
         return false;
      n->next = w->next;
      return true;
   }
}

/* Block on the caller's own flag word until signalled or dwMilliseconds
 * have passed. Returns false only on timeout. */
static bool scond_wait_win32(scond_t *cond, slock_t *lock, DWORD dwMilliseconds)
{
   struct scond_waiter w;
   LARGE_INTEGER timeout;
   uintptr_t old;
   bool woken = true;

   w.event = NULL;
   w.tid   = GetCurrentThreadId();
   retro_atomic_int_init(&w.flags, 0);
   if (scond_g.sleep == SCOND_SLEEP_EVENT)
   {
      w.event = (HANDLE)TlsGetValue(scond_g.tls_event);
      if (!w.event)
      {
         /* one auto-reset event per thread, kept for the thread's life */
         w.event = CreateEvent(NULL, FALSE, FALSE, NULL);
         if (!w.event)
            return true;   /* nothing to wait on: a spurious wake-up */
         TlsSetValue(scond_g.tls_event, w.event);
      }
   }

   /* push, keeping the lock bit as it is */
   do
   {
      old    = scond_head(cond);
      w.next = (struct scond_waiter*)(old & SCOND_HEAD_MASK);
   } while (!retro_atomic_cas_ptr(&cond->head, (void*)old,
            (void*)((uintptr_t)&w | (old & SCOND_HEAD_LOCK))));

   LeaveCriticalSection(&lock->lock);

   /* spin on the flag word before committing to the kernel */
   switch (scond_g.spin)
   {
#if defined(SCOND_HAVE_X86)
#if defined(SCOND_HAVE_MWAITX)
      case SCOND_SPIN_MWAITX:
         {
            unsigned __int64 deadline = scond_tsc() + scond_g.spin_cycles;
            for (;;)
            {
               unsigned __int64 now;
               /* arm first, then check: a write between the check and
                * the wait is then seen by the wait itself */
               scond_monitorx(&w.flags);
               if (scond_flags_peek(&w) & SCOND_W_WOKEN)
                  goto done;
               now = scond_tsc();
               if (now >= deadline)
                  break;
               scond_mwaitx((unsigned)(deadline - now));
            }
         }
         break;
#endif
#if defined(SCOND_HAVE_UMWAIT)
      case SCOND_SPIN_UMWAIT:
         {
            unsigned __int64 deadline = scond_tsc() + scond_g.spin_cycles;
            for (;;)
            {
               scond_umonitor(&w.flags);
               if (scond_flags_peek(&w) & SCOND_W_WOKEN)
                  goto done;
               if (scond_tsc() >= deadline)
                  break;
               scond_umwait(deadline);
            }
         }
         break;
#endif
#endif
      case SCOND_SPIN_PAUSE:
         {
            unsigned i;
            for (i = 0; i < scond_g.spin_iters; i++)
            {
               if (scond_flags_peek(&w) & SCOND_W_WOKEN)
                  goto done;
#if defined(SCOND_HAVE_X86)
               scond_pause();
#else
               YieldProcessor();
#endif
            }
         }
         break;
      default:
         break;
   }

   /* commit: after this a waker that takes the block must wake us */
   if (retro_atomic_fetch_or_int(&w.flags, SCOND_W_ASLEEP) & SCOND_W_WOKEN)
      goto done;

   if (dwMilliseconds != INFINITE)
      timeout.QuadPart = -(LONGLONG)dwMilliseconds * 10000;
   if (!scond_sleep(&w, dwMilliseconds != INFINITE ? &timeout : NULL))
   {
      /* timed out: unless a waker has already taken the block, in which
       * case its wake is on the way and has to be consumed */
      scond_lock(cond);
      woken = !scond_unlink(cond, &w);
      scond_unlock(cond);
      if (woken)
         scond_sleep(&w, NULL);
   }

done:
   EnterCriticalSection(&lock->lock);
   return woken;
}
#endif

void scond_wait(scond_t *cond, slock_t *lock)
{
#if defined(USE_WIN32_THREADS)
   scond_wait_win32(cond, lock, INFINITE);
#elif defined(USE_GX_THREADS)
   LWP_CondWait(cond->cond, lock->lock);
#elif defined(USE_CTR_THREADS)
   CondVar_Wait(&cond->cond, &lock->lock);
#elif defined(USE_PSP_THREADS)
   sceKernelWaitSema(cond->gate, 1, NULL);
   cond->waiters++;
   sceKernelSignalSema(cond->gate, 1);
   slock_unlock(lock);
   sceKernelWaitSema(cond->sema, 1, NULL);
   slock_lock(lock);
#elif defined(USE_VITA_THREADS)
   scond_bind_vita(cond, lock);
   sceKernelWaitLwCond(&cond->work, NULL);
#elif defined(USE_WIIU_THREADS)
   struct wiiu_cond_waiter w;
   w.next = NULL;
   OSInitEvent(&w.ev, FALSE, OS_EVENT_MODE_AUTO);
   OSFastMutex_Lock(&cond->gate);
   if (cond->tail)
      cond->tail->next = &w;
   else
      cond->head       = &w;
   cond->tail          = &w;
   OSFastMutex_Unlock(&cond->gate);
   slock_unlock(lock);
   OSWaitEvent(&w.ev);
   slock_lock(lock);
#elif defined(USE_SWITCH_THREADS)
   condvarWait(&cond->cond, &lock->lock);
#elif defined(USE_PS2_THREADS)
   struct ps2_cond_waiter w;
   ps2_cond_enqueue(cond, &w);
   slock_unlock(lock);
   SleepThread();
   slock_lock(lock);
#elif defined(RTHREADS_FUTEX_SCOND)
   /* Registering and sampling seq happen under the caller's lock, so
    * a signal issued once we let go either bumps seq before the futex
    * looks at it (immediate return) or wakes the sleep. */
   int seq = retro_atomic_load_acquire_int(&cond->seq);
   retro_atomic_fetch_add_int(&cond->waiters, 1);
   slock_unlock(lock);
   syscall(__NR_futex, &cond->seq, RTHREADS_FUTEX_WAIT_BITSET_PRIVATE,
         seq, NULL, NULL, RTHREADS_FUTEX_MATCH_ANY);
   retro_atomic_fetch_add_int(&cond->waiters, -1);
   slock_lock(lock);
#else
   pthread_cond_wait(&cond->cond, &lock->lock);
#endif
}

int scond_broadcast(scond_t *cond)
{
#if defined(USE_WIN32_THREADS)
   struct scond_waiter *w;
   uintptr_t old;
   if (!(scond_head(cond) & SCOND_HEAD_MASK))
      return 0;
   scond_lock(cond);
   /* take the whole list, dropping the lock in the same swap */
   do
   {
      old = scond_head(cond);
   } while (!retro_atomic_cas_ptr(&cond->head, (void*)old, NULL));
   w = (struct scond_waiter*)(old & SCOND_HEAD_MASK);
   while (w)
   {
      struct scond_waiter *next = w->next;
      scond_wake_one(w);
      w = next;
   }
   return 0;
#elif defined(USE_GX_THREADS)
   return LWP_CondBroadcast(cond->cond);
#elif defined(USE_CTR_THREADS)
   CondVar_Broadcast(&cond->cond);
   return 0;
#elif defined(USE_PSP_THREADS)
   int n;
   sceKernelWaitSema(cond->gate, 1, NULL);
   n             = cond->waiters;
   cond->waiters = 0;
   if (n > 0)
      sceKernelSignalSema(cond->sema, n);
   sceKernelSignalSema(cond->gate, 1);
   return 0;
#elif defined(USE_VITA_THREADS)
   if (retro_atomic_load_acquire_int(&cond->bound))
      sceKernelSignalLwCondAll(&cond->work);
   return 0;
#elif defined(USE_WIIU_THREADS)
   struct wiiu_cond_waiter *w, *next;
   OSFastMutex_Lock(&cond->gate);
   w          = cond->head;
   cond->head = NULL;
   cond->tail = NULL;
   OSFastMutex_Unlock(&cond->gate);
   while (w)
   {
      /* The node vanishes with its waiter once signaled, so step off
       * it first. */
      next = w->next;
      OSSignalEvent(&w->ev);
      w    = next;
   }
   return 0;
#elif defined(USE_SWITCH_THREADS)
   condvarWakeAll(&cond->cond);
   return 0;
#elif defined(USE_PS2_THREADS)
   struct ps2_cond_waiter *w;
   DIntr();
   w          = cond->head;
   cond->head = NULL;
   cond->tail = NULL;
   EIntr();
   while (w)
   {
      /* The node vanishes with its waiter once woken; take the id
       * and the link first. */
      struct ps2_cond_waiter *next = w->next;
      s32 tid                      = w->tid;
      w                            = next;
      WakeupThread(tid);
   }
   return 0;
#elif defined(RTHREADS_FUTEX_SCOND)
   if (retro_atomic_load_acquire_int(&cond->waiters) > 0)
   {
      retro_atomic_fetch_add_int(&cond->seq, 1);
      syscall(__NR_futex, &cond->seq, RTHREADS_FUTEX_WAKE_PRIVATE,
            0x7fffffff, NULL, NULL, 0);
   }
   return 0;
#else
   return pthread_cond_broadcast(&cond->cond);
#endif
}

void scond_signal(scond_t *cond)
{
#if defined(USE_WIN32_THREADS)
   struct scond_waiter *w;
   if (!(scond_head(cond) & SCOND_HEAD_MASK))
      return;
   scond_lock(cond);
   /* the oldest waiter is at the end: pushes go on the front */
   for (;;)
   {
      uintptr_t old = scond_head(cond);
      w = (struct scond_waiter*)(old & SCOND_HEAD_MASK);
      if (!w)
         break;
      if (!w->next)
      {
         if (retro_atomic_cas_ptr(&cond->head, (void*)old,
                  (void*)SCOND_HEAD_LOCK))
            break;
         continue;   /* a push landed in front: look again */
      }
      {
         struct scond_waiter *prev = w;
         while (prev->next->next)
            prev = prev->next;
         w = prev->next;
         prev->next = NULL;
      }
      break;
   }
   scond_unlock(cond);
   if (w)
      scond_wake_one(w);
#elif defined(USE_GX_THREADS)
   LWP_CondSignal(cond->cond);
#elif defined(USE_CTR_THREADS)
   CondVar_Signal(&cond->cond);
#elif defined(USE_PSP_THREADS)
   sceKernelWaitSema(cond->gate, 1, NULL);
   if (cond->waiters > 0)
   {
      cond->waiters--;
      sceKernelSignalSema(cond->sema, 1);
   }
   sceKernelSignalSema(cond->gate, 1);
#elif defined(USE_VITA_THREADS)
   if (retro_atomic_load_acquire_int(&cond->bound))
      sceKernelSignalLwCond(&cond->work);
#elif defined(USE_WIIU_THREADS)
   struct wiiu_cond_waiter *w;
   OSFastMutex_Lock(&cond->gate);
   w = cond->head;
   if (w)
   {
      cond->head = w->next;
      if (!cond->head)
         cond->tail = NULL;
   }
   OSFastMutex_Unlock(&cond->gate);
   if (w)
      OSSignalEvent(&w->ev);
#elif defined(USE_SWITCH_THREADS)
   condvarWakeOne(&cond->cond);
#elif defined(USE_PS2_THREADS)
   struct ps2_cond_waiter *w;
   s32 tid = -1;
   DIntr();
   w = cond->head;
   if (w)
   {
      tid        = w->tid;
      cond->head = w->next;
      if (!cond->head)
         cond->tail = NULL;
   }
   EIntr();
   if (tid >= 0)
      WakeupThread(tid);
#elif defined(RTHREADS_FUTEX_SCOND)
   if (retro_atomic_load_acquire_int(&cond->waiters) > 0)
   {
      retro_atomic_fetch_add_int(&cond->seq, 1);
      syscall(__NR_futex, &cond->seq, RTHREADS_FUTEX_WAKE_PRIVATE,
            1, NULL, NULL, 0);
   }
#else
   pthread_cond_signal(&cond->cond);
#endif
}


bool scond_wait_timeout(scond_t *cond, slock_t *lock, int64_t timeout_us)
{
#if defined(USE_WIN32_THREADS)
   /* How to convert a microsecond (us) timeout to millisecond (ms)?
    *
    * Someone asking for a 0 timeout clearly wants immediate timeout.
    * Someone asking for a 1 timeout clearly wants an actual timeout
    * of the minimum length */
   /* The implementation of a 0 timeout here with pthreads is sketchy.
    * It isn't clear what happens if pthread_cond_timedwait is called with NOW.
    * Moreover, it is possible that this thread gets preempted after the
    * clock_gettime but before the pthread_cond_timedwait.
    * In order to help smoke out problems caused by this strange usage,
    * let's treat a 0 timeout as always timing out.
    */
   if (timeout_us == 0)
      return false;
   else if (timeout_us < 1000)
      return scond_wait_win32(cond, lock, 1);
   /* Someone asking for 1000 or 1001 timeout shouldn't
    * accidentally get 2ms. */
   return scond_wait_win32(cond, lock, timeout_us / 1000);
#elif defined(USE_GX_THREADS)
#ifdef INTERNAL_LIBOGC
   /* The in-tree libogc takes an absolute deadline and compares it
    * against its RTC-based single-argument clock_gettime, so the
    * deadline has to come from that same clock. Its prototype clashes
    * with newlib's POSIX clock_gettime, hence the asm binding. A zero
    * timeout is treated as always timing out, as on Win32. */
   struct timespec dl;
   if (timeout_us <= 0)
      return false;
   {
      extern int ogc_rtc_gettime(struct timespec *tp)
            __asm__("clock_gettime");
      if (ogc_rtc_gettime(&dl) != 0)
         return false;
   }
   dl.tv_sec  += (time_t)(timeout_us / INT64_C(1000000));
   dl.tv_nsec += (long)(timeout_us % INT64_C(1000000)) * 1000L;
   if (dl.tv_nsec >= 1000000000L)
   {
      dl.tv_sec  += 1;
      dl.tv_nsec -= 1000000000L;
   }
   return LWP_CondTimedWait(cond->cond, lock->lock, &dl) == 0;
#else
   /* Upstream libogc takes the timeout as a relative timespec; a zero
    * timeout is treated as always timing out, as on Win32. */
   struct timespec rel;
   if (timeout_us <= 0)
      return false;
   rel.tv_sec  = (time_t)(timeout_us / INT64_C(1000000));
   rel.tv_nsec = (long)(timeout_us % INT64_C(1000000)) * 1000L;
   return LWP_CondTimedWait(cond->cond, lock->lock, &rel) == 0;
#endif
#elif defined(USE_CTR_THREADS)
   if (timeout_us <= 0)
      return false;
   return CondVar_WaitTimeout(&cond->cond, &lock->lock,
         timeout_us * INT64_C(1000)) == 0;
#elif defined(USE_PSP_THREADS)
   bool woken;
   SceUInt to;
   if (timeout_us <= 0)
      return false;
   to = (timeout_us > INT64_C(0xFFFFFFFF))
         ? 0xFFFFFFFFu : (SceUInt)timeout_us;
   sceKernelWaitSema(cond->gate, 1, NULL);
   cond->waiters++;
   sceKernelSignalSema(cond->gate, 1);
   slock_unlock(lock);
   woken = sceKernelWaitSema(cond->sema, 1, &to) == 0;
   if (!woken)
   {
      /* Timed out: withdraw the registration, unless a wake credit
       * arrived in the meantime, in which case consume it and report
       * the wake. */
      sceKernelWaitSema(cond->gate, 1, NULL);
      if (sceKernelPollSema(cond->sema, 1) == 0)
         woken = true;
      else
         cond->waiters--;
      sceKernelSignalSema(cond->gate, 1);
   }
   slock_lock(lock);
   return woken;
#elif defined(USE_VITA_THREADS)
   unsigned int to;
   if (timeout_us <= 0)
      return false;
   scond_bind_vita(cond, lock);
   to = (timeout_us > INT64_C(0xFFFFFFFF))
         ? 0xFFFFFFFFu : (unsigned int)timeout_us;
   return sceKernelWaitLwCond(&cond->work, &to) == 0;
#elif defined(USE_WIIU_THREADS)
   bool woken;
   struct wiiu_cond_waiter w;
   if (timeout_us <= 0)
      return false;
   w.next = NULL;
   OSInitEvent(&w.ev, FALSE, OS_EVENT_MODE_AUTO);
   OSFastMutex_Lock(&cond->gate);
   if (cond->tail)
      cond->tail->next = &w;
   else
      cond->head       = &w;
   cond->tail          = &w;
   OSFastMutex_Unlock(&cond->gate);
   slock_unlock(lock);
   woken = OSWaitEventWithTimeout(&w.ev,
         (OSTime)OSMicroseconds(timeout_us));
   if (!woken)
   {
      /* Timed out: withdraw the registration, unless a signaler
       * already dequeued this waiter - then its wake is in flight, so
       * consume it before the node on this stack goes away, and
       * report the wake. */
      struct wiiu_cond_waiter *cur, *prev = NULL;
      OSFastMutex_Lock(&cond->gate);
      cur = cond->head;
      while (cur && cur != &w)
      {
         prev = cur;
         cur  = cur->next;
      }
      if (cur)
      {
         if (prev)
            prev->next = w.next;
         else
            cond->head = w.next;
         if (cond->tail == &w)
            cond->tail = prev;
      }
      OSFastMutex_Unlock(&cond->gate);
      if (!cur)
      {
         OSWaitEvent(&w.ev);
         woken = true;
      }
   }
   slock_lock(lock);
   return woken;
#elif defined(USE_SWITCH_THREADS)
   if (timeout_us <= 0)
      return false;
   return condvarWaitTimeout(&cond->cond, &lock->lock,
         (u64)timeout_us * 1000ull) == 0;
#elif defined(USE_PS2_THREADS)
   struct ps2_cond_waiter w;
   bool woken;
   s32 alarm;
   if (timeout_us <= 0)
      return false;
   ps2_cond_enqueue(cond, &w);
   slock_unlock(lock);
   alarm = SetTimerAlarm(GetTimerSystemTime()
         + TimerUSec2BusClock((u32)(timeout_us / INT64_C(1000000)),
                              (u32)(timeout_us % INT64_C(1000000))),
         ps2_cond_alarm, &w);
   if (alarm < 0)
   {
      /* No alarm slot free: report a timeout right away, unless a
       * signaler has already claimed this waiter, whose wake must
       * then be consumed. */
      if (ps2_cond_withdraw(cond, &w))
         woken = false;
      else
      {
         SleepThread();
         woken = true;
      }
      slock_lock(lock);
      return woken;
   }
   SleepThread();
   /* Which of the alarm and a signaler woke us. Releasing under
    * DIntr means the handler cannot slip in between the check and
    * the release. */
   DIntr();
   if (!w.fired)
      iReleaseTimerAlarm(alarm);
   EIntr();
   if (!w.fired)
      woken = true;                 /* a signaler dequeued this node */
   else if (ps2_cond_withdraw(cond, &w))
      woken = false;                /* nobody signaled: timed out */
   else
   {
      /* Both the alarm and a signaler fired, so one wakeup is still
       * banked against this thread; drain it before it can end a
       * later sleep early. */
      CancelWakeupThread(w.tid);
      woken = true;
   }
   slock_lock(lock);
   return woken;
#elif defined(RTHREADS_FUTEX_SCOND)
   /* FUTEX_WAIT_BITSET takes an absolute CLOCK_MONOTONIC deadline, so
    * a spurious wake never shortens or stretches the wait. */
   struct timespec dl;
   int seq, rc;
   if (timeout_us <= 0)
      return false;
   clock_gettime(CLOCK_MONOTONIC, &dl);
   dl.tv_sec  += (time_t)(timeout_us / INT64_C(1000000));
   dl.tv_nsec += (long)(timeout_us % INT64_C(1000000)) * 1000L;
   if (dl.tv_nsec >= 1000000000L)
   {
      dl.tv_sec  += 1;
      dl.tv_nsec -= 1000000000L;
   }
   seq = retro_atomic_load_acquire_int(&cond->seq);
   retro_atomic_fetch_add_int(&cond->waiters, 1);
   slock_unlock(lock);
   rc = (int)syscall(__NR_futex, &cond->seq,
         RTHREADS_FUTEX_WAIT_BITSET_PRIVATE, seq, &dl, NULL,
         RTHREADS_FUTEX_MATCH_ANY);
   retro_atomic_fetch_add_int(&cond->waiters, -1);
   slock_lock(lock);
   return !(rc < 0 && errno == ETIMEDOUT);
#else
   int64_t seconds, remainder;
   struct timespec now;
#if defined(__MACH__) && defined(__APPLE__) && !defined(RTHREADS_HAVE_CLOCK_GETTIME)
   mach_timespec_t mts;
#endif
#if defined(__MACH__) && defined(__APPLE__)
   /* CALENDAR_CLOCK is the Mach equivalent of CLOCK_REALTIME, which is what
    * pthread_cond_timedwait() below expects. */
#ifdef RTHREADS_HAVE_CLOCK_GETTIME
   clock_gettime(CLOCK_REALTIME, &now);
#else
   pthread_once(&rthreads_calendar_clock_once, rthreads_calendar_clock_init);
   clock_get_time(rthreads_calendar_clock, &mts);
   now.tv_sec  = mts.tv_sec;
   now.tv_nsec = mts.tv_nsec;
#endif
#elif !defined(__PSL1GHT__) && defined(__PS3__)
   sys_time_sec_t s;
   sys_time_nsec_t n;
   sys_time_get_current_time(&s, &n);
   now.tv_sec            = s;
   now.tv_nsec           = n;
#elif defined(RETRO_WIN32_USE_PTHREADS)
   _ftime64_s(&now);
#else
   clock_gettime(CLOCK_REALTIME, &now);
#endif

   seconds              = timeout_us / INT64_C(1000000);
   remainder            = timeout_us % INT64_C(1000000);

   now.tv_sec          += seconds;
   now.tv_nsec         += remainder * INT64_C(1000);

   if (now.tv_nsec >= 1000000000)
   {
      now.tv_nsec      -= 1000000000;
      now.tv_sec       += 1;
   }

   return (pthread_cond_timedwait(&cond->cond, &lock->lock, &now) == 0);
#endif
}

#ifdef HAVE_THREAD_STORAGE
bool sthread_tls_create(sthread_tls_t *tls)
{
#ifdef USE_WIN32_THREADS
   return (*tls = TlsAlloc()) != TLS_OUT_OF_INDEXES;
#else
   return pthread_key_create((pthread_key_t*)tls, NULL) == 0;
#endif
}

bool sthread_tls_create_with_dtor(sthread_tls_t *tls,
      void (*destructor)(void *value))
{
#ifdef USE_WIN32_THREADS
   /* TlsAlloc() provides no destructor callback; created without one. */
   (void)destructor;
   return (*tls = TlsAlloc()) != TLS_OUT_OF_INDEXES;
#else
   return pthread_key_create((pthread_key_t*)tls, destructor) == 0;
#endif
}

bool sthread_tls_delete(sthread_tls_t *tls)
{
#ifdef USE_WIN32_THREADS
   return TlsFree(*tls) != 0;
#else
   return pthread_key_delete(*tls) == 0;
#endif
}

void *sthread_tls_get(sthread_tls_t *tls)
{
#ifdef USE_WIN32_THREADS
   return TlsGetValue(*tls);
#else
   return pthread_getspecific(*tls);
#endif
}

bool sthread_tls_set(sthread_tls_t *tls, const void *data)
{
#ifdef USE_WIN32_THREADS
   return TlsSetValue(*tls, (void*)data) != 0;
#else
   return pthread_setspecific(*tls, data) == 0;
#endif
}
#endif

uintptr_t sthread_get_thread_id(sthread_t *thread)
{
   if (thread)
#ifdef USE_SWITCH_THREADS
      return (uintptr_t)&thread->t;
#else
      return (uintptr_t)thread->id;
#endif
   return 0;
}

uintptr_t sthread_get_current_thread_id(void)
{
#if defined(USE_WIN32_THREADS)
   return (uintptr_t)GetCurrentThreadId();
#elif defined(USE_GX_THREADS)
   return (uintptr_t)LWP_GetSelf();
#elif defined(USE_CTR_THREADS)
   return (uintptr_t)threadGetCurrent();
#elif defined(RTHREADS_SCE_START)
   return (uintptr_t)sceKernelGetThreadId();
#elif defined(USE_WIIU_THREADS)
   return (uintptr_t)OSGetCurrentThread();
#elif defined(USE_SWITCH_THREADS)
   return (uintptr_t)threadGetSelf();
#elif defined(USE_PS2_THREADS)
   return (uintptr_t)GetThreadId();
#else
   return (uintptr_t)pthread_self();
#endif
}

bool sthread_is_main_thread(void)
{
#if defined(__APPLE__)
   /* BSD/Darwin extension reporting whether the caller is the initial
    * thread. pthread.h is already included on this backend. */
   return pthread_main_np() != 0;
#else
   /* No native predicate on this backend; current callers are Apple-only.
    * See the header note for the portable captured-id alternative. */
   return false;
#endif
}

/* pthread_cancel / pthread_setcancelstate are POSIX but not universally
 * available: notably absent on Android/Bionic, and meaningless on the
 * non-pthread backends. Enable only where the backend provides them. */
#if !defined(USE_WIN32_THREADS) && !defined(USE_GX_THREADS) \
      && !defined(USE_CTR_THREADS) && !defined(USE_PSP_THREADS) \
      && !defined(USE_VITA_THREADS) && !defined(USE_WIIU_THREADS) \
      && !defined(USE_SWITCH_THREADS) && !defined(USE_PS2_THREADS) \
      && !defined(__ANDROID__)
#define RTHREADS_HAVE_CANCEL 1
#endif

void sthread_set_cancel_enable(bool enable)
{
#ifdef RTHREADS_HAVE_CANCEL
   pthread_setcancelstate(
         enable ? PTHREAD_CANCEL_ENABLE : PTHREAD_CANCEL_DISABLE, NULL);
#else
   (void)enable;
#endif
}

bool sthread_cancel(sthread_t *thread)
{
#ifdef RTHREADS_HAVE_CANCEL
   if (thread)
      return pthread_cancel(thread->id) == 0;
   return false;
#else
   (void)thread;
   return false;
#endif
}

void *sthread_priority_override_begin(void)
{
#ifdef RTHREADS_HAVE_QOS_OVERRIDE
   return (void*)pthread_override_qos_class_start_np(
         pthread_self(), QOS_CLASS_USER_INTERACTIVE, 0);
#else
   return NULL;
#endif
}

void sthread_priority_override_end(void *ovr)
{
#ifdef RTHREADS_HAVE_QOS_OVERRIDE
   if (ovr)
      pthread_override_qos_class_end_np((pthread_override_t)ovr);
#else
   (void)ovr;
#endif
}
