/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#include <sceerror.h>
#include <kernel.h>
#include <ctrl.h>

#include "../driver.h"
#include "../libretro.h"
#include "../general.h"

#define ANALOGSTICK_DEADZONE_LOW  (0x40)
#define ANALOGSTICK_DEADZONE_HIGH (0xc0

static uint64_t state;

static void psp2_input_poll(void *data)
{
   CellPadInfo2 pad_info;
   (void)data;

   for (unsigned i = 0; i < MAX_PADS; i++)
   {
      SceCtrlData state_tmp;
      int ret = sceCtrlReadBufferPositive(0, &state_tmp, 1);

      if (ret == SCE_OK)
      {
         state = 0;
         state |= (state_tmp.buttons & SCE_CTRL_LEFT) ? PSP2_GAMEPAD_DPAD_LEFT : 0;
         state |= (state_tmp.buttons & SCE_CTRL_DOWN) ? PSP2_GAMEPAD_DPAD_DOWN : 0;
         state |= (state_tmp.buttons & SCE_CTRL_RIGHT) ? PSP2_GAMEPAD_DPAD_RIGHT : 0;
         state |= (state_tmp.buttons & SCE_CTRL_UP) ? PSP2_GAMEPAD_DPAD_UP : 0;
         state |= (state_tmp.buttons & SCE_CTRL_START) ? PSP2_GAMEPAD_START : 0;
         state |= (state_tmp.buttons & SCE_CTRL_SELECT) ? PSP2_GAMEPAD_SELECT : 0;
         state |= (state_tmp.buttons & SCE_CTRL_TRIANGLE) ? PSP2_GAMEPAD_TRIANGLE : 0;
         state |= (state_tmp.buttons & SCE_CTRL_SQUARE) ? PSP2_GAMEPAD_SQUARE : 0;
         state |= (state_tmp.buttons & SCE_CTRL_CROSS) ? PSP2_GAMEPAD_CROSS : 0;
         state |= (state_tmp.buttons & SCE_CTRL_CIRCLE) ? PSP2_GAMEPAD_CIRCLE : 0;
         state |= (state_tmp.buttons & SCE_CTRL_R) ? PSP2_GAMEPAD_R : 0;
         state |= (state_tmp.buttons & SCE_CTRL_L) ? PSP2_GAMEPAD_L : 0;
         state |= (state_tmp.lx < ANALOGSTICK_DEADZONE_LOW) ? PSP2_GAMEPAD_LSTICK_LEFT_MASK : 0;
         state |= (state_tmp.lx > ANALOGSTICK_DEADZONE_HIGH) ? PSP2_GAMEPAD_LSTICK_RIGHT_MASK : 0;
         state |= (state_tmp.ly < ANALOGSTICK_DEADZONE_LOW) ? PSP2_GAMEPAD_LSTICK_UP_MASK : 0;
         state |= (state_tmp.ly > ANALOGSTICK_DEADZONE_HIGH) ? PSP2_GAMEPAD_LSTICK_DOWN_MASK : 0;
         state |= (state_tmp.rx < ANALOGSTICK_DEADZONE_LOW) ? PSP2_GAMEPAD_RSTICK_LEFT_MASK : 0;
         state |= (state_tmp.rx > ANALOGSTICK_DEADZONE_HIGH) ? PSP2_GAMEPAD_RSTICK_RIGHT_MASK : 0;
         state |= (state_tmp.ry < ANALOGSTICK_DEADZONE_LOW) ? PSP2_GAMEPAD_RSTICK_UP_MASK : 0;
         state |= (state_tmp.ry > ANALOGSTICK_DEADZONE_HIGH) ? PSP2_GAMEPAD_RSTICK_DOWN_MASK : 0;
      }
   }

   cellPadGetInfo2(&pad_info);
}

static int16_t psp2_input_state(void *data, const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   (void)data;

   uint64_t button = binds[0][id].joykey;
   int16_t retval = 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         retval = (state & button) ? 1 : 0;
         break;
   }

   return retval;
}

static void psp2_input_set_analog_dpad_mapping(unsigned device, unsigned map_dpad_enum, unsigned controller_id)
{
   (void)device;
}

static void psp2_free_input(void *data)
{
   (void)data;
}

static void* psp2_input_initialize(void)
{
   sceCtrlSetSamplingMode(SCE_CTRL_MODE_DIGITALANALOG);
   return (void*)-1;
}

static void psp2_input_post_init(void)
{
}

static bool psp2_key_pressed(void *data, int key)
{
   (void)data;

   switch (key)
   {
      default:
         return false;
   }
}

static void psp2_set_default_keybind_lut(unsigned device, unsigned port)
{
   (void)device;
   (void)port;
}

const input_driver_t input_psp2 = {
   .init = psp2_input_initialize,
   .poll = psp2_input_poll,
   .input_state = psp2_input_state,
   .key_pressed = psp2_key_pressed,
   .free = psp2_free_input,
   .set_default_keybind_lut = psp2_set_default_keybind_lut,
   .set_analog_dpad_mapping = psp2_input_set_analog_dpad_mapping,
   .post_init = psp2_input_post_init,
   .max_pads = MAX_PADS,
   .ident = "psp2",
};

