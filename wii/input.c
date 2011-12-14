/*  SSNES - A Super Ninteno Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <gccore.h>
#include <ogc/pad.h>

#include "../driver.h"
#include "../libsnes.hpp"
#include <stdlib.h>

// Just plain pads for now.
static bool pad_state[5][SSNES_FIRST_META_KEY];
static bool g_quit;

static int16_t wii_input_state(void *data, const struct snes_keybind **binds,
      bool port, unsigned device,
      unsigned index, unsigned id)
{
   (void)data;
   (void)binds;
   (void)index;

   if (device != SNES_DEVICE_JOYPAD)
      return 0;

   unsigned player = 0;
   if (port == SNES_PORT_2 && device == SNES_DEVICE_MULTITAP)
      player = index + 1;
   else if (port == SNES_PORT_2)
      player = 1;

   return pad_state[player][id];
}

static void wii_free_input(void *data)
{
   (void)data;
}

static void reset_callback(void)
{
   g_quit = true;
}

static void *wii_input_init(void)
{
   PAD_Init();
   SYS_SetResetCallback(reset_callback);
   SYS_SetPowerCallback(reset_callback);
   return (void*)-1;
}

#define _B(btn) pad_state[i][SNES_DEVICE_ID_JOYPAD_##btn] = down & PAD_BUTTON_##btn

static void wii_input_poll(void *data)
{
   (void)data;

   unsigned pads = PAD_ScanPads();
   for (unsigned i = 0; i < pads; i++)
   {
      uint16_t down = PAD_ButtonsHeld(i);
      _B(B);
      _B(Y);
      pad_state[i][SNES_DEVICE_ID_JOYPAD_SELECT] = down & PAD_TRIGGER_Z;
      _B(START);
      _B(UP);
      _B(DOWN);
      _B(LEFT);
      _B(RIGHT);
      _B(A);
      _B(X);
      pad_state[i][SNES_DEVICE_ID_JOYPAD_L] = down & PAD_TRIGGER_L;
      pad_state[i][SNES_DEVICE_ID_JOYPAD_R] = down & PAD_TRIGGER_R;
   }
}

#undef _B

static bool wii_key_pressed(void *data, int key)
{
   (void)data;
   switch (key)
   {
      case SSNES_QUIT_KEY:
         return g_quit;
      default:
         return false;
   }
}

const input_driver_t input_wii = {
   .init = wii_input_init,
   .poll = wii_input_poll,
   .input_state = wii_input_state,
   .key_pressed = wii_key_pressed,
   .free = wii_free_input,
   .ident = "wii",
};

