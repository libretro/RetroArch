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

#ifndef _ANDROID_PRESS_GROUP_H
#define _ANDROID_PRESS_GROUP_H

#include <stdint.h>
#include <string.h>

#include <boolean.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>

/* The input block timeout (input/drivers/android_input.c), without
 * waiting: some pads send one press of several buttons as separate
 * events a few milliseconds apart, and a poll can fall between them.
 *
 * A pad's presses in a poll are shown at its end, unless the newest of
 * them came within the timeout of it: then all of them wait for the
 * next poll, so that the rest of the press, if there is more, is shown
 * with them. Nothing waits more than one poll - presses held over are
 * shown at the next poll with whatever came since - and a press older
 * than the timeout is shown at once. Android stamps every event with
 * when it happened, which is what "newest" and "within" go by; the
 * poll itself never waits.
 *
 * A held press released before it is shown is shown for one poll, so
 * that it is not lost. */

#define ANDROID_PRESS_GROUP_BYTES 32

typedef struct
{
   uint8_t fresh[ANDROID_PRESS_GROUP_BYTES];  /* pressed this poll    */
   uint8_t held[ANDROID_PRESS_GROUP_BYTES];   /* from the last poll   */
   uint8_t tapped[ANDROID_PRESS_GROUP_BYTES]; /* let go before shown  */
   uint8_t untap[ANDROID_PRESS_GROUP_BYTES];  /* shown for a tap: go  */
   int64_t newest_ns;  /* the newest press this poll, as Android stamps it */
   bool any_fresh;
   bool any_held;
   bool any_untap;
} android_press_group_t;

/* A pad's press of @key at @at_ns: kept back, to be shown at the end
 * of the poll or the next. */
static INLINE void android_press_group_down(android_press_group_t *g,
      int key, int64_t at_ns)
{
   BIT_SET(g->fresh, key);
   BIT_CLEAR(g->tapped, key);
   g->any_fresh = true;
   if (at_ns > g->newest_ns)
      g->newest_ns = at_ns;
}

/* A pad's release of @key: true if the press is one still kept back,
 * which is then shown as a tap; false if the row is to let go of it. */
static INLINE bool android_press_group_up(android_press_group_t *g, int key)
{
   if (!BIT_GET(g->fresh, key) && !BIT_GET(g->held, key))
      return false;
   BIT_SET(g->tapped, key);
   return true;
}

/* The poll's end, at @now_ns: into the pad's @row what is to be shown
 * now, with a window of @window_ns. @now_ns is read only where a press
 * came this poll; without one it may be anything. */
static INLINE void android_press_group_end(android_press_group_t *g,
      uint8_t *row, unsigned row_bytes, int64_t now_ns, int64_t window_ns)
{
   unsigned i;

   if (row_bytes > ANDROID_PRESS_GROUP_BYTES)
      row_bytes = ANDROID_PRESS_GROUP_BYTES;

   /* a tap shown at the last poll is let go */
   if (g->any_untap)
   {
      for (i = 0; i < row_bytes; i++)
         row[i] &= (uint8_t)~g->untap[i];
      memset(g->untap, 0, sizeof(g->untap));
      g->any_untap = false;
   }

   if (!g->any_fresh && !g->any_held)
      return;

   if (     g->any_fresh && !g->any_held
         && now_ns - g->newest_ns < window_ns)
   {
      /* the newest press is too new to know whether more follows */
      memcpy(g->held, g->fresh, sizeof(g->held));
      memset(g->fresh, 0, sizeof(g->fresh));
      g->any_held  = true;
      g->any_fresh = false;
      g->newest_ns = 0;
      return;
   }

   for (i = 0; i < row_bytes; i++)
   {
      uint8_t shown = (uint8_t)(g->held[i] | g->fresh[i]);
      row[i]       |= shown;
      g->untap[i]   = (uint8_t)(g->tapped[i] & shown);
      if (g->untap[i])
         g->any_untap = true;
   }
   memset(g->fresh,  0, sizeof(g->fresh));
   memset(g->held,   0, sizeof(g->held));
   memset(g->tapped, 0, sizeof(g->tapped));
   g->any_fresh = false;
   g->any_held  = false;
   g->newest_ns = 0;
}

#endif
