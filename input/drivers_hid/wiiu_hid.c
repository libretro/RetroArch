/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
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

#include <stdlib.h>

#include "../input_defines.h"
#include "../input_driver.h"

typedef struct wiiu_hid
{
   void *empty;
} wiiu_hid_t;

static bool wiiu_hid_joypad_query(void *data, unsigned pad)
{
   return pad < MAX_USERS;
}

static const char *wiiu_hid_joypad_name(void *data, unsigned pad)
{
   return NULL;
}

static uint64_t wiiu_hid_joypad_get_buttons(void *data, unsigned port)
{
   (void)data;
   (void)port;

   return 0;
}

static bool wiiu_hid_joypad_button(void *data, unsigned port, uint16_t joykey)
{
   (void)data;
   (void)port;
   (void)joykey;

   return false;
}

static bool wiiu_hid_joypad_rumble(void *data, unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   (void)data;
   (void)pad;
   (void)effect;
   (void)strength;

   return false;
}

static int16_t wiiu_hid_joypad_axis(void *data, unsigned port, uint32_t joyaxis)
{
   (void)data;
   (void)port;
   (void)joyaxis;

   return 0;
}

static void *wiiu_hid_init(void)
{
   return (wiiu_hid_t*)calloc(1, sizeof(wiiu_hid_t));
}

static void wiiu_hid_free(void *data)
{
   wiiu_hid_t *hid_wiiu = (wiiu_hid_t*)data;

   if (hid_wiiu)
      free(hid_wiiu);
}

static void wiiu_hid_poll(void *data)
{
   (void)data;
}

hid_driver_t wiiu_hid = {
   wiiu_hid_init,
   wiiu_hid_joypad_query,
   wiiu_hid_free,
   wiiu_hid_joypad_button,
   wiiu_hid_joypad_get_buttons,
   wiiu_hid_joypad_axis,
   wiiu_hid_poll,
   wiiu_hid_joypad_rumble,
   wiiu_hid_joypad_name,
   "wiiu",
};
