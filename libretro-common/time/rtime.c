/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rtime.c).
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

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#include <stdlib.h>
#endif

#include <string.h>

#include <boolean.h>
#include <time/rtime.h>

#ifdef HAVE_THREADS
/* TODO/FIXME - global */
slock_t *rtime_localtime_lock = NULL;
#endif

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

/* Desktop Windows implementation of retro_sleep().
 *
 * Sleep() rounds up to the global timer period, which is ~15.6 ms by
 * default - longer than one frame at any common refresh rate. Until
 * now RetroArch got a 1 ms period by accident, because the WinMM MIDI
 * driver is the Windows default and a running MIDI stream holds the
 * period down for the whole process; anything that stops that stream
 * from being opened silently broke Frame Delay and Scanline Sync.
 *
 * Rather than reintroduce that dependency by requesting a global
 * period with timeBeginPeriod(), use a high resolution waitable
 * timer. This affects only this process' own waits, needs no winmm,
 * and cannot be cancelled out from under us by an unrelated
 * timeEndPeriod() elsewhere in the process.
 *
 * Availability:
 * - CREATE_WAITABLE_TIMER_HIGH_RESOLUTION requires Windows 10 1803.
 * - CreateWaitableTimerExW() itself requires Windows Vista.
 * Both are resolved at runtime and the flag is probed rather than
 * version-checked, so this builds and runs unchanged all the way back
 * to Windows 95 (and against SDKs predating either); anything that
 * cannot satisfy the above simply keeps the old Sleep() behaviour. */

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

#ifndef TIMER_ALL_ACCESS
#define TIMER_ALL_ACCESS (STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | 0x3)
#endif

#ifndef TLS_OUT_OF_INDEXES
#define TLS_OUT_OF_INDEXES ((DWORD)0xFFFFFFFF)
#endif

typedef HANDLE (WINAPI *CreateWaitableTimerExW_t)(
      LPSECURITY_ATTRIBUTES, LPCWSTR, DWORD, DWORD);
/* The APC routine and completion argument are always NULL here, so
 * they are typed as LPVOID to avoid depending on PTIMERAPCROUTINE
 * being present in the SDK being built against. */
typedef BOOL (WINAPI *SetWaitableTimer_t)(
      HANDLE, const LARGE_INTEGER*, LONG, LPVOID, LPVOID, BOOL);

/* Pre-1803 fallback.
 *
 * Without a high resolution waitable timer, Sleep() is only as precise
 * as the global timer period, so that period has to be lowered for
 * Frame Delay and Scanline Sync to work at all. The documented way to
 * do that is timeBeginPeriod(), which lives in winmm - the dependency
 * this whole exercise exists to remove - so go to what winmm actually
 * does instead.
 *
 * winmm!timeBeginPeriod is an export forwarder to winmmbase, which
 * range checks the period, inserts it into a per-process list under a
 * critical section, and when the list minimum drops calls
 * ntdll!NtSetTimerResolution with the period converted to 100 ns units.
 * timeEndPeriod removes the entry and only calls
 * NtSetTimerResolution(0, FALSE) once the list drains. (Call chain from
 * public reverse engineering of winmmbase, not from a dump taken here.)
 *
 * So the syscall is NtSetTimerResolution and we can call it directly.
 * The caveat is that the list and critical section above are the entire
 * refcount: the kernel keeps exactly ONE resolution request per process
 * with no reference counting, and SetResolution=FALSE clears it no
 * matter who asked for it. Bypassing winmmbase means any unrelated
 * timeBeginPeriod/timeEndPeriod pair elsewhere in the process - a
 * driver DLL, a core, dsound - drains winmmbase's list to empty, which
 * clears the process request and takes our resolution with it. That is
 * the same silent-degradation failure this series is fixing, so the
 * request is reasserted on every fallback sleep rather than being set
 * once and trusted. NtSetTimerResolution replaces rather than
 * accumulates, so reasserting is idempotent, and one cheap syscall in
 * front of a >=1 ms sleep is not measurable. */

enum rtime_sleep_state
{
   RTIME_SLEEP_UNKNOWN = 0,
   RTIME_SLEEP_FALLBACK,
   RTIME_SLEEP_TIMERES,
   RTIME_SLEEP_HIGHRES
};

typedef LONG (WINAPI *NtSetTimerResolution_t)(ULONG, BOOLEAN, PULONG);
typedef LONG (WINAPI *NtQueryTimerResolution_t)(PULONG, PULONG, PULONG);

static CreateWaitableTimerExW_t rtime_create_waitable_timer_ex = NULL;
static SetWaitableTimer_t rtime_set_waitable_timer             = NULL;
static NtSetTimerResolution_t rtime_nt_set_timer_resolution    = NULL;
static ULONG rtime_timer_resolution       = 0;
static volatile LONG rtime_sleep_state    = RTIME_SLEEP_UNKNOWN;
static volatile LONG rtime_sleep_init_ran = 0;
static DWORD rtime_sleep_tls              = TLS_OUT_OF_INDEXES;

static bool rtime_timer_resolution_init(void)
{
   HMODULE ntdll;
   NtQueryTimerResolution_t query;
   ULONG res_coarsest = 0;
   ULONG res_finest   = 0;
   ULONG res_current  = 0;
   ULONG desired      = 10000; /* 1 ms, in 100 ns units */

   /* NT only; on Win9x there is no ntdll and the default period was
    * already 1 ms anyway. */
   if (!(ntdll = GetModuleHandleA("ntdll.dll")))
      return false;

   rtime_nt_set_timer_resolution = (NtSetTimerResolution_t)
         GetProcAddress(ntdll, "NtSetTimerResolution");
   query                         = (NtQueryTimerResolution_t)
         GetProcAddress(ntdll, "NtQueryTimerResolution");

   if (!rtime_nt_set_timer_resolution || !query)
      goto error;

   /* Note the naming: the "maximum" resolution is the finest the
    * system supports and is therefore the numerically smallest. */
   if (query(&res_coarsest, &res_finest, &res_current) < 0)
      goto error;

   if (desired < res_finest)
      desired = res_finest;
   if (res_coarsest && desired > res_coarsest)
      desired = res_coarsest;

   if (rtime_nt_set_timer_resolution(desired, TRUE, &res_current) < 0)
      goto error;

   rtime_timer_resolution = desired;
   return true;

error:
   rtime_nt_set_timer_resolution = NULL;
   return false;
}

/* See the note above: the kernel request is per process and not
 * refcounted, so it has to be reasserted rather than set once. */
static void rtime_sleep_fallback(unsigned usec)
{
   if (     rtime_sleep_state == RTIME_SLEEP_TIMERES
         && rtime_nt_set_timer_resolution)
   {
      ULONG res_current = 0;
      rtime_nt_set_timer_resolution(rtime_timer_resolution, TRUE,
            &res_current);
   }

   Sleep((usec + 500) / 1000);
}

static void rtime_sleep_init(void)
{
   HMODULE kernel32;
   HANDLE probe = NULL;
   LONG state   = RTIME_SLEEP_FALLBACK;
   DWORD tls    = TLS_OUT_OF_INDEXES;

   if ((kernel32 = GetModuleHandleA("kernel32.dll")))
   {
      rtime_create_waitable_timer_ex = (CreateWaitableTimerExW_t)
            GetProcAddress(kernel32, "CreateWaitableTimerExW");
      rtime_set_waitable_timer       = (SetWaitableTimer_t)
            GetProcAddress(kernel32, "SetWaitableTimer");
   }

   /* Pre-1803 kernels reject the high resolution flag outright with
    * ERROR_INVALID_PARAMETER instead of silently ignoring it, so a
    * successful create is the capability test. Note that a plain
    * (non high resolution) waitable timer is deliberately not used as
    * a fallback: it is no more accurate than Sleep(), and would only
    * add a wait object to the path for nothing. */
   if (rtime_create_waitable_timer_ex && rtime_set_waitable_timer)
   {
      if ((probe = rtime_create_waitable_timer_ex(NULL, NULL,
                  CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                  TIMER_ALL_ACCESS)))
      {
         CloseHandle(probe);

         if ((tls = TlsAlloc()) != TLS_OUT_OF_INDEXES)
         {
            rtime_sleep_tls = tls;
            state           = RTIME_SLEEP_HIGHRES;
         }
      }
   }

   /* No high resolution timer (pre-1803): lower the global timer
    * period instead so that Sleep() is at least millisecond accurate. */
   if (state != RTIME_SLEEP_HIGHRES && rtime_timer_resolution_init())
      state = RTIME_SLEEP_TIMERES;

   InterlockedExchange(&rtime_sleep_state, state);
}

/* The timer object must not be shared between threads: SetWaitableTimer()
 * on a handle that another thread is already waiting on reschedules that
 * thread's wait. One handle per thread, created on first use. */
static HANDLE rtime_sleep_timer_get(void)
{
   HANDLE timer = (HANDLE)TlsGetValue(rtime_sleep_tls);

   if (!timer)
   {
      if (!(timer = rtime_create_waitable_timer_ex(NULL, NULL,
                  CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                  TIMER_ALL_ACCESS)))
         return NULL;

      if (!TlsSetValue(rtime_sleep_tls, timer))
      {
         CloseHandle(timer);
         return NULL;
      }
   }

   return timer;
}

void retro_sleep_us(unsigned usec)
{
   HANDLE timer;
   LARGE_INTEGER due;
   DWORD timeout;

   /* A zero duration means "yield the rest of this time slice", which
    * must not be emulated with a timer. */
   if (!usec)
   {
      Sleep(0);
      return;
   }

   if (rtime_sleep_state == RTIME_SLEEP_UNKNOWN)
   {
      /* First caller performs the probe; any thread that races it
       * takes the plain Sleep() path for this one call rather than
       * spinning on the result. */
      if (InterlockedCompareExchange(&rtime_sleep_init_ran, 1, 0) != 0)
      {
         Sleep((usec + 500) / 1000);
         return;
      }

      rtime_sleep_init();
   }

   if (     rtime_sleep_state != RTIME_SLEEP_HIGHRES
         || !(timer = rtime_sleep_timer_get()))
   {
      rtime_sleep_fallback(usec);
      return;
   }

   /* Negative due time == relative, in 100 ns units */
   due.QuadPart = -((LONGLONG)usec * 10);

   if (!rtime_set_waitable_timer(timer, &due, 0, NULL, NULL, FALSE))
   {
      rtime_sleep_fallback(usec);
      return;
   }

   /* Bounded rather than INFINITE. A timer that was successfully set
    * is guaranteed to signal, but there is no reason for this to be
    * an unbounded wait if it somehow does not. */
   timeout = (usec / 1000) + 100;
   WaitForSingleObject(timer, timeout);
}

void retro_sleep(unsigned msec)
{
   /* Guard the conversion rather than wrapping around. Nothing sleeps
    * for 49 days, but a bogus value must not turn into a short sleep. */
   if (msec > 0xFFFFFFFFU / 1000)
   {
      Sleep(msec);
      return;
   }

   retro_sleep_us(msec * 1000);
}

/* Called from rtime_deinit(), i.e. main thread only, at program or
 * core termination - by which point no other thread may still be
 * calling retro_sleep(). Timer handles belonging to threads that have
 * already exited are reclaimed by the OS. */
static void rtime_sleep_deinit(void)
{
   HANDLE timer;

   if (rtime_nt_set_timer_resolution)
   {
      ULONG res_current = 0;
      rtime_nt_set_timer_resolution(rtime_timer_resolution, FALSE,
            &res_current);
      rtime_nt_set_timer_resolution = NULL;
      rtime_timer_resolution        = 0;
   }

   if (rtime_sleep_tls == TLS_OUT_OF_INDEXES)
   {
      InterlockedExchange(&rtime_sleep_state, RTIME_SLEEP_UNKNOWN);
      InterlockedExchange(&rtime_sleep_init_ran, 0);
      return;
   }

   if ((timer = (HANDLE)TlsGetValue(rtime_sleep_tls)))
   {
      TlsSetValue(rtime_sleep_tls, NULL);
      CloseHandle(timer);
   }

   TlsFree(rtime_sleep_tls);
   rtime_sleep_tls = TLS_OUT_OF_INDEXES;

   InterlockedExchange(&rtime_sleep_state, RTIME_SLEEP_UNKNOWN);
   InterlockedExchange(&rtime_sleep_init_ran, 0);
}
#endif

/* Must be called before using rtime_localtime() */
void rtime_init(void)
{
   rtime_deinit();
#ifdef HAVE_THREADS
   if (!rtime_localtime_lock)
      rtime_localtime_lock = slock_new();
#endif
}

/* Must be called upon program termination */
void rtime_deinit(void)
{
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   rtime_sleep_deinit();
#endif
#ifdef HAVE_THREADS
   if (rtime_localtime_lock)
   {
      slock_free(rtime_localtime_lock);
      rtime_localtime_lock = NULL;
   }
#endif
}

/* Thread-safe wrapper for localtime() */
struct tm *rtime_localtime(const time_t *timep, struct tm *result)
{
   struct tm *time_info = NULL;

   /* Lock mutex */
#ifdef HAVE_THREADS
   slock_lock(rtime_localtime_lock);
#endif

   time_info = localtime(timep);
   if (time_info)
      memcpy(result, time_info, sizeof(struct tm));

   /* Unlock mutex */
#ifdef HAVE_THREADS
   slock_unlock(rtime_localtime_lock);
#endif

   return result;
}
