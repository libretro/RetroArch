/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

#ifndef _INPUT_PAD_VIEW_H
#define _INPUT_PAD_VIEW_H

#include <stdlib.h>

#include <retro_inline.h>

#include "../input_driver.h"

/* A joypad driver that keeps each pad as an input_pad_view_t, filled in
 * its poll alone, answers its reads from it here and lends it through
 * view(): the snapshot bridge then reads the driver's own state, with
 * nothing copied. What these answer is what the bridge makes of the
 * view, so a driver reading through them agrees with it by building. */

/* A button, or a hat direction of the four hats the view holds */
static INLINE int32_t input_pad_view_button(const input_pad_view_t *v,
      uint16_t joykey)
{
   uint16_t dir = GET_HAT_DIR(joykey);
   if (dir)
   {
      unsigned h = GET_HAT(joykey);
      unsigned bit;
      switch (dir)
      {
         case HAT_UP_MASK:    bit = 0; break;
         case HAT_DOWN_MASK:  bit = 1; break;
         case HAT_LEFT_MASK:  bit = 2; break;
         case HAT_RIGHT_MASK: bit = 3; break;
         default:
            return 0;
      }
      if (h >= INPUT_PAD_VIEW_HATS)
         return 0;
      return (v->hats >> (h * 4 + bit)) & 1;
   }
   if (joykey >= 256)
      return 0;
   return BIT256_GET(v->buttons, joykey) ? 1 : 0;
}

/* One direction of an axis, as axis() gives it */
static INLINE int16_t input_pad_view_axis(const input_pad_view_t *v,
      uint32_t joyaxis)
{
   if (AXIS_NEG_GET(joyaxis) < INPUT_PAD_VIEW_AXES)
   {
      int16_t val = v->axes[AXIS_NEG_GET(joyaxis)];
      return (val < 0) ? val : 0;
   }
   if (AXIS_POS_GET(joyaxis) < INPUT_PAD_VIEW_AXES)
   {
      int16_t val = v->axes[AXIS_POS_GET(joyaxis)];
      return (val > 0) ? val : 0;
   }
   return 0;
}

/* The RetroPad's sixteen, as the common loop over what is behind each */
static INLINE int16_t input_pad_view_state(const input_pad_view_t *v,
      const rarch_joypad_info_t *joypad_info,
      const uint16_t *joykeys, const uint32_t *joyaxes)
{
   unsigned i;
   int16_t ret = 0;
   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      if (     joykeys[i] != NO_BTN
            && input_pad_view_button(v, joykeys[i]))
         ret |= (1 << i);
      else if (joyaxes[i] != AXIS_NONE
            && ((float)abs(input_pad_view_axis(v, joyaxes[i])) / 0x8000)
               > joypad_info->axis_threshold)
         ret |= (1 << i);
   }
   return ret;
}

/* Sixty-four buttons into the view, bit n button n */
#define INPUT_PAD_VIEW_SET_BITS64(v, bits) \
{ \
   (v)->buttons.data[0] = (uint32_t)(bits); \
   (v)->buttons.data[1] = (uint32_t)((uint64_t)(bits) >> 32); \
}

#endif
