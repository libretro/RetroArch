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
#include <ogc/lwp_watchdog.h>
#include "gx_pthread.h"
#elif defined(_3DS)
#include "ctr_pthread.h"
#else
#include <pthread.h>
#include <time.h>
#endif

#if defined(VITA) || defined(BSD) || defined(ORBIS) || defined(_3DS) || defined(PSP)
#include <sys/time.h>
#endif

/* sthread_setname */
#if defined(__linux__) && !defined(USE_WIN32_THREADS) && !defined(GEKKO) && !defined(_3DS)
#include <sys/prctl.h>
#endif

#if defined(__ANDROID__)
#include <sys/resource.h>
#include <unistd.h>
#endif

#if (defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) \
      || defined(__NetBSD__) || defined(__OpenBSD__)) \
      && !defined(USE_WIN32_THREADS) && !defined(GEKKO) && !defined(_3DS) \
      && !defined(__ANDROID__)
#include <sched.h>
#define RTHREADS_HAVE_SCHEDPARAM 1
#endif

#if defined(PS2)
#include <ps2sdkapi.h>
#endif

#if defined(__MACH__) && defined(__APPLE__)
#include <mach/clock.h>
#include <mach/mach.h>
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

struct sthread
{
#ifdef USE_WIN32_THREADS
   HANDLE thread;
   DWORD id;
#else
   pthread_t id;
#endif
};

struct slock
{
#ifdef USE_WIN32_THREADS
   CRITICAL_SECTION lock;
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

struct scond
{
#ifdef USE_WIN32_THREADS
   retro_atomic_ptr_t head;   /* struct scond_waiter * | SCOND_HEAD_LOCK */
#else
   pthread_cond_t cond;
#endif
};

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

sthread_t *sthread_create(void (*thread_func)(void*), void *userdata)
{
   return sthread_create_with_priority(thread_func, userdata, 0);
}

/* TODO/FIXME - this needs to be implemented for Switch/3DS */
#if !defined(SWITCH) && !defined(USE_WIN32_THREADS) && !defined(_3DS) && !defined(GEKKO) && !defined(__HAIKU__) && !defined(__EMSCRIPTEN__)
#define HAVE_THREAD_ATTR
#endif

sthread_t *sthread_create_with_priority(void (*thread_func)(void*), void *userdata, int thread_priority)
{
#ifdef HAVE_THREAD_ATTR
   pthread_attr_t thread_attr;
   bool thread_attr_needed  = false;
#endif
   bool thread_created      = false;
   struct thread_data *data = NULL;
   sthread_t *thread        = (sthread_t*)malloc(sizeof(*thread));

   if (!thread)
      return NULL;

   if (!(data = (struct thread_data*)malloc(sizeof(*data))))
   {
      free(thread);
      return NULL;
   }

   data->func               = thread_func;
   data->userdata           = userdata;

   thread->id               = 0;
#ifdef USE_WIN32_THREADS
   thread->thread           = CreateThread(NULL, 0, thread_wrap,
         data, 0, &thread->id);
   thread_created           = !!thread->thread;
#else
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

#if defined(VITA)
   pthread_attr_setstacksize(&thread_attr , 0x10000 );
   thread_attr_needed = true;
#elif defined(__APPLE__)
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
   free(data);
   free(thread);
   return NULL;
}

bool sthread_raise_current_priority(void)
{
#if defined(USE_WIN32_THREADS)
   return SetThreadPriority(GetCurrentThread(),
         THREAD_PRIORITY_TIME_CRITICAL) != 0;
#elif defined(__ANDROID__)
   /* Bionic lets an app move its own threads into the audio band
    * without privilege; -16 is ANDROID_PRIORITY_AUDIO. */
   return setpriority(PRIO_PROCESS, gettid(), -16) == 0;
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
#if defined(__linux__) && !defined(USE_WIN32_THREADS) && !defined(GEKKO) && !defined(_3DS)
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
   if (!name)
      return;
   pthread_setname_np(name);
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
#ifdef USE_WIN32_THREADS
   if (!thread)
      return 0;
   CloseHandle(thread->thread);
   free(thread);
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
#ifdef USE_WIN32_THREADS
   WaitForSingleObject(thread->thread, INFINITE);
   CloseHandle(thread->thread);
#else
   pthread_join(thread->id, NULL);
#endif
   free(thread);
}

#if !defined(GEKKO)
bool sthread_isself(sthread_t *thread)
{
#ifdef USE_WIN32_THREADS
   return thread ? GetCurrentThreadId() == thread->id        : false;
#else
   return thread ? pthread_equal(pthread_self(), thread->id) : false;
#endif
}
#endif

slock_t *slock_new(void)
{
   slock_t      *lock = (slock_t*)calloc(1, sizeof(*lock));
   if (!lock)
      return NULL;
#ifdef USE_WIN32_THREADS
   InitializeCriticalSection(&lock->lock);
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

#ifdef USE_WIN32_THREADS
   DeleteCriticalSection(&lock->lock);
#else
   pthread_mutex_destroy(&lock->lock);
#endif
   free(lock);
}

void slock_lock(slock_t *lock)
{
   if (!lock)
      return;
#ifdef USE_WIN32_THREADS
   EnterCriticalSection(&lock->lock);
#else
   pthread_mutex_lock(&lock->lock);
#endif
}

bool slock_try_lock(slock_t *lock)
{
#ifdef USE_WIN32_THREADS
   return lock && TryEnterCriticalSection(&lock->lock);
#else
   return lock && (pthread_mutex_trylock(&lock->lock) == 0);
#endif
}

void slock_unlock(slock_t *lock)
{
   if (!lock)
      return;
#ifdef USE_WIN32_THREADS
   LeaveCriticalSection(&lock->lock);
#else
   pthread_mutex_unlock(&lock->lock);
#endif
}

scond_t *scond_new(void)
{
   scond_t      *cond = (scond_t*)calloc(1, sizeof(*cond));

   if (!cond)
      return NULL;

#ifdef USE_WIN32_THREADS
   scond_global_init();
   retro_atomic_ptr_init(&cond->head, NULL);
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

#ifdef USE_WIN32_THREADS
   /* nothing is owned: the waiter blocks live on their threads' stacks */
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
#ifdef USE_WIN32_THREADS
   scond_wait_win32(cond, lock, INFINITE);
#else
   pthread_cond_wait(&cond->cond, &lock->lock);
#endif
}

int scond_broadcast(scond_t *cond)
{
#ifdef USE_WIN32_THREADS
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
#else
   return pthread_cond_broadcast(&cond->cond);
#endif
}

void scond_signal(scond_t *cond)
{
#ifdef USE_WIN32_THREADS
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
#else
   pthread_cond_signal(&cond->cond);
#endif
}


bool scond_wait_timeout(scond_t *cond, slock_t *lock, int64_t timeout_us)
{
#ifdef USE_WIN32_THREADS
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
#elif defined(PS2)
   {
      int tickms            = ps2_clock();
      now.tv_sec            = tickms / 1000;
      now.tv_nsec           = (long)(tickms % 1000) * 1000000L;
   }
#elif !defined(DINGUX_BETA) && (defined(VITA) || defined(_3DS) || defined(PSP))
   {
      struct timeval tm;
      gettimeofday(&tm, NULL);
      now.tv_sec            = tm.tv_sec;
      now.tv_nsec           = tm.tv_usec * 1000;
   }
#elif defined(RETRO_WIN32_USE_PTHREADS)
   _ftime64_s(&now);
#elif defined(GEKKO)
   {
      const uint64_t tickms = gettime() / TB_TIMER_CLOCK;
      now.tv_sec            = tickms / 1000;
      now.tv_nsec           = (long)(tickms % 1000) * 1000000L;
   }
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
      return (uintptr_t)thread->id;
   return 0;
}

uintptr_t sthread_get_current_thread_id(void)
{
#ifdef USE_WIN32_THREADS
   return (uintptr_t)GetCurrentThreadId();
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
#if !defined(USE_WIN32_THREADS) && !defined(GEKKO) && !defined(_3DS) && !defined(__ANDROID__)
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
