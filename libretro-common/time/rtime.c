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

#include <retro_posix_source.h>

#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <time/rtime.h>

#if defined(__linux__) || defined(ANDROID) || defined(__APPLE__) \
 || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define RTIME_POSIX_CALENDAR
/* Orbis libc has no readlink(). */
#if !defined(ORBIS)
#define RTIME_POSIX_ZONE_NAME
#endif
#include <unistd.h>
#if defined(ANDROID)
#include <sys/system_properties.h>
#endif
#if defined(__APPLE__) && !defined(IS_SALAMANDER)
/* clock_gettime() arrived with macOS 10.12 and iOS 10: looked up at
 * runtime, gettimeofday() before it. */
#define RTIME_CALENDAR_APPLE
#include <sys/time.h>
#include <dlfcn.h>
#ifndef RTLD_DEFAULT
#define RTLD_DEFAULT ((void*)-2)
#endif
/* Darwin's CLOCK_REALTIME, which older SDKs do not define. */
#define RTIME_APPLE_CLOCK_REALTIME 0
typedef int (*rtime_clock_gettime_t)(int, struct timespec*);
/* Set by rtime_init() before any thread reads it, and never cleared. */
static rtime_clock_gettime_t rtime_apple_clock_gettime = NULL;
#endif
#endif

#if defined(__WINRT__)
#include <windows.h>
#endif

/* MS CRTs keep localtime()'s result per thread. Orbis libc exports
 * localtime_s() in the C11 Annex K order, not localtime_r(), and fills
 * only the nine int fields of a struct tm. Elsewhere localtime_r(),
 * declared here because strict C89 headers hide it. */
#if defined(HAVE_THREADS) && defined(ORBIS)
#define RTIME_HAVE_LOCALTIME_S
struct tm *localtime_s(const time_t *timep, struct tm *result);
#elif defined(HAVE_THREADS) && !defined(_WIN32) && !defined(_XBOX)
#define RTIME_HAVE_LOCALTIME_R
struct tm *localtime_r(const time_t *timep, struct tm *result);
#endif

#if defined(__APPLE__) && defined(__MACH__)
#include <mach/mach_time.h>
#include <sched.h>
#include <stdint.h>

/* Darwin: the sleep is an absolute deadline on the Mach clock, the
 * same clock cpu_features_get_time_usec() reads there, so the time
 * spent between reading it and entering the kernel is not added to the
 * wait as it is by a relative nanosleep(). mach_wait_until() and the
 * timebase have been in libSystem since 10.0. The kernel may still
 * hold a normal-priority thread past the deadline by its timer leeway;
 * what returns is never early, and a caller that needs the deadline
 * itself sleeps short and spins the rest. */
static mach_timebase_info_data_t rtime_mach_tb;

void retro_sleep_us(unsigned usec)
{
   uint64_t ticks;

   /* A zero duration means "yield the rest of this time slice". */
   if (!usec)
   {
      sched_yield();
      return;
   }

   /* The timebase is a constant; a racing first read fills it with
    * the same values, so no guard is needed. */
   if (!rtime_mach_tb.denom)
      mach_timebase_info(&rtime_mach_tb);

   /* Nanoseconds to Mach ticks; the ratio is 1:1 on Intel and
    * 125:3 on Apple silicon, so the product fits for any usec. */
   ticks = (uint64_t)usec * 1000 * rtime_mach_tb.denom / rtime_mach_tb.numer;
   mach_wait_until(mach_absolute_time() + ticks);
}
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
static retro_atomic_int_t rtime_sleep_state =
   RETRO_ATOMIC_INT_INITIALIZER(RTIME_SLEEP_UNKNOWN);
static retro_atomic_int_t rtime_sleep_init_ran =
   RETRO_ATOMIC_INT_INITIALIZER(0);
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
   if (     retro_atomic_load_acquire_int(&rtime_sleep_state)
               == RTIME_SLEEP_TIMERES
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

   /* Release store: rtime_sleep_tls and the resolved entry points are
    * written above and must be visible to any thread that observes the
    * published state. */
   retro_atomic_store_release_int(&rtime_sleep_state, state);
}

/* The timer object must not be shared between threads: SetWaitableTimer()
 * on a handle that another thread is already waiting on reschedules that
 * thread's wait. One handle per thread, created on first use.
 *
 * Handles are process objects, so a thread that exits leaves its timer
 * open until the process does. That is one kernel object per thread
 * that ever slept, which is nothing for a fixed set of worker threads
 * and something to be aware of for a thread created per event: sleep
 * from a persistent thread instead. */
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

   if (retro_atomic_load_acquire_int(&rtime_sleep_state)
         == RTIME_SLEEP_UNKNOWN)
   {
      /* First caller performs the probe; any thread that races it
       * takes the plain Sleep() path for this one call rather than
       * spinning on the result. */
      if (!retro_atomic_cas_int(&rtime_sleep_init_ran, 0, 1))
      {
         Sleep((usec + 500) / 1000);
         return;
      }

      rtime_sleep_init();
   }

   if (     retro_atomic_load_acquire_int(&rtime_sleep_state)
               != RTIME_SLEEP_HIGHRES
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
 * calling retro_sleep(). Only the calling thread's own timer can be
 * closed here: the TLS slot is per thread, so other threads' handles
 * cannot be reached, and they stay open until the process exits. */
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
      retro_atomic_store_release_int(&rtime_sleep_state,
            RTIME_SLEEP_UNKNOWN);
      retro_atomic_store_release_int(&rtime_sleep_init_ran, 0);
      return;
   }

   if ((timer = (HANDLE)TlsGetValue(rtime_sleep_tls)))
   {
      TlsSetValue(rtime_sleep_tls, NULL);
      CloseHandle(timer);
   }

   TlsFree(rtime_sleep_tls);
   rtime_sleep_tls = TLS_OUT_OF_INDEXES;

   retro_atomic_store_release_int(&rtime_sleep_state, RTIME_SLEEP_UNKNOWN);
   retro_atomic_store_release_int(&rtime_sleep_init_ran, 0);
}

#if !defined(IS_SALAMANDER)
/* Windows 8 and later. Set by rtime_init() before any thread reads it,
 * and never cleared. */
typedef VOID (WINAPI *rtime_precise_time_t)(LPFILETIME);
static rtime_precise_time_t rtime_precise_time = NULL;

/* DYNAMIC_TIME_ZONE_INFORMATION, which SDKs hide below Vista. */
typedef struct
{
   LONG Bias;
   WCHAR StandardName[32];
   SYSTEMTIME StandardDate;
   LONG StandardBias;
   WCHAR DaylightName[32];
   SYSTEMTIME DaylightDate;
   LONG DaylightBias;
   WCHAR TimeZoneKeyName[128];
   BOOLEAN DynamicDaylightTimeDisabled;
} rtime_dynamic_tzi_t;

typedef DWORD (WINAPI *rtime_get_dynamic_tzi_t)(rtime_dynamic_tzi_t*);
typedef int32_t (__cdecl *rtime_ucal_zone_id_t)(const uint16_t *winid,
      int32_t len, const char *region, uint16_t *id, int32_t cap,
      int32_t *status);
typedef BOOL (WINAPI *rtime_tzi_for_year_t)(USHORT, rtime_dynamic_tzi_t*,
      LPTIME_ZONE_INFORMATION);
#endif
#endif

/* Must be called at program or core start */
void rtime_init(void)
{
   rtime_deinit();
#if defined(RTIME_CALENDAR_APPLE)
   if (!rtime_apple_clock_gettime)
      rtime_apple_clock_gettime = (rtime_clock_gettime_t)dlsym(
            RTLD_DEFAULT, "clock_gettime");
#endif
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__) \
 && !defined(IS_SALAMANDER)
   if (!rtime_precise_time)
   {
      HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
      if (kernel32)
         rtime_precise_time = (rtime_precise_time_t)GetProcAddress(
               kernel32, "GetSystemTimePreciseAsFileTime");
   }
#endif
}

/* Must be called upon program termination */
void rtime_deinit(void)
{
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   rtime_sleep_deinit();
#endif
}

/* Thread-safe wrapper for localtime() */
struct tm *rtime_localtime(const time_t *timep, struct tm *result)
{
#if defined(RTIME_HAVE_LOCALTIME_R)
   /* Through a copy: a failed conversion leaves result unwritten. */
   struct tm tm_buf;
   struct tm *time_info = localtime_r(timep, &tm_buf);
#elif defined(RTIME_HAVE_LOCALTIME_S)
   struct tm tm_buf;
   struct tm *time_info;
   memset(&tm_buf, 0, sizeof(tm_buf));
   time_info = localtime_s(timep, &tm_buf);
#else
   struct tm *time_info = localtime(timep);
#endif
   if (time_info)
      memcpy(result, time_info, sizeof(struct tm));
   return result;
}

#if !defined(IS_SALAMANDER)
bool rtime_calendar_now(int64_t *sec, int32_t *nsec)
{
#if defined(RTIME_CALENDAR_APPLE)
   struct timeval tv;
   if (rtime_apple_clock_gettime)
   {
      struct timespec ts;
      if (rtime_apple_clock_gettime(RTIME_APPLE_CLOCK_REALTIME, &ts))
         return false;
      *sec  = (int64_t)ts.tv_sec;
      *nsec = (int32_t)ts.tv_nsec;
      return true;
   }
   if (gettimeofday(&tv, NULL))
      return false;
   *sec  = (int64_t)tv.tv_sec;
   *nsec = (int32_t)tv.tv_usec * 1000;
   return true;
#elif defined(RTIME_POSIX_CALENDAR)
   struct timespec ts;
   if (clock_gettime(CLOCK_REALTIME, &ts))
      return false;
   *sec  = (int64_t)ts.tv_sec;
   *nsec = (int32_t)ts.tv_nsec;
   return true;
#elif defined(_WIN32) && !defined(_XBOX)
   FILETIME ft;
   int64_t t;
#if defined(__WINRT__)
   GetSystemTimePreciseAsFileTime(&ft);
#else
   if (rtime_precise_time)
      rtime_precise_time(&ft);
   else
      GetSystemTimeAsFileTime(&ft);
#endif
   /* 100 ns units since 1601 */
   t     = (int64_t)(((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime)
         - (int64_t)116444736 * 1000000000;
   *sec  = t / 10000000;
   *nsec = (int32_t)(t % 10000000) * 100;
   return true;
#else
   time_t t = time(NULL);
   if (t == (time_t)-1)
      return false;
   *sec  = (int64_t)t;
   *nsec = 0;
   return true;
#endif
}

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
/* The UTC year of sec */
static int64_t rtime_year_of(int64_t sec)
{
   int64_t  y;
   unsigned m, d;
   rtime_civil_from_days((sec >= 0 ? sec : sec - 86399) / 86400, &y, &m, &d);
   return y;
}
#endif

/* Reloads the host zone, so a change since the last load is seen. */
static void rtime_tzset(void)
{
#if defined(RTIME_POSIX_CALENDAR)
   tzset();
#elif defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   _tzset();
#endif
}

/* The local fields at sec against sec itself; false if the conversion
 * fails. */
static bool rtime_local_offset(int64_t sec, int32_t *offset, int *isdst)
{
   struct tm tm;
   time_t t = (time_t)sec;
   /* A time_t too narrow for sec would convert another date */
   if ((int64_t)t != sec)
      return false;
   memset(&tm, 0, sizeof(tm));
   rtime_localtime(&t, &tm);
   if (!tm.tm_mday)
      return false;
   *offset = (int32_t)(rtime_days_from_civil(tm.tm_year + 1900,
            tm.tm_mon + 1, tm.tm_mday) * 86400
         + tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec - sec);
   *isdst  = tm.tm_isdst;
   return true;
}

int32_t rtime_utc_offset(int64_t sec)
{
   int32_t offset = 0;
   int isdst;
   rtime_tzset();
   rtime_local_offset(sec, &offset, &isdst);
   return offset;
}

int32_t rtime_standard_offset(int64_t sec)
{
   int32_t offset;
   int32_t standard = 0;
   int32_t low      = 0;
   bool has_std     = false;
   bool has_low     = false;
   int isdst;
   int k;
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   const char *tz = getenv("TZ");
   if (!tz || !*tz)
   {
      TIME_ZONE_INFORMATION tzi;
      int64_t year = rtime_year_of(sec);
      HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
      /* Vista SP1 and later: the rules of sec's year */
      rtime_tzi_for_year_t for_year = kernel32
            ? (rtime_tzi_for_year_t)GetProcAddress(kernel32,
               "GetTimeZoneInformationForYear")
            : NULL;
      bool got = false;
      if (for_year && year >= 1601 && year <= 30827)
         got = for_year((USHORT)year, NULL, &tzi) ? true : false;
      if (!got)
         got = GetTimeZoneInformation(&tzi) != TIME_ZONE_ID_INVALID;
      if (got)
      {
         LONG bias = tzi.StandardBias;
         /* A DST below standard time counts as the standard. */
         if (tzi.DaylightDate.wMonth && tzi.DaylightBias > bias)
            bias = tzi.DaylightBias;
         return (int32_t)(-(tzi.Bias + bias) * 60);
      }
   }
#endif
   rtime_tzset();
   /* Standard time at sec, else at the first 30-day step after it;
    * a DST below it (Europe/Dublin in winter) takes its place. */
   for (k = 0; k <= 12; k++)
   {
      if (!rtime_local_offset(sec + (int64_t)k * 2592000, &offset, &isdst))
         continue;
      if (!isdst)
      {
         if (!has_std)
            standard = offset;
         has_std = true;
      }
      else if (!has_low || offset < low)
      {
         low     = offset;
         has_low = true;
      }
   }
   if (!has_std)
      standard = rtime_utc_offset(sec);
   if (has_low && low < standard)
      return low;
   return standard;
}

#if defined(RTIME_POSIX_ZONE_NAME)
/* A name without a leading posix/ or right/, of IANA's characters
 * only, which also keeps out "..". */
static size_t rtime_zone_name_put(char *s, size_t len, const char *name)
{
   size_t i;
   if (!strncmp(name, "posix/", 6) || !strncmp(name, "right/", 6))
      name += 6;
   for (i = 0; name[i]; i++)
   {
      char c = name[i];
      if (!(   (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
            || (c >= '0' && c <= '9')
            || c == '_' || c == '+' || c == '-' || c == '/'))
         return 0;
   }
   if (!i || i >= len)
      return 0;
   memcpy(s, name, i + 1);
   return i;
}

/* The part after zoneinfo/; a path without it is followed one link. */
static size_t rtime_zone_name_path(char *s, size_t len, const char *path)
{
   char link[256];
   ssize_t n;
   const char *part = strstr(path, "zoneinfo/");
   if (!part)
   {
      n = readlink(path, link, sizeof(link));
      if (n <= 0 || n >= (ssize_t)sizeof(link))
         return 0;
      link[n] = '\0';
      if (!(part = strstr(link, "zoneinfo/")))
         return 0;
   }
   return rtime_zone_name_put(s, len, part + 9);
}
#endif

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
/* The zone's registry key name, mapped to IANA by the icu.dll that
 * Windows 10 1903 and later ship. */
static size_t rtime_zone_name_windows(char *s, size_t len)
{
   /* The last key and the name ICU gave it: icu.dll is loaded again
    * only when the zone changes. */
   static WCHAR last_key[128];
   static char last_name[129];
   static size_t last_len = 0;
   rtime_dynamic_tzi_t tzi;
   uint16_t id[128];
   rtime_get_dynamic_tzi_t get_tzi;
   rtime_ucal_zone_id_t zone_id;
   HMODULE kernel32;
   HMODULE icu;
   int32_t status = 0;
   int32_t n      = 0;
   int32_t keylen;
   int32_t i;

   if (     !(kernel32 = GetModuleHandleA("kernel32.dll"))
         || !(get_tzi  = (rtime_get_dynamic_tzi_t)GetProcAddress(kernel32,
               "GetDynamicTimeZoneInformation")))
      return 0;
   memset(&tzi, 0, sizeof(tzi));
   if (get_tzi(&tzi) == TIME_ZONE_ID_INVALID)
      return 0;
   for (keylen = 0; keylen < 128 && tzi.TimeZoneKeyName[keylen]; keylen++)
      ;
   if (!keylen || keylen >= 128)
      return 0;

   if (memcmp(last_key, tzi.TimeZoneKeyName, (keylen + 1) * sizeof(WCHAR)))
   {
      last_len     = 0;
      last_name[0] = '\0';
      if ((icu = LoadLibraryExW(L"icu.dll", NULL,
                  0x00000800 /* LOAD_LIBRARY_SEARCH_SYSTEM32 */)))
      {
         if ((zone_id = (rtime_ucal_zone_id_t)GetProcAddress(icu,
                     "ucal_getTimeZoneIDForWindowsID")))
            n = zone_id((const uint16_t*)tzi.TimeZoneKeyName, keylen, NULL,
                  id, 128, &status);
         FreeLibrary(icu);
      }
      if (status <= 0 && n > 0 && n <= 128)
      {
         for (i = 0; i < n && id[i] <= 0x7E; i++)
            last_name[i] = (char)id[i];
         if (i == n)
         {
            last_name[n] = '\0';
            last_len     = (size_t)n;
         }
         else
            last_name[0] = '\0';
      }
      memcpy(last_key, tzi.TimeZoneKeyName, (keylen + 1) * sizeof(WCHAR));
   }

   if (!last_len || last_len >= len)
      return 0;
   memcpy(s, last_name, last_len + 1);
   return last_len;
}
#endif

size_t rtime_zone_name(char *s, size_t len)
{
#if defined(RTIME_POSIX_ZONE_NAME)
   const char *tz;
#if defined(ANDROID)
   char prop[PROP_VALUE_MAX];
#endif
#elif defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   const char *tz;
#endif
   if (!s || !len)
      return 0;
   s[0] = '\0';
#if defined(RTIME_POSIX_ZONE_NAME)
   /* libc follows a TZ that is set instead of the host zone: an empty
    * one is UTC, and one that is not a name is a rule with no name. */
   if ((tz = getenv("TZ")))
   {
      if (!*tz)
         return rtime_zone_name_put(s, len, "UTC");
      if (*tz == ':')
         tz++;
      if (*tz == '/')
         return rtime_zone_name_path(s, len, tz);
      if (!strchr(tz, '/') && strcmp(tz, "UTC"))
         return 0;
      return rtime_zone_name_put(s, len, tz);
   }
#if defined(ANDROID)
   if (__system_property_get("persist.sys.timezone", prop) <= 0)
      return 0;
   return rtime_zone_name_put(s, len, prop);
#else
   return rtime_zone_name_path(s, len, "/etc/localtime");
#endif
#elif defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   /* The CRT follows a TZ that is set, not the Windows zone. */
   if ((tz = getenv("TZ")) && *tz)
      return 0;
   return rtime_zone_name_windows(s, len);
#else
   return 0;
#endif
}
#endif
