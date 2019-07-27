/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <retro_miscellaneous.h>

#include "../../config.def.h"

#include "../input_driver.h"
#include "../input_keymaps.h"
#include "../drivers_keyboard/keyboard_event_dos.h"

/* TODO/FIXME -
 * fix game focus toggle */

typedef struct dos_input
{
   const input_device_driver_t *joypad;
} dos_input_t;

#define MAX_KEYS LAST_KEYCODE + 1

/* First ports are used to keeping track of gamepad states. Last port is used for keyboard state */
static uint16_t dos_key_state[DEFAULT_MAX_PADS+1][MAX_KEYS];

static bool dos_keyboard_port_input_pressed(
      const struct retro_keybind *binds, unsigned id)
{
   if (id < RARCH_BIND_LIST_END)
      return dos_key_state[DOS_KEYBOARD_PORT][rarch_keysym_lut[&binds[id].key]];
   return false;
}

uint16_t *dos_keyboard_state_get(unsigned port)
{
   return dos_key_state[port];
}

static void dos_keyboard_free(void)
{
   unsigned i, j;

   for (i = 0; i < DEFAULT_MAX_PADS; i++)
      for (j = 0; j < MAX_KEYS; j++)
         dos_key_state[i][j] = 0;
}

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
   dos_input_t *dos                   = (dos_input_t*)data;

   if (port > 0)
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            int16_t ret = 0;
            for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
            {
               /* Auto-binds are per joypad, not per user. */
               const uint64_t joykey  = (binds[port][i].joykey != NO_BTN)
                  ? binds[port][i].joykey : joypad_info.auto_binds[i].joykey;
               const uint32_t joyaxis = (binds[port][i].joyaxis != AXIS_NONE)
                  ? binds[port][i].joyaxis : joypad_info.auto_binds[i].joyaxis;
               
               if ((uint16_t)joykey != NO_BTN && dos->joypad->button(joypad_info.joy_idx, (uint16_t)joykey))
               {
                  ret |= (1 << i);
                  continue;
               }
               if (((float)abs(dos->joypad->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
               {
                  ret |= (1 << i);
                  continue;
               }
               if (dos_keyboard_port_input_pressed(binds[port], i))
               {
                  ret |= (1 << i);
                  continue;
               }
            }

            return ret;
         }
         else
         {
            /* Auto-binds are per joypad, not per user. */
            const uint64_t joykey  = (binds[port][id].joykey != NO_BTN)
               ? binds[port][id].joykey : joypad_info.auto_binds[id].joykey;
            const uint32_t joyaxis = (binds[port][id].joyaxis != AXIS_NONE)
               ? binds[port][id].joyaxis : joypad_info.auto_binds[id].joyaxis;

            if ((uint16_t)joykey != NO_BTN && dos->joypad->button(joypad_info.joy_idx, (uint16_t)joykey))
               return true;
            if (((float)abs(dos->joypad->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
               return true;

            if (dos_keyboard_port_input_pressed(binds[port], id))
               return true;
         }
         break;
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
   false
};
