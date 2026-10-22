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

#ifndef _ANDROID_FOCUS_H
#define _ANDROID_FOCUS_H

#include <stdint.h>

#include <boolean.h>
#include <retro_inline.h>

/* Whether RetroArch has the focus on Android, from the Activity's
 * window and the second display's Presentation; app thread only.
 *
 * Where each display has a focus of its own, the Presentation keeps
 * its display's focus while the keys go elsewhere, so it counts only
 * while in use: from a key, stick move or touch read from it, a focus
 * gained while the Activity has none, or a hand-off from the Activity,
 * until the Activity's focus changes or the Presentation's ends.
 *
 * Free of NDK types for samples/input/android_focus. */

/* The two halves of a one-focus system's hand-off arrive this close */
#define ANDROID_FOCUS_HANDOFF_US 100000

enum android_focus_flags
{
   ANDROID_FOCUS_ACTIVITY         = (1 << 0),
   ANDROID_FOCUS_SECOND           = (1 << 1),
   ANDROID_FOCUS_SECOND_IN_USE    = (1 << 2),
   ANDROID_FOCUS_APPLIED          = (1 << 3),
   ANDROID_FOCUS_APPLIED_FOCUSED  = (1 << 4),
   /* Since the last android_focus_settle() */
   ANDROID_FOCUS_ACTIVITY_CHANGED = (1 << 5),
   ANDROID_FOCUS_HANDOFF          = (1 << 6),
   ANDROID_FOCUS_SECOND_GAINED    = (1 << 7),
   ANDROID_FOCUS_SECOND_INPUT     = (1 << 8)
};

enum android_focus_act
{
   ANDROID_FOCUS_KEEP = 0,
   ANDROID_FOCUS_GAIN,
   ANDROID_FOCUS_LOSE
};

typedef struct android_focus
{
   int64_t second_gained_us;
   unsigned flags;
} android_focus_t;

/* APP_CMD_GAINED_FOCUS, APP_CMD_LOST_FOCUS */
static INLINE void android_focus_activity(android_focus_t *f,
      bool focused, int64_t now_us)
{
   f->flags |= ANDROID_FOCUS_ACTIVITY_CHANGED;
   if (focused)
   {
      f->flags |= ANDROID_FOCUS_ACTIVITY;
      return;
   }
   f->flags &= ~ANDROID_FOCUS_ACTIVITY;
   if (     (f->flags & ANDROID_FOCUS_SECOND)
         && now_us - f->second_gained_us <= ANDROID_FOCUS_HANDOFF_US)
      f->flags |= ANDROID_FOCUS_HANDOFF;
}

/* APP_CMD_SECOND_GAINED_FOCUS, APP_CMD_SECOND_LOST_FOCUS */
static INLINE void android_focus_second(android_focus_t *f,
      bool focused, int64_t now_us)
{
   if (focused)
   {
      f->flags           |= ANDROID_FOCUS_SECOND
                          | ANDROID_FOCUS_SECOND_GAINED;
      f->second_gained_us = now_us;
      return;
   }
   f->flags &= ~(ANDROID_FOCUS_SECOND | ANDROID_FOCUS_SECOND_IN_USE
         | ANDROID_FOCUS_SECOND_GAINED | ANDROID_FOCUS_SECOND_INPUT
         | ANDROID_FOCUS_HANDOFF);
}

/* A key, stick move or touch read from the Presentation */
static INLINE void android_focus_second_input(android_focus_t *f)
{
   f->flags |= ANDROID_FOCUS_SECOND_INPUT;
}

/* After a poll's rings and commands: whether RetroArch gained or lost
 * the focus. Nothing is acted on before the Activity's first focus
 * command. */
static INLINE enum android_focus_act android_focus_settle(
      android_focus_t *f)
{
   bool focused;
   enum android_focus_act act = ANDROID_FOCUS_KEEP;
   unsigned flags             = f->flags;

   if (flags & ANDROID_FOCUS_ACTIVITY_CHANGED)
      flags &= ~ANDROID_FOCUS_SECOND_IN_USE;
   if (     (flags & ANDROID_FOCUS_SECOND)
         && (     (flags & (ANDROID_FOCUS_SECOND_INPUT
                          | ANDROID_FOCUS_HANDOFF))
              || (     (flags & ANDROID_FOCUS_SECOND_GAINED)
                   && !(flags & ANDROID_FOCUS_ACTIVITY))))
      flags |= ANDROID_FOCUS_SECOND_IN_USE;

   focused = (flags & (ANDROID_FOCUS_ACTIVITY
         | ANDROID_FOCUS_SECOND_IN_USE)) != 0;

   if (     (flags & (ANDROID_FOCUS_APPLIED
                    | ANDROID_FOCUS_ACTIVITY_CHANGED))
         && (     !(flags & ANDROID_FOCUS_APPLIED)
              || !(flags & ANDROID_FOCUS_APPLIED_FOCUSED) != !focused))
   {
      act    = focused ? ANDROID_FOCUS_GAIN : ANDROID_FOCUS_LOSE;
      flags |= ANDROID_FOCUS_APPLIED;
      if (focused)
         flags |=  ANDROID_FOCUS_APPLIED_FOCUSED;
      else
         flags &= ~ANDROID_FOCUS_APPLIED_FOCUSED;
   }

   f->flags = flags & ~(ANDROID_FOCUS_ACTIVITY_CHANGED
         | ANDROID_FOCUS_HANDOFF | ANDROID_FOCUS_SECOND_GAINED
         | ANDROID_FOCUS_SECOND_INPUT);
   return act;
}

#endif
