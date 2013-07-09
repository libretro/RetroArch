/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
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

#include "input/input_common.h"
#include "general.h"

#ifdef IOS
#include "../iOS/input/BTStack/btdynamic.c"
#include "../iOS/input/BTStack/wiimote.c"
#include "../iOS/input/BTStack/btpad.c"
#include "../iOS/input/BTStack/btpad_ps3.c"
#include "../iOS/input/BTStack/btpad_wii.c"
#include "../iOS/input/BTStack/btpad_queue.c"
#elif defined(OSX)
#include "../OSX/hid_pad.c"
#endif


static bool apple_joypad_init(void)
{
   return true;
}

static bool apple_joypad_query_pad(unsigned pad)
{
   return pad < MAX_PLAYERS;
}

static void apple_joypad_destroy(void)
{
}

static bool apple_joypad_button(unsigned port, uint16_t joykey)
{
   if (joykey == NO_BTN)
      return false;

   // Check hat.
   if (GET_HAT_DIR(joykey))
      return false;
   else // Check the button
      return (port < MAX_PADS && joykey < 32) ? (g_polled_input_data.pad_buttons[port] & (1 << joykey)) != 0 : false;
}

static int16_t apple_joypad_axis(unsigned port, uint32_t joyaxis)
{
   if (joyaxis == AXIS_NONE || port != 0)
      return 0;

   int16_t val = 0;
   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      val = g_polled_input_data.pad_axis[AXIS_NEG_GET(joyaxis)];
      val = (val < 0) ? val : 0;
   }
   else if(AXIS_POS_GET(joyaxis) < 4)
   {
      val = g_polled_input_data.pad_axis[AXIS_POS_GET(joyaxis)];
      val = (val > 0) ? val : 0;
   }

   return val;
}

static void apple_joypad_poll(void)
{
}

static const char *apple_joypad_name(unsigned joypad)
{
   (void)joypad;
   return NULL;
}

const rarch_joypad_driver_t apple_joypad = {
   apple_joypad_init,
   apple_joypad_query_pad,
   apple_joypad_destroy,
   apple_joypad_button,
   apple_joypad_axis,
   apple_joypad_poll,
   apple_joypad_name,
   "apple"
};

