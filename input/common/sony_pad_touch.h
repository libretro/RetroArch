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

#ifndef _SONY_PAD_TOUCH_H
#define _SONY_PAD_TOUCH_H

#include <stdint.h>

#include <boolean.h>
#include <retro_inline.h>

#include "../input_driver.h"

/* The touchpad's reach as SDL takes it: a little short of what the
 * pads report, which their edges seldom reach */
#define SONY_PAD_TOUCH_W     1920
#define SONY_PAD_TOUCH_H_DS4  920
#define SONY_PAD_TOUCH_H_DS5 1070

/* A DualShock 4's or DualSense's two fingers into @view, from their
 * place in the input report: four bytes each, the first with its top
 * bit set while the finger is off, then x and y, twelve bits each. */
static INLINE void sony_pad_touch(input_pad_view_t *view,
      const uint8_t *t, unsigned h, bool pressed)
{
   unsigned i;
   view->touch_down = pressed ? INPUT_PAD_TOUCH_PRESS : 0;
   for (i = 0; i < INPUT_PAD_VIEW_TOUCHES; i++, t += 4)
   {
      uint32_t x, y;
      if (t[0] & 0x80)
         continue;
      x = t[1] | ((uint32_t)(t[2] & 0x0f) << 8);
      y = (t[2] >> 4) | ((uint32_t)t[3] << 4);
      if (x >= SONY_PAD_TOUCH_W)
         x = SONY_PAD_TOUCH_W - 1;
      if (y >= h)
         y = h - 1;
      x = x * 0xfffeu / (SONY_PAD_TOUCH_W - 1);
      y = y * 0xfffeu / (h - 1);
      view->touch[i] = ((x - 0x7fff) & 0xffff) | ((y - 0x7fff) << 16);
      view->touch_down |= (uint8_t)(1u << i);
   }
}

#endif
