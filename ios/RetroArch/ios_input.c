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

struct
{
   bool is_down;
   int16_t screen_x, screen_y;
   int16_t fixed_x, fixed_y;
   int16_t full_x, full_y;
} ios_touches[MAX_TOUCH];

bool ios_keys[256];

uint32_t ios_current_touch_count = 0;

static void *ios_input_init(void)
{
   memset(ios_touches, 0, sizeof(ios_touches));
   memset(ios_keys, 0, sizeof(ios_keys));
   return (void*)-1;
}

static void ios_input_poll(void *data)
{
   for (int i = 0; i != ios_current_touch_count; i ++)
   {
      input_translate_coord_viewport(ios_touches[i].screen_x, ios_touches[i].screen_y,
         &ios_touches[i].fixed_x, &ios_touches[i].fixed_y,
         &ios_touches[i].full_x, &ios_touches[i].full_y);
   }
}

static int16_t ios_input_state(void *data, const struct retro_keybind **binds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (port != 0) return 0;
         switch (id)
         {
            case RETRO_DEVICE_ID_JOYPAD_UP: return ios_keys[82];
            case RETRO_DEVICE_ID_JOYPAD_DOWN: return ios_keys[81];
            case RETRO_DEVICE_ID_JOYPAD_LEFT: return ios_keys[80];
            case RETRO_DEVICE_ID_JOYPAD_RIGHT: return ios_keys[79];
            case RETRO_DEVICE_ID_JOYPAD_B: return ios_keys[0x1D];
            case RETRO_DEVICE_ID_JOYPAD_A: return ios_keys[0x1B];
            case RETRO_DEVICE_ID_JOYPAD_Y: return ios_keys[0x04];
            case RETRO_DEVICE_ID_JOYPAD_X: return ios_keys[0x1D];
            case RETRO_DEVICE_ID_JOYPAD_START: return ios_keys[0x1A];
            case RETRO_DEVICE_ID_JOYPAD_SELECT: return ios_keys[0x14];
            case RETRO_DEVICE_ID_JOYPAD_L: return ios_keys[0x07];
            case RETRO_DEVICE_ID_JOYPAD_R: return ios_keys[0x06];
            default: return 0;
         }
         return 0;
   
      case RARCH_DEVICE_POINTER_SCREEN:
         switch (id)
         {
            case RETRO_DEVICE_ID_POINTER_X:
               return (index < ios_current_touch_count) ? ios_touches[index].full_x : 0;
            case RETRO_DEVICE_ID_POINTER_Y:
               return (index < ios_current_touch_count) ? ios_touches[index].full_y : 0;
            case RETRO_DEVICE_ID_POINTER_PRESSED:
               return (index < ios_current_touch_count) ? ios_touches[index].is_down : 0;
            default:
               return 0;
         }
      default:
         return 0;
   }
}

static bool ios_input_key_pressed(void *data, int key)
{
   switch (key)
   {
      case RARCH_FAST_FORWARD_KEY: return ios_keys[0x2C];
      case RARCH_FAST_FORWARD_HOLD_KEY: return ios_keys[0x0F];
      case RARCH_LOAD_STATE_KEY: return ios_keys[0x03D];
      case RARCH_SAVE_STATE_KEY: return ios_keys[0x03B];
      case RARCH_STATE_SLOT_PLUS: return ios_keys[0x40];
      case RARCH_STATE_SLOT_MINUS: return ios_keys[0x3F];
      case RARCH_REWIND: return ios_keys[0x15];
      case RARCH_PAUSE_TOGGLE: return ios_keys[0x13];
      case RARCH_FRAMEADVANCE: return ios_keys[0x0E];
      case RARCH_RESET: return ios_keys[0x0B];
   }

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

