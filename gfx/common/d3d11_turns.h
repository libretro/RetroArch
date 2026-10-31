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

/* libretro_d3d11.h version 2's lock on the immediate context, taken in
 * turns. A Windows critical section is not fair: a core that unlocks and
 * locks again at once, thousands of times a frame, usually gets it over a
 * frontend that has been waiting. So a core's outermost lock gives way
 * to a waiting frontend: it parks on an event the frontend sets once it
 * is in. The park and the frontend's leaving the queue are a Dekker
 * pair, so a core that parks is always woken, and a frontend sets the
 * event only when a core is parked. */
typedef struct d3d11_turns
{
   CRITICAL_SECTION   lock;
   HANDLE             frontend_in;      /* auto-reset */
   retro_atomic_int_t frontend_waiting;
   retro_atomic_int_t core_parked;
   unsigned           core_depth;       /* the core thread's own */
} d3d11_turns_t;

static INLINE void d3d11_turns_init(d3d11_turns_t *t)
{
   InitializeCriticalSection(&t->lock);
   /* Without the event a core does not give way; it never spins. */
   t->frontend_in = CreateEventA(NULL, FALSE, FALSE, NULL);
   retro_atomic_int_init(&t->frontend_waiting, 0);
   retro_atomic_int_init(&t->core_parked, 0);
   t->core_depth  = 0;
}

static INLINE void d3d11_turns_deinit(d3d11_turns_t *t)
{
   if (t->frontend_in)
      CloseHandle(t->frontend_in);
   t->frontend_in = NULL;
   DeleteCriticalSection(&t->lock);
}

static INLINE void d3d11_turns_frontend_enter(d3d11_turns_t *t)
{
   retro_atomic_fetch_add_seq_cst_int(&t->frontend_waiting, 1);
   EnterCriticalSection(&t->lock);
   retro_atomic_fetch_add_seq_cst_int(&t->frontend_waiting, -1);
   if (retro_atomic_load_seq_cst_int(&t->core_parked) && t->frontend_in)
      SetEvent(t->frontend_in);
}

static INLINE void d3d11_turns_frontend_leave(d3d11_turns_t *t)
{
   LeaveCriticalSection(&t->lock);
}

static INLINE void d3d11_turns_core_enter(d3d11_turns_t *t)
{
   if (!t->core_depth && t->frontend_in)
   {
      retro_atomic_fetch_add_seq_cst_int(&t->core_parked, 1);
#ifndef D3D11_TURNS_TEST_UNFAIR
      if (retro_atomic_load_seq_cst_int(&t->frontend_waiting))
         WaitForSingleObject(t->frontend_in, INFINITE);
#endif
      retro_atomic_fetch_add_seq_cst_int(&t->core_parked, -1);
   }
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
