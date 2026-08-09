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

enum rtime_sleep_state
{
   RTIME_SLEEP_UNKNOWN = 0,
   RTIME_SLEEP_FALLBACK,
   RTIME_SLEEP_HIGHRES
};

static CreateWaitableTimerExW_t rtime_create_waitable_timer_ex = NULL;
static SetWaitableTimer_t rtime_set_waitable_timer             = NULL;
static volatile LONG rtime_sleep_state    = RTIME_SLEEP_UNKNOWN;
static volatile LONG rtime_sleep_init_ran = 0;
static DWORD rtime_sleep_tls              = TLS_OUT_OF_INDEXES;

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
      Sleep((usec + 500) / 1000);
      return;
   }

   /* Negative due time == relative, in 100 ns units */
   due.QuadPart = -((LONGLONG)usec * 10);

   if (!rtime_set_waitable_timer(timer, &due, 0, NULL, NULL, FALSE))
   {
      Sleep((usec + 500) / 1000);
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

   if (rtime_sleep_tls == TLS_OUT_OF_INDEXES)
      return;

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
