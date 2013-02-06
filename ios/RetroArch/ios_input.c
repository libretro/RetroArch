// Input Driver Below
/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#include <unistd.h>
#include "../../input/input_common.h"
#include "../../performance.h"
#include "../../general.h"
#include "../../driver.h"

#define MAX_TOUCH 16

bool IOS_is_down;
int16_t IOS_touch_x, IOS_fix_x;
int16_t IOS_touch_y, IOS_fix_y;
int16_t IOS_full_x, IOS_full_y;

static void *ios_input_init(void)
{
   return (void*)-1;
}

static void ios_input_poll(void *data)
{
   input_translate_coord_viewport(IOS_touch_x, IOS_touch_y,
      &IOS_fix_x, &IOS_fix_y,
      &IOS_full_x, &IOS_full_y);
}

static int16_t ios_input_state(void *data, const struct retro_keybind **binds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   if (index != 0) return 0;
   switch (device)
   {
      case RARCH_DEVICE_POINTER_SCREEN:
         switch (id)
         {
            case RETRO_DEVICE_ID_POINTER_X:
               return IOS_full_x;
            case RETRO_DEVICE_ID_POINTER_Y:
               return IOS_full_y;
            case RETRO_DEVICE_ID_POINTER_PRESSED:
               return IOS_is_down;
            default:
               return 0;
         }
      default:
         return 0;
   }
}

static bool ios_input_key_pressed(void *data, int key)
{
   return false;
}

static void ios_input_free_input(void *data)
{
   (void)data;
}

const input_driver_t input_ios = {
   ios_input_init,
   ios_input_poll,
   ios_input_state,
   ios_input_key_pressed,
   ios_input_free_input,
   "ios_input",
};

