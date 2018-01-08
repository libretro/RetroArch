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

typedef struct null_hid
{
   void *empty;
} null_hid_t;

static bool null_hid_joypad_query(void *data, unsigned pad)
{
   return pad < MAX_USERS;
}

static const char *null_hid_joypad_name(void *data, unsigned pad)
{
   /* TODO/FIXME - implement properly */
   if (pad >= MAX_USERS)
      return NULL;

   return NULL;
}

static void null_hid_joypad_get_buttons(void *data, unsigned port, retro_bits_t *state)
{
   (void)data;
   (void)port;

   BIT256_CLEAR_ALL_PTR(state);
}

static bool null_hid_joypad_button(void *data, unsigned port, uint16_t joykey)
{
   (void)data;
   (void)port;
   (void)joykey;

   return false;
}

static bool null_hid_joypad_rumble(void *data, unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   (void)data;
   (void)pad;
   (void)effect;
   (void)strength;

   return false;
}

static int16_t null_hid_joypad_axis(void *data, unsigned port, uint32_t joyaxis)
{
   (void)data;
   (void)port;
   (void)joyaxis;

   return 0;
}

static void *null_hid_init(void)
{
   return (null_hid_t*)calloc(1, sizeof(null_hid_t));
}

static void null_hid_free(const void *data)
{
   null_hid_t *hid_null = (null_hid_t*)data;

   if (hid_null)
      free(hid_null);
}

static void null_hid_poll(void *data)
{
   (void)data;
}

hid_driver_t null_hid = {
   null_hid_init,
   null_hid_joypad_query,
   null_hid_free,
   null_hid_joypad_button,
   null_hid_joypad_get_buttons,
   null_hid_joypad_axis,
   null_hid_poll,
   null_hid_joypad_rumble,
   null_hid_joypad_name,
   "null",
};
