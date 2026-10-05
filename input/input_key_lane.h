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
 * When the lane is full a press is dropped and counted. A release is
 * kept in a bitmap and given back as the release of its key:
 * - only once the key's press has been given out, so a release whose
 *   press is still in the lane - behind a slot claimed and not yet
 *   filled, or in slots filled after the bitmap was looked at - waits
 *   for it rather than being thrown away;
 * - and never over a newer press: a press accepted into the lane, or
 *   reported on the frontend's thread, takes back its key's kept
 *   release.
 * A full lane cannot leave a key held. */

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
   retro_atomic_int_t released[INPUT_KEY_LANE_WORDS]; /* kept releases */
   uint32_t keys_down[INPUT_KEY_LANE_WORDS];          /* given out down: the reader's */
} input_key_lane_t;

static INLINE unsigned input_key_lane_load(retro_atomic_int_t *p)
{
   return (unsigned)retro_atomic_load_acquire_int(p);
}

/* A press supersedes a kept release of its key. */
static INLINE void input_key_lane_take_back_release(input_key_lane_t *lane,
      unsigned code)
{
   if (     code < RETROK_LAST
         && (retro_atomic_load_relaxed_int(&lane->released[code >> 5])
            & (int)(1u << (code & 31))))
      retro_atomic_fetch_and_int(&lane->released[code >> 5],
            (int)~(1u << (code & 31)));
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
            if (down)
               input_key_lane_take_back_release(lane, code);
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
      retro_atomic_fetch_or_int(&lane->released[code >> 5],
            (int)(1u << (code & 31)));
   return false;
}

/* The reader's, for every key event given out - from the lane or
 * reported on the reader's own thread (@direct): which keys are down,
 * and a press there takes back a kept release. */
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

/* The reader's: every event filled in order since the last time, then
 * the kept releases whose keys are down. @deliver is expected to call
 * input_key_lane_note() for what it is given. */
static INLINE void input_key_lane_take(input_key_lane_t *lane,
      input_key_lane_deliver_t deliver)
{
   unsigned w;

   for (;;)
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
      deliver(down, code, character, mod, device);
   }

   for (w = 0; w < INPUT_KEY_LANE_WORDS; w++)
   {
      uint32_t ready, b;
      if (!retro_atomic_load_relaxed_int(&lane->released[w]))
         continue;
      /* only those whose press has been given out; the rest wait */
      ready = (uint32_t)retro_atomic_load_acquire_int(&lane->released[w])
         & lane->keys_down[w];
      if (!ready)
         continue;
      ready &= (uint32_t)retro_atomic_fetch_and_int(&lane->released[w],
            (int)~ready);
      for (b = 0; b < 32; b++)
         if (ready & (1u << b))
            deliver(false, w * 32 + b, 0, 0, RETRO_DEVICE_KEYBOARD);
   }
}

#endif
