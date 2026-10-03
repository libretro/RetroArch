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
#include <stdint.h>

#include <boolean.h>
#include <rthreads/rthreads.h>

#include "thread_elevation.h"
#include "../gfx/common/dbus_runtime.h"

#if defined(__linux__) && !defined(_WIN32)
#include <unistd.h>
#include <sys/syscall.h>
/* The prototype every Linux libc uses; spelled out so a strict C89
 * build, where glibc hides it, sees the same one. */
extern long syscall(long number, ...);
#endif

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

enum thread_elevation_result thread_elevation_raise_current(
      const char **pending_via, const char **added_via)
{
   unsigned i;
   uint64_t tid   = 0;
   bool have_tid  = false;

   if (added_via)
      *added_via = NULL;

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
