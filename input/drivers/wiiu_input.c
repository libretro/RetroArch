/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouhlel
 *  Copyright (C) 2014-2017 - Daniel De Matteis
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

#include <boolean.h>
#include <libretro.h>
#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../input_config.h"
#include "../input_driver.h"
#include "../input_joypad_driver.h"

#include "wiiu_dbg.h"

#define MAX_PADS 5

typedef struct wiiu_input
{
   bool blocked;
   const input_device_driver_t *joypad;
} wiiu_input_t;

uint64_t lifecycle_state;

static void wiiu_input_poll(void *data)
{
   wiiu_input_t *wiiu = (wiiu_input_t*)data;

   if (wiiu->joypad)
      wiiu->joypad->poll();
}

static int16_t wiiu_input_state(void *data,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   wiiu_input_t *wiiu         = (wiiu_input_t*)data;

   if(!wiiu || !(port < MAX_PADS) || !binds || !binds[port])
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return input_joypad_pressed(wiiu->joypad, joypad_info, port, binds[port], id);
      case RETRO_DEVICE_ANALOG:
         if (binds[port])
            return input_joypad_analog(wiiu->joypad, joypad_info, port, idx, id, binds[port]);
         break;
   }

   return 0;
}

static void wiiu_input_free_input(void *data)
{
   wiiu_input_t *wiiu = (wiiu_input_t*)data;

   if (wiiu && wiiu->joypad)
      wiiu->joypad->destroy();

   free(data);
}

static void* wiiu_input_init(const char *joypad_driver)
{
   wiiu_input_t *wiiu = (wiiu_input_t*)calloc(1, sizeof(*wiiu));
   if (!wiiu)
      return NULL;

   DEBUG_STR(joypad_driver);
   wiiu->joypad = input_joypad_init_driver(joypad_driver, wiiu);

   return wiiu;
}

static bool wiiu_input_meta_key_pressed(void *data, int key)
{
   if (BIT64_GET(lifecycle_state, key))
      return true;

   return false;
}

static uint64_t wiiu_input_get_capabilities(void *data)
{
   (void)data;

   return (1 << RETRO_DEVICE_JOYPAD) |  (1 << RETRO_DEVICE_ANALOG);
}

static const input_device_driver_t *wiiu_input_get_joypad_driver(void *data)
{
   wiiu_input_t *wiiu = (wiiu_input_t*)data;
   if (wiiu)
      return wiiu->joypad;
   return NULL;
}

static void wiiu_input_grab_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool wiiu_input_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   (void)data;
   (void)port;
   (void)effect;
   (void)strength;

   return false;
}

static bool wiiu_input_keyboard_mapping_is_blocked(void *data)
{
   wiiu_input_t *wiiu = (wiiu_input_t*)data;
   if (!wiiu)
      return false;
   return wiiu->blocked;
}

static void wiiu_input_keyboard_mapping_set_block(void *data, bool value)
{
   wiiu_input_t *wiiu = (wiiu_input_t*)data;
   if (!wiiu)
      return;
   wiiu->blocked = value;
}

input_driver_t input_wiiu = {
   wiiu_input_init,
   wiiu_input_poll,
   wiiu_input_state,
   wiiu_input_meta_key_pressed,
   wiiu_input_free_input,
   NULL,
   NULL,
   wiiu_input_get_capabilities,
   "wiiu",
   wiiu_input_grab_mouse,
   NULL,
   wiiu_input_set_rumble,
   wiiu_input_get_joypad_driver,
   NULL,
   wiiu_input_keyboard_mapping_is_blocked,
   wiiu_input_keyboard_mapping_set_block,
};
