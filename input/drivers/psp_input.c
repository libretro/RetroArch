/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <stdint.h>
#include <stdlib.h>

#include <boolean.h>

#if defined(SN_TARGET_PSP2)
#include <sceerror.h>
#include <kernel.h>
#include <ctrl.h>
#elif defined(VITA)
#include <psp2/ctrl.h>
#elif defined(PSP)
#include <pspctrl.h>
#endif

#include "../../defines/psp_defines.h"

#include "../../driver.h"
#include "../../libretro.h"
#include "../../general.h"
#include "../input_config.h"
#ifdef HAVE_KERNEL_PRX
#include "../../bootstrap/psp1/kernel_functions.h"
#endif

#define MAX_PADS 1

typedef struct psp_input
{
   bool blocked;
   const input_device_driver_t *joypad;
} psp_input_t;

uint64_t lifecycle_state;

static void psp_input_poll(void *data)
{
   psp_input_t *psp = (psp_input_t*)data;

   if (psp->joypad)
      psp->joypad->poll();
}

static int16_t psp_input_state(void *data, const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   psp_input_t *psp = (psp_input_t*)data;

   if (port > 0)
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return input_joypad_pressed(psp->joypad, port, binds[port], id);
      case RETRO_DEVICE_ANALOG:
         return input_joypad_analog(psp->joypad, port, idx, id, binds[port]);
   }

   return 0;
}

static void psp_input_free_input(void *data)
{
   psp_input_t *psp = (psp_input_t*)data;

   if (psp && psp->joypad)
      psp->joypad->destroy();

   free(data);
}

static void* psp_input_initialize(void)
{
   settings_t *settings = config_get_ptr();
   psp_input_t *psp = (psp_input_t*)calloc(1, sizeof(*psp));
   if (!psp)
      return NULL;

   psp->joypad = input_joypad_init_driver(
         settings->input.joypad_driver, psp);

   return psp;
}

static bool psp_input_key_pressed(void *data, int key)
{
   settings_t *settings = config_get_ptr();
   psp_input_t *psp     = (psp_input_t*)data;

   if (input_joypad_pressed(psp->joypad, 0, settings->input.binds[0], key))
      return true;

   return false;
}

static bool psp_input_meta_key_pressed(void *data, int key)
{
   return (BIT64_GET(lifecycle_state, key));
}

static uint64_t psp_input_get_capabilities(void *data)
{
   (void)data;

   return (1 << RETRO_DEVICE_JOYPAD) |  (1 << RETRO_DEVICE_ANALOG);
}

static const input_device_driver_t *psp_input_get_joypad_driver(void *data)
{
   psp_input_t *psp = (psp_input_t*)data;
   if (psp)
      return psp->joypad;
   return NULL;
}

static void psp_input_grab_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool psp_input_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   (void)data;
   (void)port;
   (void)effect;
   (void)strength;

   return false;
}

static bool psp_input_keyboard_mapping_is_blocked(void *data)
{
   psp_input_t *psp = (psp_input_t*)data;
   if (!psp)
      return false;
   return psp->blocked;
}

static void psp_input_keyboard_mapping_set_block(void *data, bool value)
{
   psp_input_t *psp = (psp_input_t*)data;
   if (!psp)
      return;
   psp->blocked = value;
}

input_driver_t input_psp = {
   psp_input_initialize,
   psp_input_poll,
   psp_input_state,
   psp_input_key_pressed,
   psp_input_meta_key_pressed,
   psp_input_free_input,
   NULL,
   NULL,
   psp_input_get_capabilities,
#ifdef VITA
   "vita",
#else
   "psp",
#endif

   psp_input_grab_mouse,
   NULL,
   psp_input_set_rumble,
   psp_input_get_joypad_driver,
   NULL,
   psp_input_keyboard_mapping_is_blocked,
   psp_input_keyboard_mapping_set_block,
};
