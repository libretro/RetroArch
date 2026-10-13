/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouhlel
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdint.h>
#include <stdlib.h>

#include <boolean.h>
#include <libretro.h>
#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <3ds.h>

#include "../input_driver.h"
#include "../../gfx/video_driver.h"

#define CTR_BOTTOM_WIDTH  320
#define CTR_BOTTOM_HEIGHT 240

static void ctr_input_free_input(void *data) { }

static void* ctr_input_init(const char *a) { return (void*)-1; }

/* The core's pointer: the bottom screen's touch, while it shows the
 * core's screens, at that point of its packed frame. The joypad's poll
 * has scanned the touchscreen. */
static int16_t ctr_input_state(
      void *data,
      const input_device_driver_t *joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds,
      unsigned port,
      unsigned device,
      unsigned idx,
      unsigned id)
{
   touchPosition touch;
   int16_t x, y;

   if (device != RETRO_DEVICE_POINTER || port || idx)
      return 0;
   if (!(hidKeysHeld() & KEY_TOUCH))
      return 0;
   hidTouchRead(&touch);
   /* The touch across the screen, as the views map it */
   x = (int16_t)((int)touch.px * 0xfffe / (CTR_BOTTOM_WIDTH  - 1) - 0x7fff);
   y = (int16_t)((int)touch.py * 0xfffe / (CTR_BOTTOM_HEIGHT - 1) - 0x7fff);
   if (!ctr_views_touch(x, y, &x, &y))
      return 0;
   switch (id)
   {
      case RETRO_DEVICE_ID_POINTER_X:
         return x;
      case RETRO_DEVICE_ID_POINTER_Y:
         return y;
      case RETRO_DEVICE_ID_POINTER_PRESSED:
         return 1;
      case RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN:
         return (x == -0x8000) ? 1 : 0;
      case RETRO_DEVICE_ID_POINTER_COUNT:
         return 1;
      default:
         break;
   }
   return 0;
}

static uint64_t ctr_input_get_capabilities(void *data)
{
   return (1 << RETRO_DEVICE_JOYPAD) | (1 << RETRO_DEVICE_ANALOG)
        | (1 << RETRO_DEVICE_POINTER);
}

input_driver_t input_ctr = {
   ctr_input_init,
   NULL,                         /* poll */
   ctr_input_state,
   ctr_input_free_input,
   NULL,
   NULL,
   ctr_input_get_capabilities,
   "ctr",
   NULL,                         /* grab_mouse */
   NULL,
   NULL
};
