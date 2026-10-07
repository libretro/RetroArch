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

#include <stddef.h>
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

enum thread_elevation_task
{
   /* A thread that wakes for each device period and sleeps again: the
    * backend chain, real time where it is granted. */
   THREAD_ELEVATION_TASK_AUDIO = 0,
   /* A thread that may run flat out, like the main or video thread:
    * ahead of ordinary threads, never real time, which would starve
    * the system or get the thread killed for running too long. */
   THREAD_ELEVATION_TASK_GAMES
};

/* What a raise changed on the calling thread, so it can be undone.
 * Zeroed before the first raise; a raise through a token that already
 * holds one changes nothing. */
typedef struct thread_elevation_token
{
   void *module;
   void *handle;
   int   kind;
   int   prev;
} thread_elevation_token_t;

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
 * For TASK_AUDIO: applies every additive backend to the calling thread,
 * then raises it through the first backend that grants it; on Windows
 * the MMCSS Pro Audio class comes first. For TASK_GAMES: the MMCSS
 * Games class on Windows, else a priority above normal, never the
 * chain. Returns at once. On PENDING, *pending_via (if given) is set to
 * the ident of the backend now working on it; *added_via (if given) is
 * set to the ident of an additive backend that applied, or NULL. What
 * the raise changed is recorded in *token (if given).
 */
enum thread_elevation_result thread_elevation_raise_current(
      enum thread_elevation_task task, thread_elevation_token_t *token,
      const char **pending_via, const char **added_via);

/**
 * Undoes what *token records, on the calling thread - the one that
 * raised through it - and clears it. Changes made by the backend chain
 * are not recorded and stay until the thread ends.
 */
void thread_elevation_lower_current(thread_elevation_token_t *token);

/* What each thread was granted, for the statistics overlay */
enum thread_elevation_slot
{
   THREAD_ELEVATION_SLOT_MAIN = 0,
   THREAD_ELEVATION_SLOT_VIDEO,
   THREAD_ELEVATION_SLOT_AUDIO,
   THREAD_ELEVATION_SLOT_COUNT
};

/**
 * Records how a raise of the thread in 'slot' ended: the result and
 * token from thread_elevation_raise_current(), or a NULL token and
 * REFUSED for a thread that was not raised or has been lowered.
 * Called by the thread itself; read from any thread.
 */
void thread_elevation_note(enum thread_elevation_slot slot,
      enum thread_elevation_result result,
      const thread_elevation_token_t *token, bool asked);

/* Records whether the low-latency power plan is active, and with
 * idle states held off. */
void thread_elevation_note_power_plan(bool active, bool idle_disable);

/**
 * Writes one overlay line, newline included, naming what each thread
 * was granted and whether the low-latency power plan is active.
 * 'device' names what the audio driver's own thread runs under, or is
 * NULL where it has none. Writes nothing and returns 0 when nothing
 * was raised or applied.
 */
size_t thread_elevation_status(char *s, size_t len, const char *device);

/**
 * For a brokered backend refused after answering PENDING: tries the
 * brokered backends from 'next' on, for the same thread.
 */
void thread_elevation_continue(uint64_t tid, unsigned next);

RETRO_END_DECLS

#endif
