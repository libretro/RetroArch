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



static bool ready = false;

wiiu_joypad_t joypad_state = {0};

static void *wiiu_joypad_init(void *data)
{
   memset(&joypad_state, 0, sizeof(wiiu_joypad_t));
   joypad_state.pads[MAX_USERS].data = (void *)0xdeadbeef;
   joypad_state.max_slot = MAX_USERS;
   input_hid_init_first();

   wpad_driver.init(data);
   kpad_driver.init(data);
   hidpad_driver.init(data);
   ready = true;

   return (void *)-1;
}

static bool wiiu_joypad_query_pad(unsigned pad)
{
   return ready   &&
      pad < MAX_USERS          &&
      joypad_state.pads[pad].input_driver &&
      joypad_state.pads[pad].input_driver->query_pad(pad);
}

static void wiiu_joypad_destroy(void)
{
   ready = false;

   wpad_driver.destroy();
   kpad_driver.destroy();
   hidpad_driver.destroy();
}

static int32_t wiiu_joypad_button(unsigned port, uint16_t joykey)
{
   if (!wiiu_joypad_query_pad(port))
      return 0;
   if (port >= DEFAULT_MAX_PADS)
      return 0;
   return (joypad_state.pads[port].input_driver->button(port, joykey));
}

static void wiiu_joypad_get_buttons(unsigned port, input_bits_t *state)
{
   if (!wiiu_joypad_query_pad(port))
      return;
   joypad_state.pads[port].input_driver->get_buttons(port, state);
}

static int16_t wiiu_joypad_axis(unsigned port, uint32_t joyaxis)
{
   if (!wiiu_joypad_query_pad(port))
      return 0;
   return joypad_state.pads[port].input_driver->axis(port, joyaxis);
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
            && (joypad_state.pads[port].input_driver->button(port_idx, (uint16_t)joykey))
         )
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(joypad_state.pads[port].input_driver->axis(port_idx, joyaxis)) 
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static void wiiu_joypad_poll(void)
{
   wpad_driver.poll();
   kpad_driver.poll();
   hidpad_driver.poll();
}

static const char* wiiu_joypad_name(unsigned pad)
{
   if (!wiiu_joypad_query_pad(pad))
      return "N/A";

   return joypad_state.pads[pad].input_driver->name(pad);
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
