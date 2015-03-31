/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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


#include "../input_autodetect.h"
#include "../input_common.h"

static bool apple_joypad_init(void)
{
   if (!apple_hid_init())
       return false;

   return true;
}

static bool apple_joypad_query_pad(unsigned pad)
{
   return pad < MAX_USERS;
}

static void apple_joypad_destroy(void)
{
   apple_hid_free();
}

static bool apple_joypad_button(unsigned port, uint16_t joykey)
{
   return apple_hid_joypad_button(port, joykey);
}

static uint64_t apple_joypad_get_buttons(unsigned port)
{
   return apple_hid_joypad_get_buttons(port);
}

static int16_t apple_joypad_axis(unsigned port, uint32_t joyaxis)
{
   return apple_hid_joypad_axis(port, joyaxis);
}

static void apple_joypad_poll(void)
{
}

static bool apple_joypad_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   return apple_hid_joypad_rumble(pad, effect, strength);
}

static const char *apple_joypad_name(unsigned pad)
{
   /* TODO/FIXME - implement properly */
   if (pad >= MAX_USERS)
      return NULL;

   return NULL;
}

rarch_joypad_driver_t apple_hid_joypad = {
   apple_joypad_init,
   apple_joypad_query_pad,
   apple_joypad_destroy,
   apple_joypad_button,
   apple_joypad_get_buttons,
   apple_joypad_axis,
   apple_joypad_poll,
   apple_joypad_rumble,
   apple_joypad_name,
   "apple_hid"
};
