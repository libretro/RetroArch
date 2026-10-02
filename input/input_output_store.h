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

#ifndef _INPUT_OUTPUT_STORE_H
#define _INPUT_OUTPUT_STORE_H

#include <boolean.h>
#include <retro_inline.h>
#include <retro_atomic.h>

/* What a core asks of an output - a rumble motor's strength, an LED's
 * state - held until the frontend writes it to the device.
 *
 * A core's rumble and LED calls arrive from inside retro_run(), and
 * they used to go straight to the driver: a sysfs open, write and
 * close for an LED, an ioctl or a transfer for a motor, as many times
 * a frame as the core called, on the core's own call stack. A core
 * that reports its rumble level every frame paid for a device write
 * every frame.
 *
 * Here the call stores the value and returns. The frontend takes the
 * stored values once the core has run, on the main thread, and makes
 * one driver call per output that was set: the last value of the
 * frame.
 *
 * One slot per output. A slot remembers that it was posted as well as
 * the value, so an output set to the value it already had is still
 * written once that frame - drivers whose effects run out rely on a
 * core repeating itself. Nothing is written for an output nobody set.
 *
 * Posting is safe from any thread: some cores make these calls from a
 * thread of their own. Taking is for one thread, the one that owns the
 * drivers. A poster stores the value, marks the slot, then bumps a
 * counter; the taker compares the counter with the value it saw last
 * and, if it moved, empties the marked slots. The counter is only ever
 * written by posters, so a take that reads a poster's bump is ordered
 * after that poster's mark and finds it, and a bump the take did not
 * read brings the next take back. A post is never lost and never
 * written twice; when two posts to one slot race, the later value is
 * the one written.
 *
 * No driver, settings or libretro types, so that
 * samples/input/output_store can build it on its own. */

#define OUTPUT_STORE_SLOTS 32

typedef struct
{
   retro_atomic_int_t value[OUTPUT_STORE_SLOTS];
   retro_atomic_int_t posted[OUTPUT_STORE_SLOTS];
   retro_atomic_int_t posts;   /* bumped by every post */
   retro_atomic_int_t used;    /* slots at or past this were never posted */
   int                seen;    /* the taker's: posts at its last take */
} output_store_t;

/* Any thread. @slot must be below OUTPUT_STORE_SLOTS. */
static INLINE void output_store_post(output_store_t *st,
      unsigned slot, int value)
{
   int used;

   if (slot >= OUTPUT_STORE_SLOTS)
      return;

   retro_atomic_store_release_int(&st->value[slot], value);
   retro_atomic_store_release_int(&st->posted[slot], 1);

   /* Only ever grows, so the taker need not look at slots no caller
    * has used: a pad on port 1 costs two slots a take, not thirty-two. */
   for (;;)
   {
      used = retro_atomic_load_acquire_int(&st->used);
      if ((unsigned)used > slot)
         break;
      if (retro_atomic_cas_int(&st->used, used, (int)slot + 1))
         break;
   }

   retro_atomic_fetch_add_int(&st->posts, 1);
}

/* The owning thread. Whether a take would find anything. One load and
 * a compare: what the frame pays when no core is using the output. */
static INLINE bool output_store_pending(output_store_t *st)
{
   return retro_atomic_load_acquire_int(&st->posts) != st->seen;
}

/* The owning thread. Calls @write for every slot posted since the last
 * take, lowest slot first, with the latest value posted to it. Returns
 * how many it wrote. */
static INLINE unsigned output_store_take(output_store_t *st,
      void (*write)(unsigned slot, int value, void *userdata),
      void *userdata)
{
   unsigned slot, used;
   unsigned written = 0;
   int      posts   = retro_atomic_load_acquire_int(&st->posts);

   if (posts == st->seen)
      return 0;
   st->seen = posts;

   used = (unsigned)retro_atomic_load_acquire_int(&st->used);
   if (used > OUTPUT_STORE_SLOTS)
      used = OUTPUT_STORE_SLOTS;

   for (slot = 0; slot < used; slot++)
   {
      if (retro_atomic_exchange_int(&st->posted[slot], 0))
      {
         write(slot, retro_atomic_load_acquire_int(&st->value[slot]),
               userdata);
         written++;
      }
   }

   return written;
}

/* The owning thread. Forget what is posted without writing it: the
 * driver is going away, or everything is being stopped. */
static INLINE void output_store_drop(output_store_t *st)
{
   unsigned slot;

   st->seen = retro_atomic_load_acquire_int(&st->posts);
   for (slot = 0; slot < OUTPUT_STORE_SLOTS; slot++)
      retro_atomic_store_release_int(&st->posted[slot], 0);
}

#endif
