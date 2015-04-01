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

static void *generic_hid;

static bool hid_joypad_init(void)
{
   generic_hid = apple_hid_init();
   if (!generic_hid)
       return false;

   return true;
}

static bool hid_joypad_query_pad(unsigned pad)
{
   return apple_hid_joypad_query_pad(generic_hid, pad);
}

static void hid_joypad_destroy(void)
{
   apple_hid_free(generic_hid);
   generic_hid = NULL;
}

static bool hid_joypad_button(unsigned port, uint16_t joykey)
{
   return apple_hid_joypad_button(generic_hid, port, joykey);
}

static uint64_t hid_joypad_get_buttons(unsigned port)
{
   return apple_hid_joypad_get_buttons(generic_hid, port);
}

static int16_t hid_joypad_axis(unsigned port, uint32_t joyaxis)
{
   return apple_hid_joypad_axis(generic_hid, port, joyaxis);
}

static void hid_joypad_poll(void)
{
   apple_hid_poll(generic_hid);
}

static bool hid_joypad_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   return apple_hid_joypad_rumble(generic_hid, pad, effect, strength);
}

static const char *hid_joypad_name(unsigned pad)
{
   return apple_hid_joypad_name(generic_hid, pad);
}

rarch_joypad_driver_t hid_joypad = {
   hid_joypad_init,
   hid_joypad_query_pad,
   hid_joypad_destroy,
   hid_joypad_button,
   hid_joypad_get_buttons,
   hid_joypad_axis,
   hid_joypad_poll,
   hid_joypad_rumble,
   hid_joypad_name,
   "hid"
};
