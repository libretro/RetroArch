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

#ifndef _ANDROID_POLL_CHANGES_H
#define _ANDROID_POLL_CHANGES_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <boolean.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>

/* What one input poll (input/drivers/android_input.c) has changed.
 *
 * The input reader hands a poll every event since the last one, so a
 * key, a finger or a mouse button can go down and back up in between.
 * The core reads the state once a frame, after the poll, and would
 * never see that press. So the poll applies events in order until one
 * would undo a change an earlier one made, and leaves it and the rest
 * for the next poll: a press shorter than a frame is shown for one
 * frame, and nothing waits when nothing has to. A key event that
 * changes nothing, such as a repeat, never waits. A pad's hat, its
 * D-pad on many pads, is four keys the same way. */

#define ANDROID_POLL_CHANGES_ROWS      9  /* 8 pad rows and the keyboard's */
#define ANDROID_POLL_CHANGES_KEY_BYTES 32 /* keycodes 0-255 */
#define ANDROID_POLL_CHANGES_POINTERS  32

typedef struct
{
   int32_t pointer_device[ANDROID_POLL_CHANGES_POINTERS];
   int32_t pointer_id[ANDROID_POLL_CHANGES_POINTERS];
   unsigned pointers;
   unsigned mouse_down;  /* buttons pressed this poll */
   unsigned mouse_up;    /* buttons let go this poll  */
   uint8_t down[ANDROID_POLL_CHANGES_ROWS][ANDROID_POLL_CHANGES_KEY_BYTES];
   uint8_t up[ANDROID_POLL_CHANGES_ROWS][ANDROID_POLL_CHANGES_KEY_BYTES];
   uint8_t pointer_down[ANDROID_POLL_CHANGES_POINTERS];
   uint8_t hat_down[ANDROID_POLL_CHANGES_ROWS]; /* directions, one bit each */
   uint8_t hat_up[ANDROID_POLL_CHANGES_ROWS];
} android_poll_changes_t;

static INLINE void android_poll_changes_begin(android_poll_changes_t *c)
{
   memset(c, 0, sizeof(*c));
}

/* A press (@down) or release of @keycode on @row, where it is held
 * (@held) or not: false if the key went the other way earlier in this
 * poll. */
static INLINE bool android_poll_changes_key(android_poll_changes_t *c,
      unsigned row, unsigned keycode, bool held, bool down)
{
   uint8_t *mine;
   uint8_t *other;

   if (     row     >= ANDROID_POLL_CHANGES_ROWS
         || keycode >= ANDROID_POLL_CHANGES_KEY_BYTES * 8
         || held    == down)
      return true;
   mine  = down ? c->down[row] : c->up[row];
   other = down ? c->up[row]   : c->down[row];
   if (BIT_GET(other, keycode))
      return false;
   BIT_SET(mine, keycode);
   return true;
}

/* The id of the @i-th pointer, ids @stride bytes apart */
static INLINE int32_t android_poll_changes_id(const int32_t *ids,
      size_t stride, unsigned i)
{
   return *(const int32_t*)((const char*)ids + (size_t)i * stride);
}

/* @count pointers of @device going down (@down) or up together, their
 * ids @stride bytes apart: false if one of them went the other way
 * earlier in this poll, or if this poll can keep track of no more
 * pointers. */
static INLINE bool android_poll_changes_pointers(android_poll_changes_t *c,
      int32_t device, const int32_t *ids, size_t stride, unsigned count,
      bool down)
{
   unsigned i, j;
   unsigned fresh = 0;

   for (i = 0; i < count; i++)
   {
      int32_t id = android_poll_changes_id(ids, stride, i);
      for (j = 0; j < c->pointers; j++)
         if (     c->pointer_device[j] == device
               && c->pointer_id[j]     == id)
            break;
      if (j == c->pointers)
         fresh++;
      else if ((c->pointer_down[j] != 0) != down)
         return false;
   }
   if (c->pointers + fresh > ANDROID_POLL_CHANGES_POINTERS)
      return false;

   for (i = 0; i < count; i++)
   {
      int32_t id = android_poll_changes_id(ids, stride, i);
      for (j = 0; j < c->pointers; j++)
         if (     c->pointer_device[j] == device
               && c->pointer_id[j]     == id)
            break;
      if (j < c->pointers)
         continue;
      c->pointer_device[j] = device;
      c->pointer_id[j]     = id;
      c->pointer_down[j]   = down ? 1 : 0;
      c->pointers++;
   }
   return true;
}

/* The mouse buttons going from @was to @now, one bit a button: false if
 * one of them went the other way earlier in this poll. */
static INLINE bool android_poll_changes_mouse(android_poll_changes_t *c,
      unsigned was, unsigned now)
{
   unsigned pressed  = now & ~was;
   unsigned released = was & ~now;

   if ((pressed & c->mouse_up) || (released & c->mouse_down))
      return false;
   c->mouse_down |= pressed;
   c->mouse_up   |= released;
   return true;
}

/* A hat at @x, @y (-1, 0 or 1 each) as four direction bits */
static INLINE unsigned android_poll_changes_hat_bits(int x, int y)
{
   return (x < 0 ? 1u : 0u) | (x > 0 ? 2u : 0u)
        | (y < 0 ? 4u : 0u) | (y > 0 ? 8u : 0u);
}

/* The hat of @row going from directions @was to @now: false if one of
 * them went the other way earlier in this poll. */
static INLINE bool android_poll_changes_hat(android_poll_changes_t *c,
      unsigned row, unsigned was, unsigned now)
{
   unsigned pressed  = now & ~was;
   unsigned released = was & ~now;

   if (row >= ANDROID_POLL_CHANGES_ROWS)
      return true;
   if ((pressed & c->hat_up[row]) || (released & c->hat_down[row]))
      return false;
   c->hat_down[row] |= (uint8_t)pressed;
   c->hat_up[row]   |= (uint8_t)released;
   return true;
}

#endif
