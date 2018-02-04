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

#include <stdio.h>
#include <stdlib.h>

#include "../input_driver.h"
#include "../input_keymaps.h"
#include "../drivers_keyboard/keyboard_event_dos.h"

typedef struct dos_input
{
   const input_device_driver_t *joypad;
} dos_input_t;

static void dos_input_poll(void *data)
{
   dos_input_t *dos = (dos_input_t*)data;

   if (dos->joypad)
      dos->joypad->poll();
}

static int16_t dos_input_state(void *data,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   dos_input_t *dos = (dos_input_t*)data;

   if (port > 0)
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return input_joypad_pressed(dos->joypad, joypad_info, port, binds[port], id) ||
               dos_keyboard_port_input_pressed(binds[port], id);
      case RETRO_DEVICE_KEYBOARD:
         return dos_keyboard_port_input_pressed(binds[port], id);
   }

   return 0;
}

static void dos_input_free_input(void *data)
{
   dos_input_t *dos = (dos_input_t*)data;

   if (dos && dos->joypad)
      dos->joypad->destroy();

   dos_keyboard_free();

   if (data)
      free(data);
}

static void* dos_input_init(const char *joypad_driver)
{
   dos_input_t *dos = (dos_input_t*)calloc(1, sizeof(*dos));

   if (!dos)
      return NULL;

   dos_keyboard_free();

   dos->joypad = input_joypad_init_driver(joypad_driver, dos);

   input_keymaps_init_keyboard_lut(rarch_key_map_dos);

   return dos;
}

static uint64_t dos_input_get_capabilities(void *data)
{
   uint64_t caps = 0;

   caps |= UINT64_C(1) << RETRO_DEVICE_JOYPAD;

   return caps;
}

static const input_device_driver_t *dos_input_get_joypad_driver(void *data)
{
   dos_input_t *dos = (dos_input_t*)data;
   if (dos)
      return dos->joypad;
   return NULL;
}

static void dos_input_grab_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool dos_input_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   (void)data;
   (void)port;
   (void)effect;
   (void)strength;

   return false;
}

input_driver_t input_dos = {
   dos_input_init,
   dos_input_poll,
   dos_input_state,
   dos_input_free_input,
   NULL,
   NULL,
   dos_input_get_capabilities,
   "dos",
   dos_input_grab_mouse,
   NULL,
   dos_input_set_rumble,
   dos_input_get_joypad_driver,
   NULL,
   NULL,
   NULL
};
