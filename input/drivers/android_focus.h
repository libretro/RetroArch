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

#include <boolean.h>
#include <retro_inline.h>

/* Whether RetroArch has the focus on Android: its Activity's window's,
 * acted on once per poll; app thread only. The second display's
 * Presentation never takes the focus.
 *
 * Free of NDK types for samples/input/android_focus. */

enum android_focus_flags
{
   ANDROID_FOCUS_ACTIVITY         = (1 << 0),
   ANDROID_FOCUS_APPLIED          = (1 << 1),
   ANDROID_FOCUS_APPLIED_FOCUSED  = (1 << 2),
   /* Since the last android_focus_settle() */
   ANDROID_FOCUS_ACTIVITY_CHANGED = (1 << 3)
};

enum android_focus_act
{
   ANDROID_FOCUS_KEEP = 0,
   ANDROID_FOCUS_GAIN,
   ANDROID_FOCUS_LOSE
};

typedef struct android_focus
{
   unsigned flags;
} android_focus_t;

/* APP_CMD_GAINED_FOCUS, APP_CMD_LOST_FOCUS */
static INLINE void android_focus_activity(android_focus_t *f,
      bool focused)
{
   f->flags |= ANDROID_FOCUS_ACTIVITY_CHANGED;
   if (focused)
   {
      f->flags |= ANDROID_FOCUS_ACTIVITY;
      return;
   }
   f->flags &= ~ANDROID_FOCUS_ACTIVITY;
}

/* After a poll's commands: whether RetroArch gained or lost the focus.
 * Nothing is acted on before the Activity's first focus command. */
static INLINE enum android_focus_act android_focus_settle(
      android_focus_t *f)
{
   bool focused;
   enum android_focus_act act = ANDROID_FOCUS_KEEP;
   unsigned flags             = f->flags;

   focused = (flags & ANDROID_FOCUS_ACTIVITY) != 0;

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

   f->flags = flags & ~ANDROID_FOCUS_ACTIVITY_CHANGED;
   return act;
}

#endif
