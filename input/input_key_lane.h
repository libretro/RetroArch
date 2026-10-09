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

/* The key lane: keyboard events reported off the frontend's thread wait
 * here for the poll. input_driver.c is its one user; it is a header of
 * its own so that a test can drive the very same code and stop a writer
 * where it pleases (INPUT_KEY_LANE_AFTER_CLAIM).
 *
 * A bounded queue of many writers and one reader, without a lock: each
 * slot has a sequence number saying whose turn it is. A writer claims
 * the next slot by moving the tail with a compare-and-swap, fills it,
 * and hands it to the reader through the sequence; the reader hands it
 * back the same way. Writers never wait on each other - one that loses
 * a slot takes the next - and each writer's events stay in its order.
 *
 * What it does not promise: a writer stopped between claiming a slot
 * and filling it (preempted, in practice) holds back the slots behind
 * it until it goes on. The reader does not wait for it - the poll
 * returns - but those events wait.
 *
 * Positions and sequences are unsigned and wrap by design; they are
 * kept in the atomics' int, the conversion being the modular one every
 * compiler RetroArch is built with gives. A slot's sequence is kept
 * less its index, so the zeroed lane is its first state.
 *
 * When full, a release keeps the tail position it followed. The reader
 * waits for those claims, then releases the key only if no later press
 * has been delivered. An older writer cannot erase a newer release.
 * Recovery preserves key levels, not text or every dropped transition.
 */

#ifndef __INPUT_KEY_LANE_H
#define __INPUT_KEY_LANE_H

#include <stdint.h>
#include <boolean.h>
#include <retro_inline.h>
#include <retro_atomic.h>

#ifndef INPUT_KEY_LANE_SIZE
#define INPUT_KEY_LANE_SIZE 64 /* a power of two */
#endif
#define INPUT_KEY_LANE_WORDS ((RETROK_LAST + 31) / 32)

#ifndef INPUT_KEY_LANE_BEFORE_RELEASE
#define INPUT_KEY_LANE_BEFORE_RELEASE(lane, code, pos) ((void)0)
#endif

/* For tests: called by a writer after it has claimed position @pos and
 * before it fills it. */
#ifndef INPUT_KEY_LANE_AFTER_CLAIM
#define INPUT_KEY_LANE_AFTER_CLAIM(lane, pos) ((void)0)
#endif

typedef void (*input_key_lane_deliver_t)(bool down, unsigned code,
      uint32_t character, uint16_t mod, unsigned device);

typedef struct input_key_lane
{
   struct
   {
      retro_atomic_int_t seq;    /* less the slot's index */
      uint32_t character;
      unsigned code;
      unsigned device;
      uint16_t mod;
      bool     down;
   } slot[INPUT_KEY_LANE_SIZE];
   retro_atomic_int_t tail;      /* next to claim: the writers' */
   unsigned           head;      /* next to take: the reader's alone */
   retro_atomic_int_t dropped;
   retro_atomic_int_t released[INPUT_KEY_LANE_WORDS]; /* recovery notifications */
   retro_atomic_int_t release_stamp[RETROK_LAST];   /* pending bit + 31-bit tail */
   uint64_t           position;                   /* reader's extended head */
   uint64_t           last_down[RETROK_LAST];
   uint32_t keys_down[INPUT_KEY_LANE_WORDS];          /* given out down: the reader's */
} input_key_lane_t;

static INLINE unsigned input_key_lane_load(retro_atomic_int_t *p)
{
   return (unsigned)retro_atomic_load_acquire_int(p);
}

/* The reader's direct press follows every claim made so far. */
static INLINE void input_key_lane_take_back_release(input_key_lane_t *lane,
      unsigned code)
{
   if (code < RETROK_LAST)
      lane->last_down[code] = lane->position
         + (unsigned)(input_key_lane_load(&lane->tail) - lane->head);
}

/* Recovery positions use modular differences within half a 31-bit lap. */
static INLINE int input_key_lane_stamp_diff(unsigned a, unsigned b)
{
   return (int)((a - b) << 1) / 2;
}

static INLINE void input_key_lane_keep_release(input_key_lane_t *lane,
      unsigned code, unsigned pos)
{
   unsigned old, stamp = 0x80000000u | (pos & 0x7fffffffu);
   for (;;)
   {
      old = input_key_lane_load(&lane->release_stamp[code]);
      if ((old & 0x80000000u)
            && input_key_lane_stamp_diff(pos, old) < 0)
         break;
      if (retro_atomic_cas_int(&lane->release_stamp[code],
               (int)old, (int)stamp))
         break;
   }
   retro_atomic_fetch_or_int(&lane->released[code >> 5],
         (int)(1u << (code & 31)));
}

/* A writer's. False when the lane was full: the event is not in it (a
 * release is kept, see above). */
static INLINE bool input_key_lane_push(input_key_lane_t *lane, bool down,
      unsigned code, uint32_t character, uint16_t mod, unsigned device)
{
   unsigned pos = (unsigned)retro_atomic_load_relaxed_int(&lane->tail);

   for (;;)
   {
      unsigned i   = pos & (INPUT_KEY_LANE_SIZE - 1);
      unsigned seq = input_key_lane_load(&lane->slot[i].seq) + i;

      if (seq == pos)
      {
         if (retro_atomic_cas_int(&lane->tail, (int)pos, (int)(pos + 1u)))
         {
            INPUT_KEY_LANE_AFTER_CLAIM(lane, pos);
            lane->slot[i].down      = down;
            lane->slot[i].code      = code;
            lane->slot[i].character = character;
            lane->slot[i].mod       = mod;
            lane->slot[i].device    = device;
            retro_atomic_store_release_int(&lane->slot[i].seq,
                  (int)(pos + 1u - i));
            return true;
         }
      }
      /* behind the reader by a whole lap: full. The difference is read
       * as signed so that it is right across the wrap. */
      else if ((int)(seq - pos) < 0)
         break;
      pos = (unsigned)retro_atomic_load_relaxed_int(&lane->tail);
   }

   retro_atomic_fetch_add_int(&lane->dropped, 1);
   if (!down && code < RETROK_LAST)
   {
      INPUT_KEY_LANE_BEFORE_RELEASE(lane, code, pos);
      input_key_lane_keep_release(lane, code, pos);
   }
   return false;
}

/* The reader's, for every key event given out - from the lane or
 * reported on the reader's own thread (@direct): which keys are down,
 * and where a direct press follows the queue. */
static INLINE void input_key_lane_note(input_key_lane_t *lane, bool down,
      unsigned code, bool direct)
{
   if (code >= RETROK_LAST)
      return;
   if (down)
   {
      lane->keys_down[code >> 5] |=  (1u << (code & 31));
      if (direct)
         input_key_lane_take_back_release(lane, code);
   }
   else
      lane->keys_down[code >> 5] &= ~(1u << (code & 31));
}

/* The reader's: ready events among the claims present at entry, then
 * eligible kept releases not superseded by a newer press. @deliver is expected to call
 * input_key_lane_note() for what it is given. */
static INLINE void input_key_lane_take(input_key_lane_t *lane,
      input_key_lane_deliver_t deliver)
{
   unsigned w;
   unsigned start = lane->head;
   unsigned count = input_key_lane_load(&lane->tail) - start;

   while ((unsigned)(lane->head - start) < count)
   {
      unsigned pos = lane->head;
      unsigned i   = pos & (INPUT_KEY_LANE_SIZE - 1);
      unsigned seq = input_key_lane_load(&lane->slot[i].seq) + i;
      bool down;
      unsigned code, device;
      uint32_t character;
      uint16_t mod;

      if (seq != pos + 1u)
         break;                  /* nothing more, or not filled yet */

      down      = lane->slot[i].down;
      code      = lane->slot[i].code;
      character = lane->slot[i].character;
      mod       = lane->slot[i].mod;
      device    = lane->slot[i].device;
      /* the slot is free before the event is acted on */
      retro_atomic_store_release_int(&lane->slot[i].seq,
            (int)(pos + INPUT_KEY_LANE_SIZE - i));
      lane->head = pos + 1u;
      if (down && code < RETROK_LAST)
         lane->last_down[code] = lane->position;
      lane->position++;
      deliver(down, code, character, mod, device);
   }

   for (w = 0; w < INPUT_KEY_LANE_WORDS; w++)
   {
      uint32_t ready, b, retry = 0;
      if (!retro_atomic_load_relaxed_int(&lane->released[w]))
         continue;
      ready = (uint32_t)retro_atomic_exchange_int(&lane->released[w], 0);
      for (b = 0; b < 32; b++)
      {
         unsigned code, stamp;
         int diff;
         uint64_t position;
         if (!(ready & (1u << b)))
            continue;
         code = w * 32 + b;
         if (code >= RETROK_LAST)
            continue;
         stamp = input_key_lane_load(&lane->release_stamp[code]);
         if (!(stamp & 0x80000000u))
            continue;
         diff = input_key_lane_stamp_diff(stamp, lane->head);
         if (diff > 0 || !retro_atomic_cas_int(&lane->release_stamp[code],
                  (int)stamp, 0))
         {
            retry |= 1u << b;
            continue;
         }
         if ((uint64_t)(unsigned)(-diff) > lane->position)
            continue;
         position = lane->position - (unsigned)(-diff);
         if ((lane->keys_down[w] & (1u << b))
               && lane->last_down[code] < position)
            deliver(false, code, 0, 0, RETRO_DEVICE_KEYBOARD);
      }
      if (retry)
         retro_atomic_fetch_or_int(&lane->released[w], (int)retry);
   }
}

/* The reader's event cannot overtake a claimed but unpublished slot. */
static INLINE void input_key_lane_dispatch(input_key_lane_t *lane, bool down,
      unsigned code, uint32_t character, uint16_t mod, unsigned device,
      input_key_lane_deliver_t deliver)
{
   if (lane->head != input_key_lane_load(&lane->tail))
   {
      input_key_lane_take(lane, deliver);
      if (lane->head != input_key_lane_load(&lane->tail))
      {
         input_key_lane_push(lane, down, code, character, mod, device);
         return;
      }
   }
   if (down && code < RETROK_LAST)
      lane->last_down[code] = lane->position;
   deliver(down, code, character, mod, device);
}

#endif
