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

#ifndef RARCH_D3D11_TURNS_H__
#define RARCH_D3D11_TURNS_H__

#include <windows.h>
#include <boolean.h>
#include <retro_atomic.h>
#include <retro_inline.h>
#ifdef HAVE_THREADS
#include <rthreads/retro_eventcount.h>
#endif

/* libretro_d3d11.h version 2's lock on the immediate context, taken in
 * turns. A Windows critical section is not fair: a core that unlocks and
 * locks again at once, thousands of times a frame, usually gets it over a
 * frontend that has been waiting. So a core's outermost lock gives way
 * to a waiting frontend: it parks on an eventcount until the frontend it
 * saw is in.
 *
 * `frontend` holds the frontends waiting in its low byte and how many
 * have got in above it. The park and a frontend's getting in are a
 * Dekker pair on it and `core_parked`, so a parked core is always woken
 * and a frontend notifies only when one is parked. Without threads no
 * frontend presents from another thread, and the core does not give
 * way. */
#define D3D11_TURNS_WAITING 0xff
#define D3D11_TURNS_ENTRY   0x100

typedef struct d3d11_turns
{
   CRITICAL_SECTION   lock;
#ifdef HAVE_THREADS
   retro_eventcount_t frontend_in;
#endif
   retro_atomic_int_t frontend;         /* entries << 8 | waiting */
   retro_atomic_int_t core_parked;
   unsigned           core_depth;       /* the core thread's own */
   bool               fair;             /* the core gives way */
} d3d11_turns_t;

static INLINE void d3d11_turns_init(d3d11_turns_t *t)
{
   InitializeCriticalSection(&t->lock);
   retro_atomic_int_init(&t->frontend, 0);
   retro_atomic_int_init(&t->core_parked, 0);
   t->core_depth = 0;
#ifdef HAVE_THREADS
   t->fair       = retro_eventcount_init(&t->frontend_in);
#else
   t->fair       = false;
#endif
}

static INLINE void d3d11_turns_deinit(d3d11_turns_t *t)
{
#ifdef HAVE_THREADS
   if (t->fair)
      retro_eventcount_free(&t->frontend_in);
#endif
   t->fair = false;
   DeleteCriticalSection(&t->lock);
}

static INLINE void d3d11_turns_frontend_enter(d3d11_turns_t *t)
{
   retro_atomic_fetch_add_seq_cst_int(&t->frontend, 1);
   EnterCriticalSection(&t->lock);
   retro_atomic_fetch_add_seq_cst_int(&t->frontend, D3D11_TURNS_ENTRY - 1);
#ifdef HAVE_THREADS
   if (retro_atomic_load_seq_cst_int(&t->core_parked))
      retro_eventcount_notify(&t->frontend_in);
#endif
}

static INLINE void d3d11_turns_frontend_leave(d3d11_turns_t *t)
{
   LeaveCriticalSection(&t->lock);
}

/* True once a frontend has got in since the core read @seen */
#define D3D11_TURNS_ENTERED(t, seen) \
   ((retro_atomic_load_seq_cst_int(&(t)->frontend) & ~D3D11_TURNS_WAITING) \
    != ((seen) & ~D3D11_TURNS_WAITING))

static INLINE void d3d11_turns_core_enter(d3d11_turns_t *t)
{
#if defined(HAVE_THREADS) && !defined(D3D11_TURNS_TEST_UNFAIR)
   if (!t->core_depth && t->fair)
   {
      int seen = retro_atomic_load_seq_cst_int(&t->frontend);
      if (seen & D3D11_TURNS_WAITING)
      {
         retro_atomic_fetch_add_seq_cst_int(&t->core_parked, 1);
         while (!D3D11_TURNS_ENTERED(t, seen))
         {
            int key = retro_eventcount_prepare_wait(&t->frontend_in);
            if (D3D11_TURNS_ENTERED(t, seen))
            {
               retro_eventcount_cancel_wait(&t->frontend_in);
               break;
            }
            retro_eventcount_commit_wait(&t->frontend_in, key);
         }
         retro_atomic_fetch_add_seq_cst_int(&t->core_parked, -1);
      }
   }
#endif
   EnterCriticalSection(&t->lock);
   t->core_depth++;
}

static INLINE void d3d11_turns_core_leave(d3d11_turns_t *t)
{
   if (t->core_depth)
      t->core_depth--;
   LeaveCriticalSection(&t->lock);
}
#endif
