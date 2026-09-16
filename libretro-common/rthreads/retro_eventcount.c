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
 * RETRO_EVENTCOUNT_FORCE_SCOND selects the condition-variable backend on
 * any target, so a host with futex can still build and test the path the
 * console and Apple ports take.  It mirrors retro_atomic.h's
 * RETRO_ATOMIC_FORCE_* overrides and exists for the same reason.
 */
#if defined(RETRO_ATOMIC_LOCK_FREE) && !defined(RETRO_EVENTCOUNT_FORCE_SCOND)
#if defined(__linux__) && !defined(ANDROID_NO_FUTEX)
#define RETRO_EC_ADDR_LINUX 1
#elif defined(_WIN32) && !defined(_XBOX) && defined(RETRO_ATOMIC_HAS_PTR)
#define RETRO_EC_ADDR_WIN32 1
#endif
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

#if defined(RETRO_EC_ADDR_WIN32)
#include <windows.h>

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

#define EC_W_WOKEN   1  /* a waker has taken this block          */
#define EC_W_ASLEEP  2  /* the waiter committed to the kernel wait */

#define EC_HEAD_LOCK ((uintptr_t)1)
#define EC_HEAD_MASK (~EC_HEAD_LOCK)

#define EC_STATUS_TIMEOUT 0x102

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
   HANDLE             event;   /* EVENT tier only */
   retro_atomic_int_t flags;
   DWORD              tid;
};

typedef LONG (WINAPI *ec_nt_wait_alert_t)(void*, LARGE_INTEGER*);
typedef LONG (WINAPI *ec_nt_alert_tid_t)(HANDLE);
typedef LONG (WINAPI *ec_nt_keyed_t)(HANDLE, void*, BOOLEAN, LARGE_INTEGER*);
typedef LONG (WINAPI *ec_nt_create_keyed_t)(HANDLE*, ULONG, void*, ULONG);

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

static void ec_wake_one(struct ec_waiter *w)
{
   /* copies taken first: the waiter may leave as soon as it sees WOKEN */
   DWORD  tid   = w->tid;
   HANDLE event = w->event;
   int    prev  = retro_atomic_fetch_or_int(&w->flags, EC_W_WOKEN);

   if (!(prev & EC_W_ASLEEP))
      return;   /* still spinning: it sees the flag, no syscall */

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

static INLINE uintptr_t ec_head(retro_eventcount_t *ec)
{
   return (uintptr_t)retro_atomic_load_acquire_ptr(&ec->waitlist);
}

static void ec_list_lock(retro_eventcount_t *ec)
{
   for (;;)
   {
      uintptr_t old = ec_head(ec);
      if (!(old & EC_HEAD_LOCK)
            && retro_atomic_cas_ptr(&ec->waitlist, (void*)old,
               (void*)(old | EC_HEAD_LOCK)))
         return;
      retro_cpu_relax();
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

/* returns false only when a bounded wait expired */
static bool ec_win32_park(retro_eventcount_t *ec, int key, bool bounded,
      int64_t timeout_us)
{
   struct ec_waiter w;
   LARGE_INTEGER    timeout;
   bool             woken = true;
   unsigned         i;

   w.event = NULL;
   w.next  = NULL;
   w.tid   = GetCurrentThreadId();
   retro_atomic_int_init(&w.flags, 0);

   if (ec_g.sleep == EC_SLEEP_EVENT)
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

   if (!ec_sleep(&w, bounded ? &timeout : NULL))
   {
      /* timed out, unless a waker already took the block, in which case
       * its wake is in flight and has to be consumed */
      ec_list_lock(ec);
      woken = !ec_list_unlink(ec, &w);
      ec_list_unlock(ec);
      if (woken)
         ec_sleep(&w, NULL);
   }

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
#if defined(RETRO_EC_ADDR_WIN32)
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
#if defined(RETRO_EC_ADDR_WIN32)
   if (!ec->cond)
   {
      if (retro_atomic_load_acquire_int(&ec->epoch) == key)
         ec_win32_park(ec, key, false, 0);
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
#if defined(RETRO_EC_ADDR_WIN32)
   if (!ec->cond)
   {
      if (retro_atomic_load_acquire_int(&ec->epoch) == key)
         signalled = ec_win32_park(ec, key, true, timeout_us);
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
