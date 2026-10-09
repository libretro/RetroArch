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

#include <stdint.h>
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
 * A 16-bit value and its pending bit share one atomic word. A take
 * exchanges that word with zero, so a racing post belongs entirely
 * to this take or the next. Taking is for one thread; posting is safe
 * from any thread. Values cover rumble strength and boolean LEDs.
 *
 * No driver, settings or libretro types, so that
 * samples/input/output_store can build it on its own. */

#define OUTPUT_STORE_SLOTS 32

#ifndef OUTPUT_STORE_AFTER_CLAIM
#define OUTPUT_STORE_AFTER_CLAIM(st) ((void)0)
#endif

#ifndef OUTPUT_STORE_AFTER_TAKE
#define OUTPUT_STORE_AFTER_TAKE(st, slot) ((void)0)
#endif

typedef struct
{
   retro_atomic_int_t pending[OUTPUT_STORE_SLOTS];
   retro_atomic_int_t posted;  /* slots to visit on the next take */
} output_store_t;

/* Any thread. @slot must be below OUTPUT_STORE_SLOTS. */
static INLINE void output_store_post(output_store_t *st,
      unsigned slot, uint16_t value)
{
   if (slot >= OUTPUT_STORE_SLOTS)
      return;

   retro_atomic_store_release_int(&st->pending[slot], 0x10000 | value);

   retro_atomic_fetch_or_int(&st->posted, (int)(1u << slot));
}

/* The owning thread. Whether a take would find anything. One load and
 * a compare: what the frame pays when no core is using the output. */
static INLINE bool output_store_pending(output_store_t *st)
{
   return retro_atomic_load_acquire_int(&st->posted) != 0;
}

/* The owning thread. Calls @write for every slot posted since the last
 * take, lowest slot first, with the latest value posted to it. Returns
 * how many it wrote. */
static INLINE unsigned output_store_take(output_store_t *st,
      void (*write)(unsigned slot, int value, void *userdata),
      void *userdata)
{
   unsigned slot;
   unsigned written = 0;
   uint32_t posted;

   if (!output_store_pending(st))
      return 0;
   posted = (uint32_t)retro_atomic_exchange_int(&st->posted, 0);
   OUTPUT_STORE_AFTER_CLAIM(st);

   for (slot = 0; posted; slot++, posted >>= 1)
   {
      int pending;
      if (!(posted & 1u))
         continue;
      /* A racing post may leave another notification for this slot;
       * its pending word prevents delivering a consumed value twice. */
      pending = retro_atomic_exchange_int(&st->pending[slot], 0);
      if (pending)
      {
         OUTPUT_STORE_AFTER_TAKE(st, slot);
         write(slot, pending & 0xffff, userdata);
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

   retro_atomic_exchange_int(&st->posted, 0);
   for (slot = 0; slot < OUTPUT_STORE_SLOTS; slot++)
      retro_atomic_exchange_int(&st->pending[slot], 0);
}

#endif
