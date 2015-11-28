/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2015 - Ali Bouhlel
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

#include "../../driver.h"
#include "../../libretro.h"
#include "../../general.h"
#include "../input_config.h"
#include "../input_joypad.h"

#define MAX_PADS 1

typedef struct ctr_input
{
   bool blocked;
   const input_device_driver_t *joypad;
} ctr_input_t;

uint64_t lifecycle_state;

static void ctr_input_poll(void *data)
{
   ctr_input_t *ctr = (ctr_input_t*)data;

   if (ctr->joypad)
      ctr->joypad->poll();
}

static int16_t ctr_input_state(void *data, const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   ctr_input_t *ctr = (ctr_input_t*)data;

   if (port > 0)
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return input_joypad_pressed(ctr->joypad, port, binds[port], id);
      case RETRO_DEVICE_ANALOG:
         return input_joypad_analog(ctr->joypad, port, idx, id, binds[port]);
   }

   return 0;
}

static void ctr_input_free_input(void *data)
{
   ctr_input_t *ctr = (ctr_input_t*)data;

   if (ctr && ctr->joypad)
      ctr->joypad->destroy();

   free(data);
}

static void* ctr_input_initialize(void)
{
   settings_t *settings = config_get_ptr();
   ctr_input_t *ctr = (ctr_input_t*)calloc(1, sizeof(*ctr));
   if (!ctr)
      return NULL;

   ctr->joypad = input_joypad_init_driver(settings->input.joypad_driver, ctr);

   return ctr;
}

static bool ctr_input_key_pressed(void *data, int key)
{
   settings_t *settings = config_get_ptr();
   ctr_input_t *ctr     = (ctr_input_t*)data;

   if (input_joypad_pressed(ctr->joypad, 0, settings->input.binds[0], key))
      return true;

   return false;
}

static bool ctr_input_meta_key_pressed(void *data, int key)
{
   if (BIT64_GET(lifecycle_state, key))
      return true;

   return false;
}

static uint64_t ctr_input_get_capabilities(void *data)
{
   (void)data;

   return (1 << RETRO_DEVICE_JOYPAD) |  (1 << RETRO_DEVICE_ANALOG);
}

static const input_device_driver_t *ctr_input_get_joypad_driver(void *data)
{
   ctr_input_t *ctr = (ctr_input_t*)data;
   if (ctr)
      return ctr->joypad;
   return NULL;
}

static void ctr_input_grab_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool ctr_input_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   (void)data;
   (void)port;
   (void)effect;
   (void)strength;

   return false;
}

static bool ctr_input_keyboard_mapping_is_blocked(void *data)
{
   ctr_input_t *ctr = (ctr_input_t*)data;
   if (!ctr)
      return false;
   return ctr->blocked;
}

static void ctr_input_keyboard_mapping_set_block(void *data, bool value)
{
   ctr_input_t *ctr = (ctr_input_t*)data;
   if (!ctr)
      return;
   ctr->blocked = value;
}

input_driver_t input_ctr = {
   ctr_input_initialize,
   ctr_input_poll,
   ctr_input_state,
   ctr_input_key_pressed,
   ctr_input_meta_key_pressed,
   ctr_input_free_input,
   NULL,
   NULL,
   ctr_input_get_capabilities,
   "ctr",
   ctr_input_grab_mouse,
   NULL,
   ctr_input_set_rumble,
   ctr_input_get_joypad_driver,
   NULL,
   ctr_input_keyboard_mapping_is_blocked,
   ctr_input_keyboard_mapping_set_block,
};
