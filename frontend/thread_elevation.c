/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <compat/msvc.h>
#include <rthreads/rthreads.h>

#include "thread_elevation.h"
#include "../gfx/common/dbus_runtime.h"

#if defined(__linux__) && !defined(_WIN32)
#include <errno.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/syscall.h>
/* The prototype every Linux libc uses; spelled out so a strict C89
 * build, where glibc hides it, sees the same one. */
extern long syscall(long number, ...);
#endif

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#include <windows.h>
#define THREAD_ELEVATION_HAVE_MMCSS
#endif

#if defined(__APPLE__) && defined(__MACH__)
#include <dlfcn.h>
#endif

enum thread_elevation_state
{
   THREAD_ELEVATION_STATE_OFF = 0,
   THREAD_ELEVATION_STATE_REFUSED,
   THREAD_ELEVATION_STATE_PENDING,
   THREAD_ELEVATION_STATE_RAISED,
   THREAD_ELEVATION_STATE_MMCSS
};

/* One per slot, then the power plan: 0 off, 1 on, 2 on with idle
 * states held off. Each written by one thread, read by the overlay. */
static retro_atomic_int_t thread_elevation_states[THREAD_ELEVATION_SLOT_COUNT + 1] = {
   RETRO_ATOMIC_INT_INITIALIZER(0),
   RETRO_ATOMIC_INT_INITIALIZER(0),
   RETRO_ATOMIC_INT_INITIALIZER(0),
   RETRO_ATOMIC_INT_INITIALIZER(0)
};

enum thread_elevation_token_kind
{
   THREAD_ELEVATION_TOKEN_NONE = 0,
   THREAD_ELEVATION_TOKEN_MMCSS,
   THREAD_ELEVATION_TOKEN_WIN32_PRIORITY,
   THREAD_ELEVATION_TOKEN_NICE,
   THREAD_ELEVATION_TOKEN_QOS
};

/* In the order they are tried; every self-acting backend first. A
 * harness that brings its own list defines this macro and the array. */
#ifdef THREAD_ELEVATION_BACKENDS_EXTERNAL
extern const thread_elevation_backend_t *thread_elevation_backends[];
#else
/* The kernel, asked directly: rthreads knows each platform's class. */
static enum thread_elevation_result thread_elevation_rthreads_raise(
      uint64_t tid, unsigned next)
{
   (void)tid;
   (void)next;
   return sthread_raise_current_priority()
      ? THREAD_ELEVATION_GRANTED : THREAD_ELEVATION_REFUSED;
}

static const thread_elevation_backend_t thread_elevation_rthreads = {
   thread_elevation_rthreads_raise,
   "rthreads",
   false,
   false
};

#if defined(RARCH_HAVE_DBUS_RUNTIME) && defined(HAVE_THREADS) && defined(__linux__)
extern const thread_elevation_backend_t thread_elevation_rtkit;
#endif
#if defined(__linux__) && !defined(__ANDROID__) && !defined(_WIN32)
extern const thread_elevation_backend_t thread_elevation_eevdf;
#endif

static const thread_elevation_backend_t *thread_elevation_backends[] = {
#if defined(__linux__) && !defined(__ANDROID__) && !defined(_WIN32)
   &thread_elevation_eevdf,
#endif
   &thread_elevation_rthreads,
#if defined(RARCH_HAVE_DBUS_RUNTIME) && defined(HAVE_THREADS) && defined(__linux__)
   &thread_elevation_rtkit,
#endif
   NULL
};
#endif

/* The id a brokered backend is handed; only asked for once one is
 * reached. */
static uint64_t thread_elevation_current_tid(void)
{
#if defined(__linux__) && !defined(_WIN32)
   return (uint64_t)syscall(SYS_gettid);
#else
   return 0;
#endif
}

#ifdef THREAD_ELEVATION_HAVE_MMCSS
/* avrt.dll exists from Vista on, so it is resolved, not linked. */
static bool thread_elevation_mmcss(const wchar_t *task,
      thread_elevation_token_t *token)
{
   typedef HANDLE (WINAPI *av_set_t)(LPCWSTR, LPDWORD);
   DWORD idx    = 0;
   HANDLE h     = NULL;
   av_set_t set;
   HMODULE avrt = LoadLibraryA("avrt.dll");

   if (!avrt)
      return false;
   if ((set = (av_set_t)GetProcAddress(avrt,
               "AvSetMmThreadCharacteristicsW")))
      h = set(task, &idx);
   if (!h || h == INVALID_HANDLE_VALUE)
   {
      FreeLibrary(avrt);
      return false;
   }
   token->module = avrt;
   token->handle = h;
   token->kind   = THREAD_ELEVATION_TOKEN_MMCSS;
   return true;
}
#endif

#if defined(__APPLE__) && defined(__MACH__) && defined(RTLD_DEFAULT)
/* pthread_set_qos_class_self_np arrived with 10.10: looked up, so one
 * binary still runs on older releases. */
static int thread_elevation_set_qos(unsigned qos_class)
{
   int (*set)(unsigned, int) = NULL;
   *(void**)&set = dlsym(RTLD_DEFAULT, "pthread_set_qos_class_self_np");
   return set ? set(qos_class, 0) : -1;
}
#endif

/* Above ordinary threads and never real time, on the systems where
 * that is the thread's own to ask for; the consoles keep their
 * defaults, since the core's own threads share a few cores there. */
static enum thread_elevation_result thread_elevation_games_raise(
      thread_elevation_token_t *token)
{
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   int prev = GetThreadPriority(GetCurrentThread());
   if (     prev == THREAD_PRIORITY_ERROR_RETURN
         || !SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST))
      return THREAD_ELEVATION_REFUSED;
   token->kind = THREAD_ELEVATION_TOKEN_WIN32_PRIORITY;
   token->prev = prev;
   return THREAD_ELEVATION_GRANTED;
#elif defined(__linux__)
   /* -4 is Android's display band, which an app may give its own
    * threads; elsewhere as far as RLIMIT_NICE allows, up to -5. */
#if defined(__ANDROID__)
   int want  = -4;
#else
   int want  = -5;
   struct rlimit rl;
#endif
   id_t tid  = (id_t)syscall(SYS_gettid);
   int prev;

   errno = 0;
   prev  = getpriority(PRIO_PROCESS, tid);
   if (errno || want >= prev)
      return THREAD_ELEVATION_REFUSED;
   if (setpriority(PRIO_PROCESS, tid, want) != 0)
   {
#if defined(__ANDROID__)
      return THREAD_ELEVATION_REFUSED;
#else
      /* Without CAP_SYS_NICE, as low as RLIMIT_NICE allows */
      if (     getrlimit(RLIMIT_NICE, &rl) != 0
            || rl.rlim_cur == RLIM_INFINITY
            || (want = 20 - (int)rl.rlim_cur) >= prev
            || setpriority(PRIO_PROCESS, tid, want) != 0)
         return THREAD_ELEVATION_REFUSED;
#endif
   }
   token->kind = THREAD_ELEVATION_TOKEN_NICE;
   token->prev = prev;
   return THREAD_ELEVATION_GRANTED;
#elif defined(__APPLE__) && defined(__MACH__) && defined(RTLD_DEFAULT)
   /* QOS_CLASS_USER_INTERACTIVE */
   if (thread_elevation_set_qos(0x21) != 0)
      return THREAD_ELEVATION_REFUSED;
   token->kind = THREAD_ELEVATION_TOKEN_QOS;
   return THREAD_ELEVATION_GRANTED;
#else
   (void)token;
   return THREAD_ELEVATION_REFUSED;
#endif
}

void thread_elevation_lower_current(thread_elevation_token_t *token)
{
   if (!token)
      return;

   switch (token->kind)
   {
#ifdef THREAD_ELEVATION_HAVE_MMCSS
      case THREAD_ELEVATION_TOKEN_MMCSS:
         {
            typedef BOOL (WINAPI *av_revert_t)(HANDLE);
            av_revert_t revert = (av_revert_t)GetProcAddress(
                  (HMODULE)token->module, "AvRevertMmThreadCharacteristics");
            if (revert)
               revert((HANDLE)token->handle);
            FreeLibrary((HMODULE)token->module);
         }
         break;
#endif
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
      case THREAD_ELEVATION_TOKEN_WIN32_PRIORITY:
         SetThreadPriority(GetCurrentThread(), token->prev);
         break;
#endif
#if defined(__linux__) && !defined(_WIN32)
      case THREAD_ELEVATION_TOKEN_NICE:
         setpriority(PRIO_PROCESS, (id_t)syscall(SYS_gettid), token->prev);
         break;
#endif
#if defined(__APPLE__) && defined(__MACH__) && defined(RTLD_DEFAULT)
      case THREAD_ELEVATION_TOKEN_QOS:
         /* QOS_CLASS_DEFAULT */
         thread_elevation_set_qos(0x15);
         break;
#endif
      default:
         break;
   }

   token->module = NULL;
   token->handle = NULL;
   token->kind   = THREAD_ELEVATION_TOKEN_NONE;
   token->prev   = 0;
}

enum thread_elevation_result thread_elevation_raise_current(
      enum thread_elevation_task task, thread_elevation_token_t *token,
      const char **pending_via, const char **added_via)
{
   unsigned i;
   uint64_t tid   = 0;
   bool have_tid  = false;

   if (added_via)
      *added_via = NULL;
   if (token && token->kind != THREAD_ELEVATION_TOKEN_NONE)
      return THREAD_ELEVATION_GRANTED;

   if (task == THREAD_ELEVATION_TASK_GAMES)
   {
      thread_elevation_token_t scratch;
      if (!token)
         token = &scratch;
#ifdef THREAD_ELEVATION_HAVE_MMCSS
      if (thread_elevation_mmcss(L"Games", token))
         return THREAD_ELEVATION_GRANTED;
#endif
      return thread_elevation_games_raise(token);
   }

   /* Additive backends first: each goes on top of whatever the chain
    * grants, so none of them ends it. */
   for (i = 0; thread_elevation_backends[i]; i++)
   {
      const thread_elevation_backend_t *b = thread_elevation_backends[i];
      if (     b->additive && !b->brokered
            && b->raise(0, i + 1) == THREAD_ELEVATION_GRANTED
            && added_via)
         *added_via = b->ident;
   }

#ifdef THREAD_ELEVATION_HAVE_MMCSS
   /* The class carries its own priority, so the chain is not added on
    * top of it. */
   if (token && thread_elevation_mmcss(L"Pro Audio", token))
      return THREAD_ELEVATION_GRANTED;
#endif

   for (i = 0; thread_elevation_backends[i]; i++)
   {
      const thread_elevation_backend_t *b = thread_elevation_backends[i];
      enum thread_elevation_result r;

      if (b->additive)
         continue;
      if (b->brokered && !have_tid)
      {
         tid      = thread_elevation_current_tid();
         have_tid = true;
      }
      r = b->raise(tid, i + 1);
      if (r == THREAD_ELEVATION_GRANTED)
         return r;
      if (r == THREAD_ELEVATION_PENDING)
      {
         if (pending_via)
            *pending_via = b->ident;
         return r;
      }
   }
   return THREAD_ELEVATION_REFUSED;
}

void thread_elevation_continue(uint64_t tid, unsigned next)
{
   unsigned i;

   for (i = 0; thread_elevation_backends[i]; i++)
   {
      const thread_elevation_backend_t *b = thread_elevation_backends[i];
      if (i < next || !b->brokered)
         continue;
      if (b->raise(tid, i + 1) != THREAD_ELEVATION_REFUSED)
         return;
   }
}

void thread_elevation_note(enum thread_elevation_slot slot,
      enum thread_elevation_result result,
      const thread_elevation_token_t *token, bool asked)
{
   int state = THREAD_ELEVATION_STATE_OFF;

   if (asked)
   {
      if (result == THREAD_ELEVATION_PENDING)
         state = THREAD_ELEVATION_STATE_PENDING;
      else if (result != THREAD_ELEVATION_GRANTED)
         state = THREAD_ELEVATION_STATE_REFUSED;
      else if (token && token->kind == THREAD_ELEVATION_TOKEN_MMCSS)
         state = THREAD_ELEVATION_STATE_MMCSS;
      else
         state = THREAD_ELEVATION_STATE_RAISED;
   }
   retro_atomic_store_release_int(&thread_elevation_states[slot], state);
}

void thread_elevation_note_power_plan(bool active, bool idle_disable)
{
   retro_atomic_store_release_int(
         &thread_elevation_states[THREAD_ELEVATION_SLOT_COUNT],
         active ? (idle_disable ? 2 : 1) : 0);
}

size_t thread_elevation_status(char *s, size_t len, const char *device)
{
   /* -: not asked, no: refused, ask: a broker is still on it */
   static const char *const names[] = { "-", "no", "ask", "high", "MMCSS" };
   static const char *const plans[] = { "", ", plan", ", plan+idle" };
   int st[THREAD_ELEVATION_SLOT_COUNT];
   int plan;
   int ret;
   unsigned i;
   bool any;

   plan = retro_atomic_load_acquire_int(
         &thread_elevation_states[THREAD_ELEVATION_SLOT_COUNT]);
   if (plan < 0 || plan > 2)
      plan = 0;
   any  = plan != 0 || (device && strcmp(device, "-"));
   for (i = 0; i < THREAD_ELEVATION_SLOT_COUNT; i++)
   {
      st[i] = retro_atomic_load_acquire_int(&thread_elevation_states[i]);
      if (st[i] < 0 || st[i] > THREAD_ELEVATION_STATE_MMCSS)
         st[i] = THREAD_ELEVATION_STATE_OFF;
      any  |= st[i] != THREAD_ELEVATION_STATE_OFF;
   }
   if (!any || !len)
      return 0;

   ret = snprintf(s, len, " Priority:   main %s, video %s, audio %s%s%s%s\n",
         names[st[THREAD_ELEVATION_SLOT_MAIN]],
         names[st[THREAD_ELEVATION_SLOT_VIDEO]],
         names[st[THREAD_ELEVATION_SLOT_AUDIO]],
         device ? ", device " : "",
         device ? device : "",
         plans[plan]);
   if (ret < 0)
   {
      s[0] = '\0';
      return 0;
   }
   return (size_t)ret < len ? (size_t)ret : len - 1;
}
