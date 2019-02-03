/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <stdint.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <pad.h>

#include <boolean.h>
#include <libretro.h>
#include <retro_miscellaneous.h>

#include "../input_driver.h"

/* TODO/FIXME -
 * fix game focus toggle */

typedef struct ps4_input
{
   bool blocked;
   const input_device_driver_t *joypad;
} ps4_input_t;

static void ps4_input_poll(void *data)
{
   ps4_input_t *ps4 = (ps4_input_t*)data;

   if (ps4 && ps4->joypad)
      ps4->joypad->poll();
}

static int16_t ps4_input_state(void *data,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   ps4_input_t *ps4           = (ps4_input_t*)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return input_joypad_pressed(ps4->joypad, joypad_info, port, binds[port], id);
      case RETRO_DEVICE_ANALOG:
         if (binds[port])
            return input_joypad_analog(ps4->joypad, joypad_info, port, idx, id, binds[port]);
         break;
   }

   return 0;
}

static void ps4_input_free_input(void *data)
{
   ps4_input_t *ps4 = (ps4_input_t*)data;

   if (ps4 && ps4->joypad)
      ps4->joypad->destroy();

   free(data);
}

static void* ps4_input_initialize(const char *joypad_driver)
{
   ps4_input_t *ps4 = (ps4_input_t*)calloc(1, sizeof(*ps4));
   if (!ps4)
      return NULL;

   ps4->joypad = input_joypad_init_driver(joypad_driver, ps4);

   return ps4;
}

static uint64_t ps4_input_get_capabilities(void *data)
{
   (void)data;

   return (1 << RETRO_DEVICE_JOYPAD) |  (1 << RETRO_DEVICE_ANALOG);
}

static const input_device_driver_t *ps4_input_get_joypad_driver(void *data)
{
   ps4_input_t *ps4 = (ps4_input_t*)data;
   if (ps4)
      return ps4->joypad;
   return NULL;
}

static void ps4_input_grab_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool ps4_input_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   ps4_input_t *ps4 = (ps4_input_t*)data;

   if (ps4 && ps4->joypad)
      return input_joypad_set_rumble(ps4->joypad,
         port, effect, strength);
   return false;
}

static bool ps4_input_keyboard_mapping_is_blocked(void *data)
{
   ps4_input_t *ps4 = (ps4_input_t*)data;
   if (!ps4)
      return false;
   return ps4->blocked;
}

static void ps4_input_keyboard_mapping_set_block(void *data, bool value)
{
   ps4_input_t *ps4 = (ps4_input_t*)data;
   if (!ps4)
      return;
   ps4->blocked = value;
}

input_driver_t input_ps4 = {
   ps4_input_initialize,
   ps4_input_poll,
   ps4_input_state,
   ps4_input_free_input,
   NULL,
   NULL,
   ps4_input_get_capabilities,
   "ps4",
   ps4_input_grab_mouse,
   NULL,
   ps4_input_set_rumble,
   ps4_input_get_joypad_driver,
   NULL,
   ps4_input_keyboard_mapping_is_blocked,
   ps4_input_keyboard_mapping_set_block,
};
