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

#include "../../tasks/tasks_internal.h"
#include "../input_driver.h"

/* TODO/FIXME - static global */
static const hid_driver_t *generic_hid = NULL;

static void *hid_joypad_init(void *data)
{
   generic_hid = input_hid_init_first();
   if (!generic_hid)
      return NULL;
   return (void*)-1;
}

static bool hid_joypad_query_pad(unsigned pad)
{
   if (generic_hid && generic_hid->query_pad)
      return generic_hid->query_pad((void*)hid_driver_get_data(), pad);
   return false;
}

static void hid_joypad_free(void)
{
   if (!generic_hid)
       return;

   if (generic_hid->free)
      generic_hid->free((void*)hid_driver_get_data());

   generic_hid = NULL;
}

static int32_t hid_joypad_button(unsigned port, uint16_t joykey)
{
   if (generic_hid && generic_hid->button)
      return generic_hid->button((void*)hid_driver_get_data(), port, joykey);
   return 0;
}

static int16_t hid_joypad_axis(unsigned port, uint32_t joyaxis)
{
   if (generic_hid && generic_hid->axis)
      return generic_hid->axis((void*)hid_driver_get_data(), port, joyaxis);
   return 0;
}

static int16_t hid_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   unsigned i;
   int16_t ret                          = 0;
   uint16_t port_idx                    = joypad_info->joy_idx;

   if (port_idx >= DEFAULT_MAX_PADS)
      return 0;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      if ((uint16_t)joykey != NO_BTN && hid_joypad_button(
               port_idx, (uint16_t)joykey))
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(hid_joypad_axis(port_idx, joyaxis)) 
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static void hid_joypad_get_buttons(unsigned port, input_bits_t *state)
{
   if (generic_hid && generic_hid->get_buttons)
      generic_hid->get_buttons((void*)hid_driver_get_data(), port, state);
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static void hid_joypad_poll(void)
{
   if (generic_hid && generic_hid->poll)
      generic_hid->poll((void*)hid_driver_get_data());
}

static bool hid_joypad_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   if (generic_hid && generic_hid->set_rumble)
      return generic_hid->set_rumble((void*)hid_driver_get_data(), pad, effect, strength);
   return false;
}

static const char *hid_joypad_name(unsigned pad)
{
   if (generic_hid && generic_hid->name)
      return generic_hid->name((void*)hid_driver_get_data(), pad);
   return NULL;
}

input_device_driver_t hid_joypad = {
   hid_joypad_init,
   hid_joypad_query_pad,
   hid_joypad_free,
   hid_joypad_button,
   hid_joypad_state,
   hid_joypad_get_buttons,
   hid_joypad_axis,
   hid_joypad_poll,
   hid_joypad_rumble,
   NULL,
   hid_joypad_name,
   "hid"
};
