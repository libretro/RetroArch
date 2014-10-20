/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#if defined(SN_TARGET_PSP2)
#include <sceerror.h>
#include <kernel.h>
#include <ctrl.h>
#elif defined(PSP)
#include <pspctrl.h>
#endif

#include "../gfx/psp/sdk_defines.h"

#include "../driver.h"
#include "../libretro.h"
#include "../general.h"
#ifdef HAVE_KERNEL_PRX
#include "../psp1/kernel_functions.h"
#endif

#define MAX_PADS 1

typedef struct psp_input
{
   const rarch_joypad_driver_t *joypad;
} psp_input_t;

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
   psp_input_t *psp = (psp_input_t*)calloc(1, sizeof(*psp));
   if (!psp)
      return NULL;
   
   psp->joypad = input_joypad_init_driver(g_settings.input.joypad_driver);   

   return psp;
}

static bool psp_input_key_pressed(void *data, int key)
{
   psp_input_t *psp = (psp_input_t*)data;
   return (g_extern.lifecycle_state & (1ULL << key)) || 
      input_joypad_pressed(psp->joypad, 0, g_settings.input.binds[0], key);
}

static uint64_t psp_input_get_capabilities(void *data)
{
   (void)data;

   return (1 << RETRO_DEVICE_JOYPAD) |  (1 << RETRO_DEVICE_ANALOG);
}

static const rarch_joypad_driver_t *psp_input_get_joypad_driver(void *data)
{
   psp_input_t *psp = (psp_input_t*)data;
   if (psp)
      return psp->joypad;
   return NULL;
}

input_driver_t input_psp = {
   psp_input_initialize,
   psp_input_poll,
   psp_input_state,
   psp_input_key_pressed,
   psp_input_free_input,
   NULL,
   NULL,
   psp_input_get_capabilities,
   "psp",

   NULL,
   NULL,
   psp_input_get_joypad_driver,
};
