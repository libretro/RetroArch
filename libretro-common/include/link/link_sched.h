/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (link_sched.h).
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

#ifndef __LIBRETRO_SDK_LINK_SCHED_H
#define __LIBRETRO_SDK_LINK_SCHED_H

#include <link/link_bus.h>

RETRO_BEGIN_DECLS

/* Runs the consoles of a link bus session on one OS thread, each on a libco
 * cothread of its own for the scheduler's life, and serves the link
 * interface over the bus. An advance that cannot be granted yet parks its
 * console and another console runs; when none can, the bus is released.
 * No clock, lock, thread or wait. */

#define LINK_SCHED_DEFAULT_STACK 1048576
#define LINK_SCHED_SPIN_LIMIT    1024

typedef struct link_sched link_sched_t;
typedef void (*link_sched_run_t)(void *user);     /* calls the console's retro_run */

enum link_sched_pick
{
   LINK_SCHED_PICK_LOWEST = 0,
   LINK_SCHED_PICK_REVERSED,
   LINK_SCHED_PICK_SHUFFLED
};

enum link_sched_frame
{
   LINK_SCHED_FRAME_DONE = 0,
   LINK_SCHED_FRAME_RELEASED
};

/* One parked console at a release, from link_bus_blocked_by. A console
 * parked for making no progress is its own bound_console, with port and
 * bound_port ~0u and the ticks of its first entry, the grant as bound. */
struct link_sched_stuck
{
   unsigned console, port;
   uint64_t local_tick, request_tick, bound;
   unsigned bound_console, bound_port;
   bool     edge_unknown;
};

struct link_sched_stats
{
   uint64_t switches, parks, spin_parks, frames;
};

link_sched_t *link_sched_new(link_bus_t *bus);
void link_sched_free(link_sched_t *s);
int link_sched_add(link_sched_t *s, link_sched_run_t run, void *user, size_t stack_size); /* console number or -1; 0 = default stack */
void link_sched_set_pick(link_sched_t *s, enum link_sched_pick pick, uint32_t seed);
bool link_sched_environment(link_sched_t *s, void *data);   /* RETRO_ENVIRONMENT_GET_LINK_INTERFACE */
void link_sched_enter(link_sched_t *s, unsigned console);    /* around every other entry-point call */
void link_sched_leave(link_sched_t *s);
int link_sched_console(const link_sched_t *s);              /* the calling console, or -1 */
enum link_sched_frame link_sched_run_frame(link_sched_t *s);
unsigned link_sched_stuck_report(const link_sched_t *s, struct link_sched_stuck *out, unsigned cap); /* returns the total */
void link_sched_get_stats(const link_sched_t *s, struct link_sched_stats *out);

RETRO_END_DECLS

#endif
