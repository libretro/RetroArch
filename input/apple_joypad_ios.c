/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include "apple_input.h"
#include "input_common.h"
#include "../general.h"
#include "../apple/common/apple_gamecontroller.h"

#include "../apple/iOS/bluetooth/btdynamic.c"
#include "../apple/iOS/bluetooth/btpad.c"
#include "../apple/iOS/bluetooth/btpad_queue.c"
#include "connect/joypad_connection.h"

static bool apple_joypad_init(void)
{
   pad_connection_init();
   return true;
}

static bool apple_joypad_query_pad(unsigned pad)
{
   return pad < MAX_PLAYERS;
}

static void apple_joypad_destroy(void)
{
   pad_connection_destroy();
}

static bool apple_joypad_button(unsigned port, uint16_t joykey)
{
   apple_input_data_t *apple = (apple_input_data_t*)driver.input_data;
   uint32_t buttons = pad_connection_get_buttons(port);
   if (!apple || joykey == NO_BTN)
      return false;

   // Check hat.
   if (GET_HAT_DIR(joykey))
      return false;
   // Check the button
   if ((port < MAX_PLAYERS) && (joykey < 32))
       return ((apple->buttons[port] & (1 << joykey)) != 0) ||
              ((buttons & (1 << joykey)) != 0);
    return false;
}

static int16_t apple_joypad_axis(unsigned port, uint32_t joyaxis)
{
   apple_input_data_t *apple = (apple_input_data_t*)driver.input_data;
   int16_t val = 0;

   if (!apple || joyaxis == AXIS_NONE)
      return 0;

   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      val = apple->axes[port][AXIS_NEG_GET(joyaxis)];
      val += pad_connection_get_axis(port, AXIS_NEG_GET(joyaxis));
      val = (val < 0) ? val : 0;
   }
   else if(AXIS_POS_GET(joyaxis) < 4)
   {
      val = apple->axes[port][AXIS_POS_GET(joyaxis)];
      val += pad_connection_get_axis(port, AXIS_POS_GET(joyaxis));
      val = (val > 0) ? val : 0;
   }

   return val;
}

static void apple_joypad_poll(void)
{
    apple_gamecontroller_poll_all();
}

static bool apple_joypad_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   return pad_connection_rumble(pad, effect, strength);
}

static const char *apple_joypad_name(unsigned joypad)
{
   (void)joypad;
   return NULL;
}

rarch_joypad_driver_t apple_ios_joypad = {
   apple_joypad_init,
   apple_joypad_query_pad,
   apple_joypad_destroy,
   apple_joypad_button,
   apple_joypad_axis,
   apple_joypad_poll,
   apple_joypad_rumble,
   apple_joypad_name,
   "apple_ios"
};
