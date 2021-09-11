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

extern pad_connection_listener_t wiiu_pad_connection_listener;

/* TODO/FIXME - static globals */
static input_device_driver_t *wiiu_pad_drivers[MAX_USERS];
static bool wiiu_joypad_ready = false;

static void *wiiu_joypad_init(void *data)
{
   set_connection_listener(&wiiu_pad_connection_listener);
   hid_instance.pad_list = pad_connection_init(MAX_USERS);
   hid_instance.max_slot = MAX_USERS;

   wpad_driver.init(data);
   kpad_driver.init(data);
#ifdef WIIU_HID
   hidpad_driver.init(data);
#endif

   wiiu_joypad_ready = true;

   return (void*)-1;
}

static bool wiiu_joypad_query_pad(unsigned pad)
{
   return wiiu_joypad_ready    &&
      pad < MAX_USERS          &&
      wiiu_pad_drivers[pad]    &&
      wiiu_pad_drivers[pad]->query_pad(pad);
}

static void wiiu_joypad_destroy(void)
{
   wiiu_joypad_ready = false;

   wpad_driver.destroy();
   kpad_driver.destroy();
#ifdef WIIU_HID
   hidpad_driver.destroy();
#endif
}

static int32_t wiiu_joypad_button(unsigned port, uint16_t joykey)
{
   if (!wiiu_joypad_query_pad(port))
      return 0;
   if (port >= DEFAULT_MAX_PADS)
      return 0;
   return (wiiu_pad_drivers[port]->button(port, joykey));
}

static void wiiu_joypad_get_buttons(unsigned port, input_bits_t *state)
{
   if (!wiiu_joypad_query_pad(port))
      return;
   wiiu_pad_drivers[port]->get_buttons(port, state);
}

static int16_t wiiu_joypad_axis(unsigned port, uint32_t joyaxis)
{
   if (!wiiu_joypad_query_pad(port))
      return 0;
   return wiiu_pad_drivers[port]->axis(port, joyaxis);
}

static int16_t wiiu_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   unsigned i;
   int16_t ret                          = 0;
   uint16_t port_idx                    = joypad_info->joy_idx;

   if (!wiiu_joypad_query_pad(port_idx))
      return 0;
   if (port_idx >= DEFAULT_MAX_PADS)
      return 0;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      if (
               (uint16_t)joykey != NO_BTN 
            && (wiiu_pad_drivers[port]->button(port_idx, (uint16_t)joykey))
         )
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(wiiu_pad_drivers[port]->axis(port_idx, joyaxis)) 
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
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

   return wiiu_pad_drivers[pad]->name(pad);
}

static void wiiu_joypad_connection_listener(unsigned pad,
               input_device_driver_t *driver)
{
   if (pad < MAX_USERS)
      wiiu_pad_drivers[pad] = driver;
}

input_device_driver_t wiiu_joypad =
{
  wiiu_joypad_init,
  wiiu_joypad_query_pad,
  wiiu_joypad_destroy,
  wiiu_joypad_button,
  wiiu_joypad_state,
  wiiu_joypad_get_buttons,
  wiiu_joypad_axis,
  wiiu_joypad_poll,
  NULL,
  NULL,
  wiiu_joypad_name,
  "wiiu",
};

pad_connection_listener_t wiiu_pad_connection_listener =
{
   wiiu_joypad_connection_listener
};
