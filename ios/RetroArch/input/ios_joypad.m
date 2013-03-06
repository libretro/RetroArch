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
#include "BTStack/wiimote.h"
#include "general.h"

static uint32_t g_buttons[MAX_PLAYERS];

static bool ios_joypad_init(void)
{
   return true;
}

static bool ios_joypad_query_pad(unsigned pad)
{
   return pad < MAX_PLAYERS;
}

static void ios_joypad_destroy(void)
{
}

static bool ios_joypad_button(unsigned port, uint16_t joykey)
{
   if (joykey == NO_BTN)
      return false;

   // Check hat.
   if (GET_HAT_DIR(joykey))
      return false;
   else // Check the button
      return (port < MAX_PLAYERS && joykey < 32) ? (g_buttons[port] & (1 << joykey)) != 0 : false;
}

static int16_t ios_joypad_axis(unsigned port, uint32_t joyaxis)
{
   return 0;
}

static void ios_joypad_poll(void)
{
   for (int i = 0; i != MAX_PLAYERS; i ++)
   {
      g_buttons[i] = 0;
      if (i < myosd_num_of_joys)
      {
         g_buttons[i] = joys[i].btns;
         g_buttons[i] |= (joys[i].exp.type == EXP_CLASSIC) ? (joys[i].exp.classic.btns << 16) : 0;
      }
   }
}

const rarch_joypad_driver_t ios_joypad = {
   ios_joypad_init,
   ios_joypad_query_pad,
   ios_joypad_destroy,
   ios_joypad_button,
   ios_joypad_axis,
   ios_joypad_poll,
   "ios",
};

