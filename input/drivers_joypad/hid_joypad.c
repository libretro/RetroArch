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
#include "../input_hid_driver.h"
#include "../../driver.h"

static const hid_driver_t *generic_hid;

static bool hid_joypad_init(void *data)
{
   generic_hid = input_hid_init_first();
   if (!generic_hid)
       return false;

   (void)data;

   return true;
}

static bool hid_joypad_query_pad(unsigned pad)
{
   driver_t *driver   = driver_get_ptr();
   return generic_hid->query_pad(driver->hid_data, pad);
}

static void hid_joypad_free(void)
{
   driver_t *driver   = driver_get_ptr();
   generic_hid->free(driver->hid_data);
   generic_hid = NULL;
}

static bool hid_joypad_button(unsigned port, uint16_t joykey)
{
   driver_t *driver   = driver_get_ptr();
   return generic_hid->button(driver->hid_data, port, joykey);
}

static uint64_t hid_joypad_get_buttons(unsigned port)
{
   driver_t *driver   = driver_get_ptr();
   return generic_hid->get_buttons(driver->hid_data, port);
}

static int16_t hid_joypad_axis(unsigned port, uint32_t joyaxis)
{
   driver_t *driver   = driver_get_ptr();
   return generic_hid->axis(driver->hid_data, port, joyaxis);
}

static void hid_joypad_poll(void)
{
   driver_t *driver   = driver_get_ptr();
   generic_hid->poll(driver->hid_data);
}

static bool hid_joypad_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   driver_t *driver   = driver_get_ptr();
   return generic_hid->set_rumble(driver->hid_data, pad, effect, strength);
}

static const char *hid_joypad_name(unsigned pad)
{
   driver_t *driver   = driver_get_ptr();
   return generic_hid->name(driver->hid_data, pad);
}

input_device_driver_t hid_joypad = {
   hid_joypad_init,
   hid_joypad_query_pad,
   hid_joypad_free,
   hid_joypad_button,
   hid_joypad_get_buttons,
   hid_joypad_axis,
   hid_joypad_poll,
   hid_joypad_rumble,
   hid_joypad_name,
   "hid"
};
