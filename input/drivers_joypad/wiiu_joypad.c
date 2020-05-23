/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouhlel
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

#include "../include/wiiu/input.h"

#include "wiiu_dbg.h"

static input_device_driver_t *pad_drivers[MAX_USERS];
extern pad_connection_listener_t wiiu_pad_connection_listener;

static bool ready = false;

static bool wiiu_joypad_init(void* data)
{
   set_connection_listener(&wiiu_pad_connection_listener);
   hid_instance.pad_list = pad_connection_init(MAX_USERS);
   hid_instance.max_slot = MAX_USERS;

   wpad_driver.init(data);
   kpad_driver.init(data);
#ifdef WIIU_HID
   hidpad_driver.init(data);
#endif

   ready = true;
   (void)data;

   return true;
}

static bool wiiu_joypad_query_pad(unsigned pad)
{
   return ready                &&
      pad < MAX_USERS          &&
      pad_drivers[pad] != NULL &&
      pad_drivers[pad]->query_pad(pad);
}

static void wiiu_joypad_destroy(void)
{
   ready = false;

   wpad_driver.destroy();
   kpad_driver.destroy();
#ifdef WIIU_HID
   hidpad_driver.destroy();
#endif
}

static bool wiiu_joypad_button(unsigned pad, uint16_t key)
{
   if (!wiiu_joypad_query_pad(pad))
      return false;

   return pad_drivers[pad]->button(pad, key);
}

static void wiiu_joypad_get_buttons(unsigned pad, input_bits_t *state)
{
   if (!wiiu_joypad_query_pad(pad))
      return;

   pad_drivers[pad]->get_buttons(pad, state);
}

static int16_t wiiu_joypad_axis(unsigned pad, uint32_t joyaxis)
{
   if (!wiiu_joypad_query_pad(pad))
      return 0;

   return pad_drivers[pad]->axis(pad, joyaxis);
}

static void wiiu_joypad_poll(void)
{
   wpad_driver.poll();
   kpad_driver.poll();
#ifdef WIIU_HID
   hidpad_driver.poll();
#endif
}

static const char* wiiu_joypad_name(unsigned pad)
{
   if (!wiiu_joypad_query_pad(pad))
      return "N/A";

   return pad_drivers[pad]->name(pad);
}

static void wiiu_joypad_connection_listener(unsigned pad,
               input_device_driver_t *driver)
{
   if (pad < MAX_USERS)
      pad_drivers[pad] = driver;
}

input_device_driver_t wiiu_joypad =
{
  wiiu_joypad_init,
  wiiu_joypad_query_pad,
  wiiu_joypad_destroy,
  wiiu_joypad_button,
  wiiu_joypad_get_buttons,
  wiiu_joypad_axis,
  wiiu_joypad_poll,
  NULL,
  wiiu_joypad_name,
  "wiiu",
};

pad_connection_listener_t wiiu_pad_connection_listener =
{
   wiiu_joypad_connection_listener
};
