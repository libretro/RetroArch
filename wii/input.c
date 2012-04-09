/*  SSNES - A frontend for libretro.
 *  Copyright (C) 2012 - Hans-Kristian Arntzen
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
#include <wiiuse/wpad.h>
#include <string.h>

#include "../driver.h"
#include "../libretro.h"
#include <stdlib.h>

// Just plain pads for now.
static bool pad_state[5][SSNES_FIRST_META_KEY];
static bool wpad_state[5][SSNES_FIRST_META_KEY];
static bool g_quit;

static int16_t wii_input_state(void *data, const struct snes_keybind **binds,
      unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   (void)data;
   (void)binds;
   (void)index;

   unsigned player = port;

   return pad_state[player][id] || wpad_state[player][id];
}

static void wii_free_input(void *data)
{
   (void)data;
}

static void reset_callback(void)
{
   g_quit = true;
}

void wii_input_init(void)
{
   PAD_Init();
   WPAD_Init();
   SYS_SetResetCallback(reset_callback);
   SYS_SetPowerCallback(reset_callback);
}

void wii_input_deinit(void)
{}

static void *wii_input_initialize(void)
{
   return (void*)-1;
}

static void wii_input_poll(void *data)
{
   (void)data;

   unsigned pads = PAD_ScanPads();
   unsigned wpads = WPAD_ScanPads();
#define _BIND(btn) pad_state[i][SNES_DEVICE_ID_JOYPAD_##btn] = down & PAD_BUTTON_##btn
   for (unsigned i = 0; i < pads; i++)
   {
      uint16_t down = PAD_ButtonsHeld(i) | PAD_ButtonsDown(i);
      down &= ~PAD_ButtonsUp(i);

      _BIND(B);
      _BIND(Y);
      pad_state[i][SNES_DEVICE_ID_JOYPAD_SELECT] = down & PAD_TRIGGER_Z;
      _BIND(START);
      _BIND(UP);
      _BIND(DOWN);
      _BIND(LEFT);
      _BIND(RIGHT);
      _BIND(A);
      _BIND(X);
      pad_state[i][SNES_DEVICE_ID_JOYPAD_L] = down & PAD_TRIGGER_L;
      pad_state[i][SNES_DEVICE_ID_JOYPAD_R] = down & PAD_TRIGGER_R;
   }

#undef _BIND
#define _BIND(btn) \
   wpad_state[i][SNES_DEVICE_ID_JOYPAD_##btn] = down & WPAD_CLASSIC_BUTTON_##btn;

   for (unsigned i = 0; i < wpads; i++)
   {
      uint32_t down = WPAD_ButtonsHeld(i) | WPAD_ButtonsDown(i);
      down &= ~WPAD_ButtonsUp(i);

      _BIND(B);
      _BIND(Y);
      wpad_state[i][SNES_DEVICE_ID_JOYPAD_SELECT] = down & WPAD_CLASSIC_BUTTON_MINUS;
      wpad_state[i][SNES_DEVICE_ID_JOYPAD_START] = down & WPAD_CLASSIC_BUTTON_PLUS;
      _BIND(UP);
      _BIND(DOWN);
      _BIND(LEFT);
      _BIND(RIGHT);
      _BIND(A);
      _BIND(X);
      wpad_state[i][SNES_DEVICE_ID_JOYPAD_L] = down & WPAD_CLASSIC_BUTTON_FULL_L;
      wpad_state[i][SNES_DEVICE_ID_JOYPAD_R] = down & WPAD_CLASSIC_BUTTON_FULL_R;
   }
}

static bool wii_key_pressed(void *data, int key)
{
   (void)data;
   switch (key)
   {
      case SSNES_QUIT_KEY:
         return g_quit ||
            (pad_state[0][SNES_DEVICE_ID_JOYPAD_SELECT] &&
             pad_state[0][SNES_DEVICE_ID_JOYPAD_START] &&
             pad_state[0][SNES_DEVICE_ID_JOYPAD_L] &&
             pad_state[0][SNES_DEVICE_ID_JOYPAD_R]) ||
            (wpad_state[0][SNES_DEVICE_ID_JOYPAD_SELECT] &&
             wpad_state[0][SNES_DEVICE_ID_JOYPAD_START] &&
             wpad_state[0][SNES_DEVICE_ID_JOYPAD_L] &&
             wpad_state[0][SNES_DEVICE_ID_JOYPAD_R]);
      default:
         return false;
   }
}

const input_driver_t input_wii = {
   .init = wii_input_initialize,
   .poll = wii_input_poll,
   .input_state = wii_input_state,
   .key_pressed = wii_key_pressed,
   .free = wii_free_input,
   .ident = "wii",
};

