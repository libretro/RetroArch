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

#ifndef __FRONTEND_THREAD_ELEVATION_H
#define __FRONTEND_THREAD_ELEVATION_H

#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Scheduling a thread ahead of ordinary ones, through whichever of the
 * mechanisms built in will grant it. The mechanisms are backends in one
 * ordered list, so a caller asks once and never names one.
 *
 * A self-acting backend can only change the thread that calls it, and
 * answers there and then. A brokered backend acts on a thread id from
 * any thread, and may answer PENDING: it has started the request on a
 * thread of its own and returned, and nothing waits for it. Every
 * self-acting backend comes before every brokered one in the list, so
 * a brokered backend that is refused on its own thread carries on with
 * thread_elevation_continue(), which only ever has brokered backends
 * left to try. How a request ended is logged by the backend that ended
 * it; callers are told only what was known when they asked.
 *
 * An additive backend is self-acting and complements the rest rather
 * than standing in for them - a scheduler hint, not a priority. Every
 * additive backend is tried first, whatever it answers the chain goes
 * on, and a continuation never reaches one. */

enum thread_elevation_result
{
   THREAD_ELEVATION_REFUSED = 0,
   THREAD_ELEVATION_GRANTED,
   THREAD_ELEVATION_PENDING
};

typedef struct thread_elevation_backend
{
   /* 'tid' names the thread for a brokered backend and is ignored by a
    * self-acting one. 'next' is where the chain resumes should a
    * PENDING request be refused. */
   enum thread_elevation_result (*raise)(uint64_t tid, unsigned next);
   /* Names the backend in the caller's log line: while it is pending,
    * or, for an additive one, once it has applied. */
   const char *ident;
   bool brokered;
   bool additive;
} thread_elevation_backend_t;

/**
 * Applies every additive backend to the calling thread, then raises it
 * through the first backend that grants it. Returns at once. On
 * PENDING, *pending_via (if given) is set to the ident of the backend
 * now working on it; *added_via (if given) is set to the ident of an
 * additive backend that applied, or NULL.
 */
enum thread_elevation_result thread_elevation_raise_current(
      const char **pending_via, const char **added_via);

/**
 * For a brokered backend refused after answering PENDING: tries the
 * brokered backends from 'next' on, for the same thread.
 */
void thread_elevation_continue(uint64_t tid, unsigned next);

RETRO_END_DECLS

#endif
