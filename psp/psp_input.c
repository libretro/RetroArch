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

#include "sdk_defines.h"
#include "psp_input.h"

#include "../driver.h"
#include "../libretro.h"
#include "../general.h"

#define ANALOGSTICK_DEADZONE_LOW  (0x40)
#define ANALOGSTICK_DEADZONE_HIGH (0xc0)

#define MAX_PADS 1

const struct platform_bind platform_keys[] = {
   { PSP_GAMEPAD_CIRCLE, "Circle button" },
   { PSP_GAMEPAD_CROSS, "Cross button" },
   { PSP_GAMEPAD_TRIANGLE, "Triangle button" },
   { PSP_GAMEPAD_SQUARE, "Square button" },
   { PSP_GAMEPAD_DPAD_UP, "D-Pad Up" },
   { PSP_GAMEPAD_DPAD_DOWN, "D-Pad Down" },
   { PSP_GAMEPAD_DPAD_LEFT, "D-Pad Left" },
   { PSP_GAMEPAD_DPAD_RIGHT, "D-Pad Right" },
   { PSP_GAMEPAD_SELECT, "Select button" },
   { PSP_GAMEPAD_START, "Start button" },
   { PSP_GAMEPAD_L, "L button" },
   { 0, "Unused" },
   { 0, "Unused" },
   { PSP_GAMEPAD_R, "R button" },
   { 0, "Unused" },
   { 0, "Unused" },
   { PSP_GAMEPAD_LSTICK_LEFT_MASK, "LStick Left" },
   { PSP_GAMEPAD_LSTICK_RIGHT_MASK, "LStick Right" },
   { PSP_GAMEPAD_LSTICK_UP_MASK, "LStick Up" },
   { PSP_GAMEPAD_LSTICK_DOWN_MASK, "LStick Down" },
#ifdef SN_TARGET_PSP2
   { PSP_GAMEPAD_RSTICK_LEFT_MASK, "RStick Left" },
   { PSP_GAMEPAD_RSTICK_RIGHT_MASK, "RStick Right" },
   { PSP_GAMEPAD_RSTICK_UP_MASK, "RStick Up" },
   { PSP_GAMEPAD_RSTICK_DOWN_MASK, "RStick Down" },
#else
   { 0, "Unused" },
   { 0, "Unused" },
   { 0, "Unused" },
   { 0, "Unused" },
   { 0, "Unused" },
   { 0, "Unused" },
   { 0, "Unused" },
   { 0, "Unused" },
#endif
};

static uint64_t state;

static void psp_input_poll(void *data)
{
   (void)data;

   SceCtrlData state_tmp;
   int ret = CtrlReadBufferPositive(0, &state_tmp, 1);

   if (ret == SCE_OK)
   {
      state = 0;
      state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_LEFT) ? PSP_GAMEPAD_DPAD_LEFT : 0;
      state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_DOWN) ? PSP_GAMEPAD_DPAD_DOWN : 0;
      state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_RIGHT) ? PSP_GAMEPAD_DPAD_RIGHT : 0;
      state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_UP) ? PSP_GAMEPAD_DPAD_UP : 0;
      state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_START) ? PSP_GAMEPAD_START : 0;
      state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_SELECT) ? PSP_GAMEPAD_SELECT : 0;
      state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_TRIANGLE) ? PSP_GAMEPAD_TRIANGLE : 0;
      state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_SQUARE) ? PSP_GAMEPAD_SQUARE : 0;
      state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_CROSS) ? PSP_GAMEPAD_CROSS : 0;
      state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_CIRCLE) ? PSP_GAMEPAD_CIRCLE : 0;
      state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_R) ? PSP_GAMEPAD_R : 0;
      state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_L) ? PSP_GAMEPAD_L : 0;
      state |= (STATE_ANALOGLX(state_tmp) < ANALOGSTICK_DEADZONE_LOW) ? PSP_GAMEPAD_LSTICK_LEFT_MASK : 0;
      state |= (STATE_ANALOGLX(state_tmp) > ANALOGSTICK_DEADZONE_HIGH) ? PSP_GAMEPAD_LSTICK_RIGHT_MASK : 0;
      state |= (STATE_ANALOGLY(state_tmp) < ANALOGSTICK_DEADZONE_LOW) ? PSP_GAMEPAD_LSTICK_UP_MASK : 0;
      state |= (STATE_ANALOGLY(state_tmp) > ANALOGSTICK_DEADZONE_HIGH) ? PSP_GAMEPAD_LSTICK_DOWN_MASK : 0;
#ifdef SN_TARGET_PSP2
      state |= (STATE_ANALOGRX(state_tmp) < ANALOGSTICK_DEADZONE_LOW) ? PSP_GAMEPAD_RSTICK_LEFT_MASK : 0;
      state |= (STATE_ANALOGRX(state_tmp) > ANALOGSTICK_DEADZONE_HIGH) ? PSP_GAMEPAD_RSTICK_RIGHT_MASK : 0;
      state |= (STATE_ANALOGRY(state_tmp) < ANALOGSTICK_DEADZONE_LOW) ? PSP_GAMEPAD_RSTICK_UP_MASK : 0;
      state |= (STATE_ANALOGRY(state_tmp) > ANALOGSTICK_DEADZONE_HIGH) ? PSP_GAMEPAD_RSTICK_DOWN_MASK : 0;
#endif
   }
}

static int16_t psp_input_state(void *data, const struct retro_keybind **binds,
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

static void psp_input_free_input(void *data)
{
   (void)data;
}

static void psp_input_set_keybinds(void *data, unsigned device, unsigned port,
      unsigned id, unsigned keybind_action)
{
   (void)device;
   (void)id;

   if (keybind_action & (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS))
   {
      for (unsigned i = 0; i < RARCH_CUSTOM_BIND_LIST_END; i++)
      {
         g_settings.input.binds[port][i].id = i;
         g_settings.input.binds[port][i].def_joykey = platform_keys[i].joykey;
         g_settings.input.binds[port][i].joykey = g_settings.input.binds[port][i].def_joykey;
      }
   }
}

static void* psp_input_initialize(void)
{
#ifdef PSP
   sceCtrlSetSamplingCycle(0);
#endif
   sceCtrlSetSamplingMode(DEFAULT_SAMPLING_MODE);

   return (void*)-1;
}

static bool psp_input_key_pressed(void *data, int key)
{
   (void)data;

   switch (key)
   {
      case RARCH_QUIT_KEY:
         RARCH_LOG("Got to here once.\n");
         return true;
      default:
         return false;
   }
}

static uint64_t psp_input_get_capabilities(void *data)
{
   uint64_t caps = 0;

   caps |= (1 << RETRO_DEVICE_JOYPAD);

   return caps;
}

const input_driver_t input_psp = {
   psp_input_initialize,
   psp_input_poll,
   psp_input_state,
   psp_input_key_pressed,
   psp_input_free_input,
   psp_input_set_keybinds,
   NULL,
   NULL,
   psp_input_get_capabilities,
   "psp",
};
