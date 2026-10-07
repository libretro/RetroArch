/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_eventcount.c).
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

/* struct timespec and syscall() are POSIX/glibc surface that a strict
 * C89 compile does not expose by default; libretro-common builds as
 * C89, and rthreads.c asks for the same baseline for the same reason.
 *
 * Guarded the way rthreads.c guards its own, and not only for symmetry.
 * Darwin does not need it - the surface is visible there by default -
 * and asking for it does harm: _POSIX_C_SOURCE lowers
 * __DARWIN_C_LEVEL, which hides the BSD names. In a normal build that
 * would stop at the end of this file, but griffin is one translation
 * unit, so a define made here applies to every file included after it.
 * IFF_UP in net/if.h and RTLD_DEFAULT in dlfcn.h are two that then
 * vanish, in files that never asked for any of this. */
#if defined(__unix__) && !defined(__APPLE__) && !defined(__sun__)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309
#endif
#endif
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <rthreads/retro_eventcount.h>
#include <rthreads/rthreads.h>

/* The address-wait backends hand the kernel a bare 32-bit word, so the
 * epoch must be exactly that.  C89 has no static assertion; a negative
 * array bound is the portable spelling. */
typedef char retro_eventcount_epoch_is_a_word_
   [(sizeof(retro_atomic_int_t) == sizeof(int)) ? 1 : -1];

/* Backend selection.
 *
 * RETRO_EC_ADDR_LINUX and RETRO_EC_ADDR_WIN32 name builds that can park
 * a thread on a bare word.  Both still need the atomics to be lock-free
 * for the handshake in notify to hold, so a build that lands on the
 * volatile fallback drops to the locked backend regardless of platform.
 *
 * RETRO_EC_ADDR_SWITCH parks on the Horizon address arbiter
 * (svcWaitForAddress / svcSignalToAddress), which is the futex shape:
 * wait while the word still holds the value read, wake every waiter on
 * the word.  The arbiter is a 4.0.0 system call, so the object falls
 * back to the condition variable on older firmware, decided once at
 * init from the version libnx reads at startup.
 *
 * RETRO_EC_ADDR_3DS parks on the 3DS's address arbiter, the same shape
 * with a less-than comparison in place of the Switch's equal: the epoch
 * only ever climbs, so waiting while it is below key + 1 is waiting
 * while it still reads key, except at the one key whose successor does
 * not exist, which is answered as a spurious wake instead.
 *
 * RETRO_EC_SEM names the builds that park on a semaphore of the
 * waiting thread's own, listed on the object the way the Windows
 * backend lists its blocks: Darwin, where the semaphore is a Mach one
 * and every call is in Mac OS X 10.4's headers, and the BSDs, where it
 * is a POSIX one.  Like the Windows list it needs the pointer atomics.
 *
 * RETRO_EC_SEM_POOL is the same backend where the thread has nowhere to
 * keep a semaphore of its own: the console ports have no thread-local
 * storage rthreads exposes.  The semaphores are kept in a pool instead,
 * a lock-free stack of kernel semaphores that a park takes one from and
 * puts back once it returns, which the protocol lets it do because a
 * park never returns with a signal still counted.  The Vita takes this
 * one, on the kernel semaphore its threads are built on; so does the
 * PS3, on the lv2 semaphore, and the Wii U, where what is pooled is an
 * auto-reset OSEvent - a signal that arrives before the wait sets it
 * and the wait then returns at once, which is the counted property the
 * protocol needs, since it signals at most once per park.
 *
 * RETRO_EVENTCOUNT_FORCE_SCOND selects the condition-variable backend on
 * any target, so a host with futex can still build and test the path the
 * console ports take.  RETRO_EVENTCOUNT_FORCE_SEM selects the semaphore
 * backend on any POSIX host, so its list protocol runs where futex would
 * otherwise be chosen, and RETRO_EVENTCOUNT_FORCE_SEM_POOL the pooled
 * one.  All mirror retro_atomic.h's RETRO_ATOMIC_FORCE_* overrides and
 * exist for the same reason.  RETRO_EVENTCOUNT_FORCE_SWITCH likewise
 * builds the arbiter protocol on a host that supplies the two system
 * calls and the version query (the sample stands them in over futex),
 * and RETRO_EVENTCOUNT_FORCE_3DS the arbiter's less-than form.
 */
#if defined(RETRO_ATOMIC_LOCK_FREE) && !defined(RETRO_EVENTCOUNT_FORCE_SCOND)
#if defined(RETRO_EVENTCOUNT_FORCE_SEM) && defined(RETRO_ATOMIC_HAS_PTR)
#define RETRO_EC_SEM 1
#elif defined(RETRO_EVENTCOUNT_FORCE_SEM_POOL) && defined(RETRO_ATOMIC_HAS_PTR)
#define RETRO_EC_SEM 1
#define RETRO_EC_SEM_POOL 1
#elif defined(RETRO_EVENTCOUNT_FORCE_SWITCH)
#define RETRO_EC_ADDR_SWITCH 1
#elif defined(RETRO_EVENTCOUNT_FORCE_3DS)
#define RETRO_EC_ADDR_3DS 1
#elif defined(__linux__) && !defined(ANDROID_NO_FUTEX)
#define RETRO_EC_ADDR_LINUX 1
#elif defined(__SWITCH__)
#define RETRO_EC_ADDR_SWITCH 1
#elif defined(_3DS)
#define RETRO_EC_ADDR_3DS 1
#elif (defined(VITA) || defined(__PS3__) || defined(WIIU)) \
      && defined(RETRO_ATOMIC_HAS_PTR)
#define RETRO_EC_SEM 1
#define RETRO_EC_SEM_POOL 1
#elif defined(_WIN32) && !defined(_XBOX) && defined(RETRO_ATOMIC_HAS_PTR)
#define RETRO_EC_ADDR_WIN32 1
#elif (defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) \
      || defined(__OpenBSD__) || defined(__DragonFly__)) \
      && !defined(__ORBIS__) && !defined(ORBIS) \
      && defined(RETRO_ATOMIC_HAS_PTR)
/* The PS4 is FreeBSD underneath and its toolchain defines __FreeBSD__,
 * but its libc carries neither the POSIX clock the bounded wait needs
 * nor a semaphore this backend can rely on: it keeps the condition
 * variable, as rthreads.c and retro_procbarrier.c keep it off the
 * FreeBSD paths. */
#define RETRO_EC_SEM 1
#endif
#endif

/* The two list backends share the waiter list; only the sleep differs. */
#if defined(RETRO_EC_ADDR_WIN32) || defined(RETRO_EC_SEM)
#define RETRO_EC_WAITLIST 1
#endif

/* Where the atomics are not lock-free their read-modify-writes are not
 * atomic either -- the volatile backend spells fetch-add as a plain
 * load, add and store -- so two notifiers can lose an epoch bump and
 * two waiters can lose a count.  The epoch and the waiter count are
 * then kept under the mutex like everything else, which is what makes
 * the any-number-of-waiters, any-number-of-notifiers promise in the
 * header true on those builds as well.  Nothing is held across the
 * caller's predicate window either way.
 *
 * The PS2 backend is atomic but deliberately not lock-free, so it
 * takes the lock too; it has one core, where it is uncontended. */
#if !defined(RETRO_ATOMIC_LOCK_FREE)
#define RETRO_EC_LOCKED_BOOKKEEPING 1
#endif

#if defined(RETRO_EC_ADDR_LINUX)
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/syscall.h>
#include <linux/futex.h>

/* Older kernel headers predate the _PRIVATE flags; the syscall has
 * carried them since 2.6.22 and the numbers are ABI. */
#ifndef FUTEX_WAIT_PRIVATE
#define FUTEX_WAIT_PRIVATE 128
#endif
#ifndef FUTEX_WAKE_PRIVATE
#define FUTEX_WAKE_PRIVATE 129
#endif
#endif

#if defined(RETRO_EC_ADDR_SWITCH)
#include <switch.h>

/* Whether the running firmware has the arbiter: libnx reads the
 * version at startup, and this is asked once per object at init. */
static bool ec_switch_has_arbiter(void)
{
   return hosversionAtLeast(4, 0, 0);
}

/* Waits while the epoch still reads @key, for @timeout_ns (negative:
 * no bound).  InvalidState is the word having moved before the call
 * took effect, which is a wake; only TimedOut is the bound expiring. */
static bool ec_switch_park(retro_eventcount_t *ec, int key, s64 timeout_ns)
{
   Result rc = svcWaitForAddress((void*)&ec->epoch,
         ArbitrationType_WaitIfEqual, (s64)key, timeout_ns);
   return R_VALUE(rc) != KERNELRESULT(TimedOut);
}

static void ec_switch_wake_all(retro_eventcount_t *ec)
{
   svcSignalToAddress((void*)&ec->epoch, SignalType_Signal, 0, -1);
}
#endif

#if defined(RETRO_EC_ADDR_3DS)
#include <3ds/svc.h>
#include <3ds/synchronization.h>
#include <3ds/result.h>
#include <retro_inline.h>

/* One call on the process's arbiter.  libctru 2 wraps it as
 * syncArbitrateAddress[WithTimeout]; libctru 1.x has the arbiter and
 * the svc but not the wrappers, so the call is made directly there,
 * as rthreads.c does on the same toolchains. */
static INLINE Result ec_3ds_arbitrate(s32 *addr, ArbitrationType type,
      s32 value, s64 timeout_ns)
{
#if defined(_3DS) && !defined(USE_CTRULIB_2)
   return svcArbitrateAddress(__sync_get_arbiter(), (u32)addr, type,
         value, timeout_ns);
#else
   return syncArbitrateAddressWithTimeout(addr, type, value, timeout_ns);
#endif
}

/* Waits while the epoch still reads @key, for @timeout_ns.  The
 * arbiter waits while the word is below the value given, and the epoch
 * only climbs, so below key + 1 is still at key.  INT_MAX has no
 * successor to compare against: that key is not parked on, and the
 * caller re-checks its predicate as for any spurious wake. */
static bool ec_3ds_park(retro_eventcount_t *ec, int key, s64 timeout_ns)
{
   Result rc;
   if (key == INT_MAX)
      return true;
   rc = ec_3ds_arbitrate((s32*)&ec->epoch,
         ARBITRATION_WAIT_IF_LESS_THAN_TIMEOUT, (s32)key + 1, timeout_ns);
   return R_DESCRIPTION(rc) != RD_TIMEOUT;
}

static void ec_3ds_wake_all(retro_eventcount_t *ec)
{
   ec_3ds_arbitrate((s32*)&ec->epoch, ARBITRATION_SIGNAL,
         ARBITRATION_SIGNAL_ALL, 0);
}
#endif

#if defined(RETRO_EC_SEM)
#if defined(VITA)
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/error.h>
#elif defined(WIIU)
/* The tree ships its own coreinit headers under wiiu/os; wut's layout
 * is the fallback for a build that points at the SDK instead. */
#if defined(__has_include)
#if __has_include(<wiiu/os/event.h>)
#define RETRO_EC_WIIU_VENDORED
#endif
#endif
#ifdef RETRO_EC_WIIU_VENDORED
/* time.h's tick rate reads the system info, so that comes first */
#include <wiiu/os/systeminfo.h>
#include <wiiu/os/event.h>
#include <wiiu/os/time.h>
#else
#include <coreinit/event.h>
#include <coreinit/time.h>
#endif
#ifndef OSMicroseconds
#define OSMicroseconds(us) OSMicrosecondsToTicks(us)
#endif
#elif defined(__PS3__)
/* Both PS3 SDKs put the lv2 semaphore behind different names, as
 * rthreads.c finds for its mutex and condition variable; the four
 * calls below are written against one set of the file's own. */
#ifdef __PSL1GHT__
#include <sys/sem.h>
typedef sys_sem_t ec_ps3_sem_t;
#define ec_ps3_sem_create(sem, init, max) \
   ec_ps3_sem_create_psl1ght(sem, init, max)
#define ec_ps3_sem_wait     sysSemWait
#define ec_ps3_sem_post     sysSemPost
#else
#include <sys/synchronization.h>
typedef sys_semaphore_t ec_ps3_sem_t;
#define ec_ps3_sem_create(sem, init, max) \
   ec_ps3_sem_create_cell(sem, init, max)
#define ec_ps3_sem_wait     sys_semaphore_wait
#define ec_ps3_sem_post     sys_semaphore_post
#endif
#else
#include <errno.h>
#if !defined(RETRO_EC_SEM_POOL)
#include <pthread.h>
#endif
#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/semaphore.h>
#include <mach/task.h>
#else
#include <semaphore.h>
#include <time.h>
#endif
#endif
#endif

#if defined(RETRO_EC_ADDR_WIN32)
#include <windows.h>
#endif

#if defined(RETRO_EC_WAITLIST)
/* Windows has no one primitive that reaches every version, so the
 * waiter list is ours and only the sleep is delegated.  Three tiers,
 * resolved once from ntdll:
 *
 *   ALERT  NtWaitForAlertByThreadId / NtAlertThreadByThreadId, Windows 8
 *          and newer.  A per-thread wakeup that sticks if it arrives
 *          before the wait, so no rendezvous accounting is needed.
 *   KEYED  NtWaitForKeyedEvent / NtReleaseKeyedEvent, Windows XP and
 *          newer.  A rendezvous: a release blocks until a waiter with
 *          the same key arrives, so a waker may only release for a
 *          block that has committed to sleeping.  The ASLEEP flag below
 *          is what makes that exact.
 *   EVENT  One auto-reset event per thread, kept in TLS for the
 *          thread's life.  Works everywhere, including 9x.
 *
 * rthreads' scond resolves the same three and its protocol is the one
 * copied here; what is dropped is the condition variable's mutex, which
 * every scond_wait re-acquires before returning and which serialises a
 * broadcast across its waiters.
 */

/* The semaphore backend keeps the same list and the same two flags.
 * What it sleeps on is a counting semaphore of the waiting thread's
 * own, created on its first park and kept for the thread's life: a
 * signal that arrives before the wait is counted, not lost, which is
 * the property the ALERT tier above has and the reason neither needs
 * a rendezvous.  A wake is therefore one signal per parked waiter and
 * a park one wait, the same as the address-wait calls cost, with no
 * mutex on either side; and every call it makes is in Mac OS X 10.4's
 * headers, so one binary runs from there up. */

#define EC_W_WOKEN   1  /* a waker has taken this block          */
#define EC_W_ASLEEP  2  /* the waiter committed to the kernel wait */

#define EC_HEAD_LOCK ((uintptr_t)1)
#define EC_HEAD_MASK (~EC_HEAD_LOCK)

#define EC_STATUS_TIMEOUT 0x102

#if defined(RETRO_EC_SEM)
#if defined(VITA)
typedef SceUID      ec_sem_t;
#elif defined(__PS3__)
typedef ec_ps3_sem_t ec_sem_t;
#elif defined(WIIU)
typedef OSEvent     ec_sem_t;
#elif defined(__APPLE__)
typedef semaphore_t ec_sem_t;
#else
typedef sem_t       ec_sem_t;
#endif
#endif

/* How long a waiter spins on its flag word before committing to the
 * kernel, on multiprocessor only.  A budget in microseconds rather
 * than a count of iterations: a PAUSE is worth an order of magnitude
 * more cycles on some processors than others, so a fixed count is a
 * different amount of time on every machine.  The count is derived
 * from a measured relax at resolve time.
 *
 * Measured on Windows 11 x64, four waiters on one object, where a
 * relax costs 11.8ns.  Round trip and broadcast per waiter, against
 * what a spin that finds nothing costs:
 *
 *     spin      burn    round trip   broadcast/waiter
 *        0     0.00us      4.21us          0.94us
 *       64     0.76us      0.27us          0.96us
 *      128     1.49us      0.11us          0.51us
 *      256     2.99us      0.06us          0.11us
 *      512     6.04us      0.08us          0.12us
 *     4096    48.63us      0.07us          0.12us
 *
 * The knee is at 256 iterations there, which is 3us, and nothing above
 * it buys anything -- a spin that finds nothing then costs about what
 * the wake syscall it avoids would have.  RETRO_EVENTCOUNT_SPIN_US
 * moves the budget; RETRO_EVENTCOUNT_SPIN sets the count outright and
 * skips the calibration. */
#define EC_SPIN_US 3

/* Bounds on the derived count, so a mismeasured relax cannot turn the
 * spin into either a no-op or an unbounded burn. */
#define EC_SPIN_MIN 32
#define EC_SPIN_MAX 8192

struct ec_waiter
{
   struct ec_waiter  *next;
#if defined(RETRO_EC_SEM)
   ec_sem_t          *sem;     /* the waiting thread's own */
#else
   HANDLE             event;   /* EVENT tier only */
#endif
   retro_atomic_int_t flags;
#if !defined(RETRO_EC_SEM)
   DWORD              tid;
#endif
};
#endif /* RETRO_EC_WAITLIST */

#if defined(RETRO_EC_ADDR_WIN32)

typedef LONG (WINAPI *ec_nt_wait_alert_t)(volatile void*, LARGE_INTEGER*);
typedef LONG (WINAPI *ec_nt_alert_tid_t)(HANDLE);
typedef LONG (WINAPI *ec_nt_keyed_t)(HANDLE, void*, BOOLEAN, LARGE_INTEGER*);
typedef LONG (WINAPI *ec_nt_create_keyed_t)(HANDLE*, ULONG, void*, ULONG);
typedef HANDLE (WINAPI *ec_create_timer_ex_t)(LPSECURITY_ATTRIBUTES, LPCWSTR,
      DWORD, DWORD);
/* The APC routine and its argument are always NULL here, so they are
 * typed as LPVOID rather than depending on PTIMERAPCROUTINE */
typedef BOOL (WINAPI *ec_set_timer_t)(HANDLE, const LARGE_INTEGER*, LONG,
      LPVOID, LPVOID, BOOL);

#ifndef EC_TIMER_HIGH_RESOLUTION
#define EC_TIMER_HIGH_RESOLUTION 0x00000002 /* CREATE_WAITABLE_TIMER_HIGH_RESOLUTION */
#endif
#ifndef EC_TIMER_ALL_ACCESS
#define EC_TIMER_ALL_ACCESS      0x001F0003 /* TIMER_ALL_ACCESS */
#endif

enum
{
   EC_SLEEP_ALERT = 1,
   EC_SLEEP_KEYED,
   EC_SLEEP_EVENT
};

static struct
{
   ec_nt_wait_alert_t wait_alert;
   ec_nt_alert_tid_t  alert_tid;
   ec_nt_keyed_t      wait_keyed;
   ec_nt_keyed_t      release_keyed;
   HANDLE             keyed;
   DWORD              tls_event;
   /* Bounded waits: a kernel timeout ends on the system timer's tick,
    * 15.6 ms unless something in the process has lowered it, so a
    * bounded wait also sleeps on a high resolution waitable timer, one
    * per thread, together with the thread's event. Unset where the
    * timer cannot be had (before Windows 10 1803): the kernel timeout
    * then bounds the wait, as before. */
   ec_create_timer_ex_t create_timer_ex;
   ec_set_timer_t       set_timer;
   DWORD                tls_timer;
   bool                 hires;
   retro_atomic_int_t state;
   unsigned           spin;   /* 0 on a single processor */
   int                sleep;
} ec_g;

/* Relax iterations that fit in @budget_us.  Timed rather than assumed,
 * and taken as the best of a few short passes so a preemption in the
 * middle of one does not stretch the answer.  QueryPerformanceCounter
 * is the clock because it is the one available on every Windows this
 * backend runs on. */
static unsigned ec_spin_for_budget(unsigned budget_us)
{
   const unsigned  samples = 3;
   const unsigned  iters   = 2048;
   LARGE_INTEGER   freq;
   double          best = 0.0;
   unsigned        s, i;

   if (!budget_us)
      return 0;

   if (!QueryPerformanceFrequency(&freq) || freq.QuadPart <= 0)
      return EC_SPIN_MIN * 8;   /* no clock: a middling fixed count */

   for (s = 0; s < samples; s++)
   {
      LARGE_INTEGER t0, t1;
      double        us;

      QueryPerformanceCounter(&t0);
      for (i = 0; i < iters; i++)
         retro_cpu_relax();
      QueryPerformanceCounter(&t1);

      us = (double)(t1.QuadPart - t0.QuadPart) * 1000000.0
         / (double)freq.QuadPart;

      if (us > 0.0 && (best == 0.0 || us < best))
         best = us;
   }

   if (best <= 0.0)
      return EC_SPIN_MIN * 8;

   {
      double n = (double)budget_us * (double)iters / best;

      if (n < (double)EC_SPIN_MIN)
         return EC_SPIN_MIN;
      if (n > (double)EC_SPIN_MAX)
         return EC_SPIN_MAX;
      return (unsigned)n;
   }
}

static void ec_win32_resolve(void)
{
   HMODULE nt        = GetModuleHandleA("ntdll.dll");
   /* alert / keyed / event pin a tier; none pins the case where no tier
    * is usable, which is otherwise only reachable by exhausting the
    * process's TLS indices.  That case has its own code path and had
    * never been run - see the fallback at the end of this function. */
   const char *force = getenv("RETRO_EVENTCOUNT_WIN32");

   ec_g.sleep = EC_SLEEP_EVENT;

   if (nt && !(force && !strcmp(force, "none")))
   {
      ec_nt_create_keyed_t create_keyed;

      ec_g.wait_alert    = (ec_nt_wait_alert_t)(void (*)(void))
         GetProcAddress(nt, "NtWaitForAlertByThreadId");
      ec_g.alert_tid     = (ec_nt_alert_tid_t)(void (*)(void))
         GetProcAddress(nt, "NtAlertThreadByThreadId");
      ec_g.wait_keyed    = (ec_nt_keyed_t)(void (*)(void))
         GetProcAddress(nt, "NtWaitForKeyedEvent");
      ec_g.release_keyed = (ec_nt_keyed_t)(void (*)(void))
         GetProcAddress(nt, "NtReleaseKeyedEvent");
      create_keyed       = (ec_nt_create_keyed_t)(void (*)(void))
         GetProcAddress(nt, "NtCreateKeyedEvent");

      if (ec_g.wait_alert && ec_g.alert_tid
            && !(force && strcmp(force, "alert")))
         ec_g.sleep = EC_SLEEP_ALERT;
      else if (ec_g.wait_keyed && ec_g.release_keyed && create_keyed
            && !(force && strcmp(force, "keyed"))
            && create_keyed(&ec_g.keyed, 0x1f0003 /* EVENT_ALL_ACCESS */,
               NULL, 0) == 0)
         ec_g.sleep = EC_SLEEP_KEYED;
   }

   if (ec_g.sleep == EC_SLEEP_EVENT)
   {
      ec_g.tls_event = (force && !strcmp(force, "none"))
         ? TLS_OUT_OF_INDEXES : TlsAlloc();
      /* Nothing else can park on this tier without it, so fall through
       * to the condition variable rather than handing out a bad index.
       * sleep == 0 is read by retro_eventcount_init(), which then gives
       * the object a mutex and a condition variable instead of the
       * waiter list, and by every path that would otherwise have used
       * that list. */
      if (ec_g.tls_event == TLS_OUT_OF_INDEXES)
         ec_g.sleep = 0;
   }

   /* The high resolution timer for bounded waits, on every tier that
    * parks on the waiter list. It needs the thread's event to wait on
    * beside it, so that slot is taken here on the tiers that do not
    * otherwise use one. Probed rather than version-checked: the flag is
    * refused before Windows 10 1803. */
   ec_g.hires     = false;
   ec_g.tls_timer = TLS_OUT_OF_INDEXES;
   /* RETRO_EVENTCOUNT_HIRES=0 keeps the kernel timeout alone, for
    * measuring against it */
   if (     ec_g.sleep
         && !((force = getenv("RETRO_EVENTCOUNT_HIRES")) && !strcmp(force, "0")))
   {
      HMODULE k32 = GetModuleHandleA("kernel32.dll");
      if (k32)
      {
         ec_g.create_timer_ex = (ec_create_timer_ex_t)(void (*)(void))
            GetProcAddress(k32, "CreateWaitableTimerExW");
         ec_g.set_timer       = (ec_set_timer_t)(void (*)(void))
            GetProcAddress(k32, "SetWaitableTimer");
      }
      if (ec_g.create_timer_ex && ec_g.set_timer)
      {
         HANDLE probe = ec_g.create_timer_ex(NULL, NULL,
               EC_TIMER_HIGH_RESOLUTION, EC_TIMER_ALL_ACCESS);
         if (probe)
         {
            CloseHandle(probe);
            if (ec_g.sleep != EC_SLEEP_EVENT)
               ec_g.tls_event = TlsAlloc();
            ec_g.tls_timer = TlsAlloc();
            ec_g.hires     = ec_g.tls_event != TLS_OUT_OF_INDEXES
                          && ec_g.tls_timer != TLS_OUT_OF_INDEXES;
         }
      }
   }

   /* Spinning before the kernel wait only pays where the thread being
    * waited for can run at the same time.  On one processor it is pure
    * delay, so the spin is skipped entirely there -- the same gate
    * rthreads' scond applies. */
   {
      SYSTEM_INFO si;
      const char *env;
      unsigned    budget = EC_SPIN_US;

      GetSystemInfo(&si);

      /* An asked-for budget is honoured whatever the processor count,
       * so the calibration is reachable for measurement on a machine
       * that would otherwise skip the spin entirely. */
      if ((env = getenv("RETRO_EVENTCOUNT_SPIN_US")))
         budget = (unsigned)strtoul(env, NULL, 0);
      else if (si.dwNumberOfProcessors <= 1)
         budget = 0;

      ec_g.spin = ec_spin_for_budget(budget);

      /* An explicit count wins over the budget, for sweeping. */
      if ((env = getenv("RETRO_EVENTCOUNT_SPIN")))
         ec_g.spin = (unsigned)strtoul(env, NULL, 0);
   }
}

static void ec_win32_init(void)
{
   if (retro_atomic_load_acquire_int(&ec_g.state) == 2)
      return;
   if (retro_atomic_cas_int(&ec_g.state, 0, 1))
   {
      ec_win32_resolve();
      retro_atomic_store_release_int(&ec_g.state, 2);
      return;
   }
   while (retro_atomic_load_acquire_int(&ec_g.state) != 2)
      Sleep(0);
}

/* block until woken or the timeout (NULL = never) passes; false on timeout */
static bool ec_sleep(struct ec_waiter *w, LARGE_INTEGER *timeout)
{
   switch (ec_g.sleep)
   {
      case EC_SLEEP_ALERT:
         return ec_g.wait_alert(&w->flags, timeout) != EC_STATUS_TIMEOUT;
      case EC_SLEEP_KEYED:
         return ec_g.wait_keyed(ec_g.keyed, w, FALSE, timeout)
            != EC_STATUS_TIMEOUT;
      default:
         {
            DWORD ms = INFINITE;
            DWORD rc;
            if (timeout)
            {
               LONGLONG t = (-timeout->QuadPart + 9999) / 10000;
               /* Rounding up costs at most a millisecond; wrapping a
                * long wait into a short one would be a bug, so it is
                * clamped instead.  INFINITE is not a duration. */
               ms = (t >= (LONGLONG)INFINITE) ? INFINITE - 1 : (DWORD)t;
            }
            rc = WaitForSingleObject(w->event, ms);
            if (rc == WAIT_TIMEOUT)
               return false;
            if (rc == WAIT_OBJECT_0)
               return true;
            /* Anything else is the handle being unusable, which since
             * the TLS fallback was fixed should not be reachable: a
             * CreateEvent that fails is answered in ec_win32_park
             * before the block is ever listed.  Reported as a wake
             * because the caller re-checks its own predicate and a
             * false timeout would be a lie, but slept on first: a bad
             * handle fails immediately, and without this the caller's
             * loop turns into a spin on a core that has no work.  A
             * millisecond of poll is the right shape for something
             * that should not happen at all.
             *
             * There is nowhere to report it from - this is
             * libretro-common and has no logger - so the poll is the
             * whole mitigation.  This branch is not exercised by any
             * test: no way to induce a failing wait on a valid handle
             * was found. */
            Sleep(1);
            return true;
         }
   }
}

/* This thread's event and high resolution timer, made on first use and
 * kept for the thread's life, as the event tier keeps its event. NULL
 * when either cannot be had, and the wait falls back to the kernel
 * timeout. */
static HANDLE ec_thread_event(void)
{
   HANDLE event = (HANDLE)TlsGetValue(ec_g.tls_event);
   if (!event)
   {
      if (!(event = CreateEvent(NULL, FALSE, FALSE, NULL)))
         return NULL;
      if (!TlsSetValue(ec_g.tls_event, event))
      {
         CloseHandle(event);
         return NULL;
      }
   }
   return event;
}

static HANDLE ec_thread_timer(void)
{
   HANDLE timer = (HANDLE)TlsGetValue(ec_g.tls_timer);
   if (!timer)
   {
      if (!(timer = ec_g.create_timer_ex(NULL, NULL,
                  EC_TIMER_HIGH_RESOLUTION, EC_TIMER_ALL_ACCESS)))
         return NULL;
      if (!TlsSetValue(ec_g.tls_timer, timer))
      {
         CloseHandle(timer);
         return NULL;
      }
   }
   return timer;
}

/* A bounded sleep on the waiter's event and the high resolution timer:
 * false when the timer fired first. Setting the timer clears whatever
 * an earlier wait left signalled on it. */
static bool ec_sleep_hires(struct ec_waiter *w, HANDLE timer,
      int64_t timeout_us)
{
   HANDLE        handles[2];
   LARGE_INTEGER due;
   DWORD         rc;

   due.QuadPart = -(LONGLONG)timeout_us * 10;
   if (!ec_g.set_timer(timer, &due, 0, NULL, NULL, FALSE))
   {
      LONGLONG ms = (timeout_us + 999) / 1000;
      rc = WaitForSingleObject(w->event,
            ms >= (LONGLONG)INFINITE ? INFINITE - 1 : (DWORD)ms);
      return rc != WAIT_TIMEOUT;
   }
   handles[0] = w->event;
   handles[1] = timer;
   /* The timer is set and will fire; the bound is only there so a
    * timer that somehow does not cannot make this wait forever */
   rc = WaitForMultipleObjects(2, handles, FALSE,
         (DWORD)(timeout_us / 1000) + 100);
   if (rc == WAIT_OBJECT_0)
      return true;
   if (rc == WAIT_OBJECT_0 + 1 || rc == WAIT_TIMEOUT)
      return false;
   /* An unusable handle: as ec_sleep, a wake after a millisecond */
   Sleep(1);
   return true;
}

static void ec_wake_one(struct ec_waiter *w)
{
   /* copies taken first: the waiter may leave as soon as it sees WOKEN */
   DWORD  tid   = w->tid;
   HANDLE event = w->event;
   int    prev  = retro_atomic_fetch_or_int(&w->flags, EC_W_WOKEN);

   if (!(prev & EC_W_ASLEEP))
      return;   /* still spinning: it sees the flag, no syscall */

   /* A waiter that sleeps on its event - every waiter on the event
    * tier, and bounded ones on the others - is woken through it */
   if (event)
   {
      SetEvent(event);
      return;
   }

   switch (ec_g.sleep)
   {
      case EC_SLEEP_ALERT:
         ec_g.alert_tid((HANDLE)(uintptr_t)tid);
         break;
      case EC_SLEEP_KEYED:
         /* only ever for a block that has committed, so the rendezvous
          * always finds its waiter */
         ec_g.release_keyed(ec_g.keyed, w, FALSE, NULL);
         break;
      default:
         SetEvent(event);
         break;
   }
}

#endif /* RETRO_EC_ADDR_WIN32: resolve, sleep, wake */

#if defined(RETRO_EC_SEM)
/* The three semaphore calls in each platform's spelling.  Create
 * answers false when the kernel will not give one. */
#if defined(__PS3__)
#ifdef __PSL1GHT__
static int ec_ps3_sem_create_psl1ght(ec_ps3_sem_t *sem, int init, int max)
{
   sys_sem_attr_t attr;
   attr.attr_protocol = SYS_SEM_ATTR_PROTOCOL;
   attr.attr_pshared  = SYS_SEM_ATTR_PSHARED;
   attr.key           = 0;
   attr.flags         = 0;
   attr.name[0]       = '\0';
   return sysSemCreate(sem, &attr, init, max);
}
#else
static int ec_ps3_sem_create_cell(ec_ps3_sem_t *sem, int init, int max)
{
   sys_semaphore_attribute_t attr;
   sys_semaphore_attribute_initialize(attr);
   return sys_semaphore_create(sem, &attr, init, max);
}
#endif
#endif

static bool ec_sem_create(ec_sem_t *sem)
{
#if defined(VITA)
   return (*sem = sceKernelCreateSema("rarch_ec", 0, 0, 0x7FFFFFFF, NULL))
      >= 0;
#elif defined(__PS3__)
   return ec_ps3_sem_create(sem, 0, 0x7FFFFFFF) == 0;
#elif defined(WIIU)
   OSInitEvent(sem, FALSE, OS_EVENT_MODE_AUTO);
   return true;
#elif defined(__APPLE__)
   return semaphore_create(mach_task_self(), sem, SYNC_POLICY_FIFO, 0)
      == KERN_SUCCESS;
#else
   return sem_init(sem, 0, 0) == 0;
#endif
}

static void ec_sem_signal(ec_sem_t *sem)
{
#if defined(VITA)
   sceKernelSignalSema(*sem, 1);
#elif defined(__PS3__)
   ec_ps3_sem_post(*sem, 1);
#elif defined(WIIU)
   OSSignalEvent(sem);
#elif defined(__APPLE__)
   semaphore_signal(*sem);
#else
   sem_post(sem);
#endif
}

#if defined(RETRO_EC_SEM_POOL)
/* Nowhere to keep a semaphore per thread, so a pool of them: a stack
 * of slots, each holding a kernel semaphore made on the slot's first
 * use and kept for the process's life.  A park pops a slot and pushes
 * it back on return, and the semaphore it hands back always counts
 * zero, because the park protocol below consumes any signal a waker
 * left in flight before it returns; so the next thread to pop the
 * slot gets a clean one.
 *
 * The stack head packs the slot index with a tag that moves on every
 * pop, which is what keeps a pop that read a stale head from
 * succeeding after the slot went out and came back in between (the
 * ABA case).  An index of zero is the empty stack, so the zeroed
 * static is the initial state and no init step is needed.  The pool
 * is as large as the number of threads that can be parked at once,
 * which on the ports that take it is a handful; a park that finds it
 * empty and full up answers as a spurious wake-up, as the per-thread
 * variant does when a semaphore cannot be had. */
#define EC_POOL_MAX   64
#define EC_POOL_IDX   0xFF
#define EC_POOL_TAG   0x100

struct ec_pool_slot
{
   ec_sem_t           sem;
   retro_atomic_int_t next;   /* index + 1 of the slot below; 0: none */
};

static struct
{
   retro_atomic_int_t   head;   /* (tag) | (index + 1); 0: empty */
   retro_atomic_int_t   made;   /* slots whose semaphore exists */
   struct ec_pool_slot  slot[EC_POOL_MAX];
   int                  ready;
} ec_g;

static void ec_sem_init(void)
{
   ec_g.ready = 1;
}

static ec_sem_t *ec_sem_get(void)
{
   int h;
   int n;

   /* pop */
   for (;;)
   {
      h = retro_atomic_load_acquire_int(&ec_g.head);
      if (!(h & EC_POOL_IDX))
         break;
      n = retro_atomic_load_acquire_int(&ec_g.slot[(h & EC_POOL_IDX) - 1].next);
      if (retro_atomic_cas_int(&ec_g.head, h,
               (int)(((unsigned)h + EC_POOL_TAG) & ~(unsigned)EC_POOL_IDX) | n))
         return &ec_g.slot[(h & EC_POOL_IDX) - 1].sem;
   }

   /* empty: make a slot, while there are slots left to make */
   for (;;)
   {
      n = retro_atomic_load_acquire_int(&ec_g.made);
      if (n >= EC_POOL_MAX)
         return NULL;
      if (retro_atomic_cas_int(&ec_g.made, n, n + 1))
         break;
   }
   if (!ec_sem_create(&ec_g.slot[n].sem))
      return NULL;   /* the slot is spent; the count stays taken */
   return &ec_g.slot[n].sem;
}

static void ec_sem_put(ec_sem_t *sem)
{
   struct ec_pool_slot *s = (struct ec_pool_slot*)sem;
   int                  i = (int)(s - ec_g.slot) + 1;
   int                  h;

   do
   {
      h = retro_atomic_load_acquire_int(&ec_g.head);
      retro_atomic_store_release_int(&s->next, h & EC_POOL_IDX);
   } while (!retro_atomic_cas_int(&ec_g.head, h,
            (int)((unsigned)h & ~(unsigned)EC_POOL_IDX) | i));
}

#else /* per-thread: kept in a key for the thread's life */
static struct
{
   pthread_key_t      key;    /* this thread's semaphore, for its life */
   retro_atomic_int_t state;
   int                ready;  /* the key exists */
} ec_g;

static void ec_sem_release(void *p)
{
   ec_sem_t *sem = (ec_sem_t*)p;
   if (!sem)
      return;
#if defined(__APPLE__)
   semaphore_destroy(mach_task_self(), *sem);
#else
   sem_destroy(sem);
#endif
   free(sem);
}

static void ec_sem_init(void)
{
   if (retro_atomic_load_acquire_int(&ec_g.state) == 2)
      return;
   if (retro_atomic_cas_int(&ec_g.state, 0, 1))
   {
      ec_g.ready = (pthread_key_create(&ec_g.key, ec_sem_release) == 0);
      retro_atomic_store_release_int(&ec_g.state, 2);
      return;
   }
   while (retro_atomic_load_acquire_int(&ec_g.state) != 2)
      sthread_yield();
}

/* This thread's semaphore, made on its first park.  NULL when one
 * cannot be had, which the park answers as a spurious wake. */
static ec_sem_t *ec_sem_get(void)
{
   ec_sem_t *sem = (ec_sem_t*)pthread_getspecific(ec_g.key);
   if (sem)
      return sem;
   if (!(sem = (ec_sem_t*)malloc(sizeof(*sem))))
      return NULL;
   if (!ec_sem_create(sem))
   {
      free(sem);
      return NULL;
   }
   pthread_setspecific(ec_g.key, sem);
   return sem;
}

static void ec_sem_put(ec_sem_t *sem)
{
   (void)sem;   /* the thread keeps it */
}
#endif /* RETRO_EC_SEM_POOL */

/* Blocks on the semaphore until signalled or, when bounded, until
 * timeout_us has passed.  An interrupted wait resumes: the block is
 * still listed, and leaving here would free its frame under a waker.
 * False on the timeout, and on the Vita and the PS3 on any other
 * failure too: the caller then takes the block off the list itself,
 * which is the one safe way out. */
static bool ec_sem_sleep(ec_sem_t *sem, bool bounded, int64_t timeout_us)
{
#if defined(VITA)
   SceUInt us;
   if (!bounded)
      return sceKernelWaitSema(*sem, 1, NULL) == 0;
   /* a bound of zero would mean none to the kernel */
   us = timeout_us > 0 ? (SceUInt)timeout_us : 1;
   return sceKernelWaitSema(*sem, 1, &us) == 0;
#elif defined(__PS3__)
   /* An lv2 wait takes microseconds, with zero standing for no
    * timeout, so a bound of zero is spelled as one. */
   if (!bounded)
      return ec_ps3_sem_wait(*sem, 0) == 0;
   return ec_ps3_sem_wait(*sem,
         (uint64_t)(timeout_us > 0 ? timeout_us : 1)) == 0;
#elif defined(WIIU)
   if (!bounded)
   {
      OSWaitEvent(sem);
      return true;
   }
   /* In timer ticks; a bound of zero is spelled as a microsecond */
   return OSWaitEventWithTimeout(sem,
         (OSTime)OSMicroseconds(timeout_us > 0 ? timeout_us : 1))
      ? true : false;
#elif defined(__APPLE__)
   kern_return_t kr;
   if (bounded)
   {
      mach_timespec_t ts;
      ts.tv_sec  = (unsigned int)(timeout_us / 1000000);
      ts.tv_nsec = (clock_res_t)((timeout_us % 1000000) * 1000);
      do
      {
         kr = semaphore_timedwait(*sem, ts);
      } while (kr == KERN_ABORTED);
      return kr != KERN_OPERATION_TIMED_OUT;
   }
   do
   {
      kr = semaphore_wait(*sem);
   } while (kr == KERN_ABORTED);
   return true;
#else
   int r;
   if (bounded)
   {
      struct timespec ts;
      clock_gettime(CLOCK_REALTIME, &ts);
      ts.tv_sec  += (time_t)(timeout_us / 1000000);
      ts.tv_nsec += (long)((timeout_us % 1000000) * 1000);
      if (ts.tv_nsec >= 1000000000L)
      {
         ts.tv_sec++;
         ts.tv_nsec -= 1000000000L;
      }
      do
      {
         r = sem_timedwait(sem, &ts);
      } while (r != 0 && errno == EINTR);
      return !(r != 0 && errno == ETIMEDOUT);
   }
   do
   {
      r = sem_wait(sem);
   } while (r != 0 && errno == EINTR);
   return true;
#endif
}

static void ec_wake_one(struct ec_waiter *w)
{
   /* copied first: the waiter may leave as soon as it sees WOKEN */
   ec_sem_t *sem = w->sem;
   int       prev = retro_atomic_fetch_or_int(&w->flags, EC_W_WOKEN);

   if (!(prev & EC_W_ASLEEP))
      return;   /* not yet committed: it sees the flag, no syscall */
   ec_sem_signal(sem);
}
#endif /* RETRO_EC_SEM */

#if defined(RETRO_EC_WAITLIST)
static INLINE uintptr_t ec_head(retro_eventcount_t *ec)
{
   return (uintptr_t)retro_atomic_load_acquire_ptr(&ec->waitlist);
}

/* The bit is held across a few instructions - a swap of the head, an
 * unlink - so a spin is right, but a bounded one: with more runnable
 * threads than cores the holder may not be running, and a spin then
 * only keeps it from getting back on. */
static void ec_list_lock(retro_eventcount_t *ec)
{
   unsigned spins = 0;
   for (;;)
   {
      uintptr_t old = ec_head(ec);
      if (!(old & EC_HEAD_LOCK)
            && retro_atomic_cas_ptr(&ec->waitlist, (void*)old,
               (void*)(old | EC_HEAD_LOCK)))
         return;
      if (++spins < 64)
         retro_cpu_relax();
      else
      {
         sthread_yield();
         spins = 0;
      }
   }
}

static void ec_list_unlock(retro_eventcount_t *ec)
{
   for (;;)
   {
      uintptr_t old = ec_head(ec);
      if (retro_atomic_cas_ptr(&ec->waitlist, (void*)old,
               (void*)(old & EC_HEAD_MASK)))
         return;
   }
}

/* unlink w if it is still listed; the list lock must be held.  A block
 * that is gone has been taken by a waker, whose wake is on its way. */
static bool ec_list_unlink(retro_eventcount_t *ec, struct ec_waiter *w)
{
   for (;;)
   {
      uintptr_t old = ec_head(ec);
      struct ec_waiter *n = (struct ec_waiter*)(old & EC_HEAD_MASK);

      if (n == w)
      {
         if (retro_atomic_cas_ptr(&ec->waitlist, (void*)old,
                  (void*)((uintptr_t)w->next | EC_HEAD_LOCK)))
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

static void ec_list_push(retro_eventcount_t *ec, struct ec_waiter *w)
{
   uintptr_t old;
   do
   {
      old    = ec_head(ec);
      w->next = (struct ec_waiter*)(old & EC_HEAD_MASK);
   } while (!retro_atomic_cas_ptr(&ec->waitlist, (void*)old,
            (void*)((uintptr_t)w | (old & EC_HEAD_LOCK))));
}

static void ec_wake_all(retro_eventcount_t *ec)
{
   struct ec_waiter *w;
   uintptr_t         old;

   if (!(ec_head(ec) & EC_HEAD_MASK))
      return;

   ec_list_lock(ec);
   /* Take the whole list, dropping the lock in the same swap.  A push
    * does not take the list lock -- it only preserves the bit -- so
    * reading the head and then storing over it would drop any block
    * that landed in between, and that block would never be woken. */
   do
   {
      old = ec_head(ec);
   } while (!retro_atomic_cas_ptr(&ec->waitlist, (void*)old, NULL));

   w = (struct ec_waiter*)(old & EC_HEAD_MASK);
   while (w)
   {
      struct ec_waiter *next = w->next;
      ec_wake_one(w);
      w = next;
   }
}

#endif /* RETRO_EC_WAITLIST */

#if defined(RETRO_EC_ADDR_WIN32)
/* returns false only when a bounded wait expired */
static bool ec_win32_park(retro_eventcount_t *ec, int key, bool bounded,
      int64_t timeout_us)
{
   struct ec_waiter w;
   LARGE_INTEGER    timeout;
   HANDLE           timer = NULL;
   bool             woken = true;
   unsigned         i;

   w.event = NULL;
   w.next  = NULL;
   w.tid   = GetCurrentThreadId();
   retro_atomic_int_init(&w.flags, 0);

   /* A bounded wait sleeps on the thread's event and its high
    * resolution timer, whatever the tier, so it ends when it should
    * rather than on the system timer's tick. Decided before the block
    * is listed: the waker reads w.event to know how to wake it. */
   if (     bounded && timeout_us > 0 && ec_g.hires
         && (timer = ec_thread_timer()))
   {
      if (!(w.event = ec_thread_event()))
         timer = NULL;
   }

   if (!w.event && ec_g.sleep == EC_SLEEP_EVENT)
   {
      if (!(w.event = (HANDLE)TlsGetValue(ec_g.tls_event)))
      {
         if (!(w.event = CreateEvent(NULL, FALSE, FALSE, NULL)))
            return true;   /* nothing to wait on: a spurious wake-up */
         TlsSetValue(ec_g.tls_event, w.event);
      }
   }

   ec_list_push(ec, &w);

   /* Listed first, then re-check: a notify from here on either finds
    * this block or has already moved the epoch. */
   if (retro_atomic_load_acquire_int(&ec->epoch) != key)
   {
      bool unlinked;

      ec_list_lock(ec);
      unlinked = ec_list_unlink(ec, &w);
      ec_list_unlock(ec);

      /* This block lives on this thread's stack, so leaving here frees
       * it.  That is only safe while it is still listed: a waker that
       * has already taken it off the list is walking it right now, and
       * publishes WOKEN once it is done reading -- after it has taken
       * the next pointer and the wake-up details.  So when the unlink
       * finds nothing, wait for that flag before the frame goes away.
       * No wake is in flight to consume: ASLEEP is not set yet, so the
       * waker issued none. */
      if (!unlinked)
      {
         while (!(retro_atomic_load_acquire_int(&w.flags) & EC_W_WOKEN))
            retro_cpu_relax();
      }
      return true;
   }

   for (i = 0; i < ec_g.spin; i++)
   {
      if (retro_atomic_load_acquire_int(&w.flags) & EC_W_WOKEN)
         return true;
      retro_cpu_relax();
   }

   /* commit: past this a waker that takes the block must wake us */
   if (retro_atomic_fetch_or_int(&w.flags, EC_W_ASLEEP) & EC_W_WOKEN)
      return true;

   if (bounded)
      timeout.QuadPart = -(LONGLONG)timeout_us * 10;

   if (timer ? !ec_sleep_hires(&w, timer, timeout_us)
             : !ec_sleep(&w, bounded ? &timeout : NULL))
   {
      /* timed out, unless a waker already took the block, in which case
       * its wake is in flight and has to be consumed - on the event,
       * where the block slept on one */
      ec_list_lock(ec);
      woken = !ec_list_unlink(ec, &w);
      ec_list_unlock(ec);
      if (woken)
      {
         if (timer)
            WaitForSingleObject(w.event, INFINITE);
         else
            ec_sleep(&w, NULL);
      }
   }

   return woken;
}
#endif

#if defined(RETRO_EC_SEM)
/* The Windows park without its spin: returns false only when a
 * bounded wait expired.  Every way out hands the semaphore back with
 * nothing counted on it, which is what lets the pooled variant give
 * it to another thread next. */
static bool ec_sem_park(retro_eventcount_t *ec, int key, bool bounded,
      int64_t timeout_us)
{
   struct ec_waiter w;
   bool             woken = true;

   if (!(w.sem = ec_sem_get()))
   {
      /* Nothing to wait on: a spurious wake-up, after a turn for the
       * thread this one would have waited for, so a pool that has
       * run out degrades to yielding rather than spinning. */
      sthread_yield();
      return true;
   }
   w.next = NULL;
   retro_atomic_int_init(&w.flags, 0);

   ec_list_push(ec, &w);

   /* Listed first, then re-check: a notify from here on either finds
    * this block or has already moved the epoch. */
   if (retro_atomic_load_acquire_int(&ec->epoch) != key)
   {
      bool unlinked;

      ec_list_lock(ec);
      unlinked = ec_list_unlink(ec, &w);
      ec_list_unlock(ec);

      /* This block lives on this thread's stack: a waker that has
       * taken it off the list is reading it now and publishes WOKEN
       * once done, so the frame stays until then.  No signal is in
       * flight to consume: ASLEEP was never set. */
      if (!unlinked)
      {
         while (!(retro_atomic_load_acquire_int(&w.flags) & EC_W_WOKEN))
            retro_cpu_relax();
      }
      ec_sem_put(w.sem);
      return true;
   }

   /* commit: past this a waker that takes the block must signal */
   if (retro_atomic_fetch_or_int(&w.flags, EC_W_ASLEEP) & EC_W_WOKEN)
   {
      ec_sem_put(w.sem);   /* taken before the commit: no signal sent */
      return true;
   }

   if (!ec_sem_sleep(w.sem, bounded, timeout_us))
   {
      /* timed out, unless a waker already took the block, in which case
       * its signal is in flight and has to be consumed so the count
       * does not carry into the semaphore's next park */
      ec_list_lock(ec);
      woken = !ec_list_unlink(ec, &w);
      ec_list_unlock(ec);
      if (woken)
         ec_sem_sleep(w.sem, false, 0);
   }

   ec_sem_put(w.sem);
   return woken;
}
#endif

bool retro_eventcount_init(retro_eventcount_t *ec)
{
   int lockless = 0;

   if (!ec)
      return false;

   memset(ec, 0, sizeof(*ec));
   retro_atomic_int_init(&ec->epoch, 0);
   retro_atomic_int_init(&ec->waiters, 0);

#if defined(RETRO_EC_ADDR_LINUX)
   lockless = 1;
#elif defined(RETRO_EC_ADDR_SWITCH)
   /* Only on firmware with the arbiter; else this object takes the
    * condition variable and ec->cond says so */
   if (ec_switch_has_arbiter())
      lockless = 1;
#elif defined(RETRO_EC_ADDR_3DS)
   lockless = 1;
#elif defined(RETRO_EC_SEM)
   ec_sem_init();
   /* As for Win32 below: only with a key to keep the semaphores in,
    * else this object takes the condition variable and ec->cond says
    * so. */
   if (ec_g.ready)
   {
      retro_atomic_ptr_init(&ec->waitlist, NULL);
      lockless = 1;
   }
#elif defined(RETRO_EC_ADDR_WIN32)
   ec_win32_init();
   /* Only if a sleep tier resolved. With none - no ntdll entry points
    * and no TLS index for the per-thread event - the waiter list has
    * nothing to sleep on, so this object takes a mutex and a condition
    * variable and every path below follows it there. ec->cond is what
    * says which of the two this object is, rather than the global:
    * one load of a field that never changes after this point. */
   if (ec_g.sleep)
   {
      retro_atomic_ptr_init(&ec->waitlist, NULL);
      lockless = 1;
   }
#endif

   if (lockless)
      return true;

   if (!(ec->lock = slock_new()))
      return false;

   if (!(ec->cond = scond_new()))
   {
      slock_free(ec->lock);
      ec->lock = NULL;
      return false;
   }

   return true;
}

void retro_eventcount_free(retro_eventcount_t *ec)
{
   if (!ec)
      return;

   if (ec->cond)
      scond_free(ec->cond);
   if (ec->lock)
      slock_free(ec->lock);

   ec->cond = NULL;
   ec->lock = NULL;
}

void retro_eventcount_notify(retro_eventcount_t *ec)
{
   /* Publish the state change, then read the waiter count, both
    * sequentially consistent.  The two here and the mirrored pair in
    * prepare_wait share one total order, which is what stops a notify
    * reading zero waiters while a consumer that has not yet seen the
    * new epoch is on its way into a park.  A separate seq_cst fence
    * would do the same job and costs a second locked operation. */
#if defined(RETRO_EC_LOCKED_BOOKKEEPING)
   slock_lock(ec->lock);
   retro_atomic_fetch_add_int(&ec->epoch, 1);
   if (retro_atomic_load_relaxed_int(&ec->waiters) != 0)
      scond_broadcast(ec->cond);
   slock_unlock(ec->lock);
   return;
#else
   retro_atomic_fetch_add_seq_cst_int(&ec->epoch, 1);
#endif

#if defined(RETRO_ATOMIC_LOCK_FREE)
   /* The ordering is the sequentially-consistent bump above and this
    * sequentially-consistent load, as a pair: the store is visible
    * before the load is taken, so a notify cannot read zero waiters
    * while a registering waiter reads the pre-notify epoch. There is
    * no separate fence here any more - there was, and this comment
    * used to name it.
    *
    * Where the atomics degrade to a compiler barrier the pair carries
    * no such guarantee, so that build takes the lock on every notify
    * instead -- the single-core case, where it is uncontended. */
   if (retro_atomic_load_seq_cst_int(&ec->waiters) == 0)
      return;
#endif

#if defined(RETRO_EC_ADDR_LINUX)
   syscall(SYS_futex, (void*)&ec->epoch, FUTEX_WAKE_PRIVATE,
         INT_MAX, NULL, NULL, 0);
#else
#if defined(RETRO_EC_ADDR_SWITCH)
   if (!ec->cond)
   {
      ec_switch_wake_all(ec);
      return;
   }
#endif
#if defined(RETRO_EC_ADDR_3DS)
   ec_3ds_wake_all(ec);
   return;
#endif
#if defined(RETRO_EC_WAITLIST)
   if (!ec->cond)
   {
      ec_wake_all(ec);
      return;
   }
#endif
   /* Taking the lock here cannot overtake a waiter's own re-check,
    * because commit_wait does that re-check under this same lock and
    * then sleeps on the condition variable, which releases it
    * atomically. prepare_wait does NOT hold it - it registers and
    * reads the epoch with atomics alone, which is what keeps N
    * waiters from serialising here just to announce themselves. */
   slock_lock(ec->lock);
   scond_broadcast(ec->cond);
   slock_unlock(ec->lock);
#endif
}

int retro_eventcount_prepare_wait(retro_eventcount_t *ec)
{
   /* Mirror of notify: register with a sequentially-consistent
    * read-modify-write, then take a sequentially-consistent load of the
    * epoch.  There are no standalone fences here any more - this
    * comment used to name two.  The pair here and the pair in notify
    * share one total order, which is what makes it impossible for a
    * notify to see no waiters and for this thread to read the
    * pre-notify epoch.
    *
    * Lock-free backends register with atomics alone. Where the
    * atomics are not lock-free the bookkeeping is protected by
    * ec->lock, just below -- but never held across the caller's
    * predicate window either way. The epoch carries the handshake, so
    * the condition-variable backend needs its mutex only across the
    * re-check-and-sleep in commit_wait, which is what keeps N waiters
    * from serialising on this object to register. */
#if defined(RETRO_EC_LOCKED_BOOKKEEPING)
   {
      int key;
      slock_lock(ec->lock);
      retro_atomic_fetch_add_int(&ec->waiters, 1);
      key = retro_atomic_load_relaxed_int(&ec->epoch);
      slock_unlock(ec->lock);
      return key;
   }
#else
   retro_atomic_fetch_add_seq_cst_int(&ec->waiters, 1);

   return retro_atomic_load_seq_cst_int(&ec->epoch);
#endif
}

void retro_eventcount_cancel_wait(retro_eventcount_t *ec)
{
#if defined(RETRO_EC_LOCKED_BOOKKEEPING)
   slock_lock(ec->lock);
#endif
   retro_atomic_fetch_sub_int(&ec->waiters, 1);
#if defined(RETRO_EC_LOCKED_BOOKKEEPING)
   slock_unlock(ec->lock);
#endif
}

void retro_eventcount_commit_wait(retro_eventcount_t *ec, int key)
{
#if defined(RETRO_EC_ADDR_LINUX)
   int expect = key;
   if (retro_atomic_load_acquire_int(&ec->epoch) == key)
      syscall(SYS_futex, (void*)&ec->epoch, FUTEX_WAIT_PRIVATE,
            expect, NULL, NULL, 0);
   retro_atomic_fetch_sub_int(&ec->waiters, 1);
#else
#if defined(RETRO_EC_ADDR_SWITCH)
   if (!ec->cond)
   {
      if (retro_atomic_load_acquire_int(&ec->epoch) == key)
         ec_switch_park(ec, key, -1);
      retro_atomic_fetch_sub_int(&ec->waiters, 1);
      return;
   }
#endif
#if defined(RETRO_EC_ADDR_3DS)
   /* No unbounded form of the wait: a long bound, re-armed while the
    * epoch still reads key, is the same thing */
   while (   retro_atomic_load_acquire_int(&ec->epoch) == key
          && !ec_3ds_park(ec, key, INT64_C(1000000000)))
      ;
   retro_atomic_fetch_sub_int(&ec->waiters, 1);
   return;
#endif
#if defined(RETRO_EC_ADDR_WIN32)
   if (!ec->cond)
   {
      if (retro_atomic_load_acquire_int(&ec->epoch) == key)
         ec_win32_park(ec, key, false, 0);
      retro_atomic_fetch_sub_int(&ec->waiters, 1);
      return;
   }
#endif
#if defined(RETRO_EC_SEM)
   if (!ec->cond)
   {
      if (retro_atomic_load_acquire_int(&ec->epoch) == key)
         ec_sem_park(ec, key, false, 0);
      retro_atomic_fetch_sub_int(&ec->waiters, 1);
      return;
   }
#endif
   /* The mutex is taken here, not in prepare_wait: it has to cover the
    * epoch re-check and the sleep together, and nothing before that.  A
    * notify that lands before the lock is acquired has already moved
    * the epoch, so the re-check finds it and this never sleeps. */
   slock_lock(ec->lock);
   if (retro_atomic_load_acquire_int(&ec->epoch) == key)
      scond_wait(ec->cond, ec->lock);
   retro_atomic_fetch_sub_int(&ec->waiters, 1);
   slock_unlock(ec->lock);
#endif
}

bool retro_eventcount_commit_wait_timeout(retro_eventcount_t *ec,
      int key, int64_t timeout_us)
{
   bool signalled = true;

   if (timeout_us < 0)
      timeout_us = 0;

#if defined(RETRO_EC_ADDR_LINUX)
   if (retro_atomic_load_acquire_int(&ec->epoch) == key)
   {
      struct timespec ts;
      int expect = key;

      ts.tv_sec  = (time_t)(timeout_us / 1000000);
      ts.tv_nsec = (long)((timeout_us % 1000000) * 1000);

      if (syscall(SYS_futex, (void*)&ec->epoch, FUTEX_WAIT_PRIVATE,
               expect, &ts, NULL, 0) != 0 && errno == ETIMEDOUT)
         signalled = false;
   }
   retro_atomic_fetch_sub_int(&ec->waiters, 1);
#else
#if defined(RETRO_EC_ADDR_SWITCH)
   if (!ec->cond)
   {
      if (retro_atomic_load_acquire_int(&ec->epoch) == key)
      {
         /* A bound of zero polls; the call takes nanoseconds and a
          * negative value means none, so zero is spelled as one. */
         s64 ns = timeout_us > 0 ? (s64)timeout_us * 1000 : 1;
         signalled = ec_switch_park(ec, key, ns);
      }
      retro_atomic_fetch_sub_int(&ec->waiters, 1);
      return signalled;
   }
#endif
#if defined(RETRO_EC_ADDR_3DS)
   if (retro_atomic_load_acquire_int(&ec->epoch) == key)
      signalled = ec_3ds_park(ec, key,
            timeout_us > 0 ? (s64)timeout_us * 1000 : 1);
   retro_atomic_fetch_sub_int(&ec->waiters, 1);
   return signalled;
#endif
#if defined(RETRO_EC_ADDR_WIN32)
   if (!ec->cond)
   {
      if (retro_atomic_load_acquire_int(&ec->epoch) == key)
         signalled = ec_win32_park(ec, key, true, timeout_us);
      retro_atomic_fetch_sub_int(&ec->waiters, 1);
      return signalled;
   }
#endif
#if defined(RETRO_EC_SEM)
   if (!ec->cond)
   {
      if (retro_atomic_load_acquire_int(&ec->epoch) == key)
         signalled = ec_sem_park(ec, key, true, timeout_us);
      retro_atomic_fetch_sub_int(&ec->waiters, 1);
      return signalled;
   }
#endif
   slock_lock(ec->lock);
   if (retro_atomic_load_acquire_int(&ec->epoch) == key)
      signalled = scond_wait_timeout(ec->cond, ec->lock, timeout_us);
   retro_atomic_fetch_sub_int(&ec->waiters, 1);
   slock_unlock(ec->lock);
#endif

   return signalled;
}

unsigned retro_eventcount_spin_iters(void)
{
#if defined(RETRO_EC_ADDR_WIN32)
   ec_win32_init();
   /* The fallback parks on a condition variable, which has no flag word
    * to spin on. */
   return ec_g.sleep ? ec_g.spin : 0;
#else
   return 0;
#endif
}

const char *retro_eventcount_backend_name(void)
{
#if defined(RETRO_EC_ADDR_LINUX)
   return "futex";
#elif defined(RETRO_EC_ADDR_SWITCH)
   return ec_switch_has_arbiter()
      ? "horizon address arbiter" : "scond (firmware before 4.0.0)";
#elif defined(RETRO_EC_ADDR_3DS)
   return "3ds address arbiter";
#elif defined(RETRO_EC_SEM)
   ec_sem_init();
#if defined(VITA)
   return "sce semaphore pool";
#elif defined(__PS3__)
   return "lv2 semaphore pool";
#elif defined(WIIU)
   return "coreinit event pool";
#elif defined(RETRO_EC_SEM_POOL)
   return "posix semaphore pool";
#elif defined(__APPLE__)
   return ec_g.ready ? "mach semaphore" : "scond (no thread key)";
#else
   return ec_g.ready ? "posix semaphore" : "scond (no thread key)";
#endif
#elif defined(RETRO_EC_ADDR_WIN32)
   ec_win32_init();
   switch (ec_g.sleep)
   {
      case EC_SLEEP_ALERT: return "ntdll alert-by-thread-id";
      case EC_SLEEP_KEYED: return "ntdll keyed event";
      case EC_SLEEP_EVENT: return "win32 event";
      default:             return "scond (no win32 sleep primitive)";
   }
#elif !defined(RETRO_ATOMIC_LOCK_FREE)
   return "scond (atomics not lock-free)";
#elif defined(RETRO_EVENTCOUNT_FORCE_SCOND)
   return "scond (forced)";
#else
   return "scond";
#endif
}
