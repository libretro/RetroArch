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

#ifndef MENU_BIND_TRIGGER_H__
#define MENU_BIND_TRIGGER_H__

#include <stdint.h>
#include <retro_inline.h>

#include "../input/input_defines.h"

/* Binding L2 or R2 by pressing it.
 *
 * Some triggers are a button and an axis at once: a DualSense's and a
 * DualShock 4's report a button as soon as the trigger leaves its
 * rest, and its position on an axis. The capture looks at buttons
 * first, so such a trigger was bound to its button and was on or off
 * from then on, with its analog position bound to nothing.
 *
 * When a button answers while L2 or R2 is being bound, the trigger's
 * axis is looked for, and bound in the button's place if it is there.
 * It is told by two things together: it rests at an end of its range,
 * which a stick does not, and it has just left that rest, which the
 * other trigger has not.
 *
 * Returns the bind for that axis, in the direction away from its rest,
 * or AXIS_NONE when there is none - a trigger that is a button alone,
 * which is then bound to the button as before. */

/* how near an end an axis has to rest to be a trigger's */
#define MENU_BIND_TRIGGER_REST   0x7000
/* and how far it has to have left it: more than noise, less than the
 * first step a trigger reports - one 255th of the range is 257 */
#define MENU_BIND_TRIGGER_MOVED  128

static INLINE uint32_t menu_bind_trigger_axis(const int16_t *now,
      const int16_t *rest, unsigned count)
{
   unsigned a;
   for (a = 0; a < count; a++)
   {
      int at_rest = rest[a];
      int moved   = (int)now[a] - at_rest;
      if (at_rest <= -MENU_BIND_TRIGGER_REST && moved >=  MENU_BIND_TRIGGER_MOVED)
         return AXIS_POS(a);
      if (at_rest >=  MENU_BIND_TRIGGER_REST && moved <= -MENU_BIND_TRIGGER_MOVED)
         return AXIS_NEG(a);
   }
   return AXIS_NONE;
}

#endif
