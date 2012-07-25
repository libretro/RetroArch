/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2012 - Hans-Kristian Arntzen
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
#include <gccore.h>
#include <ogc/pad.h>
#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif
#include <string.h>

#include "../driver.h"
#include "../libretro.h"
#include <stdlib.h>

static bool pad_state[5][RARCH_FIRST_META_KEY];   /* Gamecube pads */
#ifdef HW_RVL
static bool wpad_state[5][RARCH_FIRST_META_KEY];  /* Wii Classic pads */
#endif

const struct platform_bind platform_keys[] = {
   { PAD_BUTTON_B, "(NGC) B button" },
   { PAD_BUTTON_A, "(NGC) A button" },
   { PAD_BUTTON_Y, "(NGC) Y button" },
   { PAD_BUTTON_X, "(NGC) X button" },
   { PAD_BUTTON_UP, "(NGC) D-Pad Up" },
   { PAD_BUTTON_DOWN, "(NGC) D-Pad Down" },
   { PAD_BUTTON_LEFT, "(NGC) D-Pad Left" },
   { PAD_BUTTON_RIGHT, "(NGC) D-Pad Right" },
   { PAD_TRIGGER_Z, "(NGC) Z trigger" },
   { PAD_BUTTON_START, "(NGC) Start button" },
   { PAD_TRIGGER_L, "(NGC) Left Trigger" },
   { PAD_TRIGGER_R, "(NGC) Right Trigger" },
#ifdef HW_RVL
   { WPAD_CLASSIC_BUTTON_B, "(Wii Classici) B button" },
   { WPAD_CLASSIC_BUTTON_A, "(Wii Classic) A button" },
   { WPAD_CLASSIC_BUTTON_Y, "(Wii Classic) Y button" },
   { WPAD_CLASSIC_BUTTON_X, "(Wii Classic) X button" },
   { WPAD_CLASSIC_BUTTON_UP, "(Wii Classic) D-Pad Up" },
   { WPAD_CLASSIC_BUTTON_DOWN, "(Wii Classic) D-Pad Down" },
   { WPAD_CLASSIC_BUTTON_LEFT, "(Wii Classic) D-Pad Left" },
   { WPAD_CLASSIC_BUTTON_RIGHT, "(Wii Classic) D-Pad Right" },
   { WPAD_CLASSIC_BUTTON_MINUS, "(Wii Classic) Select/Minus button" },
   { WPAD_CLASSIC_BUTTON_PLUS, "(Wii Classic) Start/Plus button" },
   { WPAD_CLASSIC_BUTTON_HOME, "(Wii Classic) Home button" },
   { WPAD_CLASSIC_BUTTON_FULL_L, "(Wii Classic) Left Trigger" },
   { WPAD_CLASSIC_BUTTON_FULL_R, "(Wii Classic) Right Trigger" },
   { WPAD_CLASSIC_BUTTON_ZL, "(Wii Classic) ZL button" },
   { WPAD_CLASSIC_BUTTON_ZR, "(Wii Classic) ZR button" },
#endif
};

const unsigned int platform_keys_size = sizeof(platform_keys);

static bool g_quit;

static int16_t wii_input_state(void *data, const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   (void)data;
   (void)binds;
   (void)index;

   unsigned player = port;

   return pad_state[player][id]
#ifdef HW_RVL
      || wpad_state[player][id]
#endif
   ;
}

static void wii_free_input(void *data)
{
   (void)data;
}

static void reset_callback(void)
{
   g_quit = true;
}

static void *wii_input_initialize(void)
{
   PAD_Init();
#ifdef HW_RVL
   WPAD_Init();
#endif
   SYS_SetResetCallback(reset_callback);
   SYS_SetPowerCallback(reset_callback);
   return (void*)-1;
}

static void wii_input_poll(void *data)
{
   (void)data;

   unsigned pads = PAD_ScanPads();
#ifdef HW_RVL
   unsigned wpads = WPAD_ScanPads();
#endif

   /* Gamecube controller */
   for (unsigned i = 0; i < pads; i++)
   {
      uint16_t down = PAD_ButtonsHeld(i) | PAD_ButtonsDown(i);
      down &= ~PAD_ButtonsUp(i);

      pad_state[i][RETRO_DEVICE_ID_JOYPAD_B] = down & PAD_BUTTON_B;
      pad_state[i][RETRO_DEVICE_ID_JOYPAD_Y] = down & PAD_BUTTON_Y;
      pad_state[i][RETRO_DEVICE_ID_JOYPAD_SELECT] = down & PAD_TRIGGER_Z;
      pad_state[i][RETRO_DEVICE_ID_JOYPAD_START] = down & PAD_BUTTON_START;
      pad_state[i][RETRO_DEVICE_ID_JOYPAD_UP] = down & PAD_BUTTON_UP;
      pad_state[i][RETRO_DEVICE_ID_JOYPAD_DOWN] = down & PAD_BUTTON_DOWN;
      pad_state[i][RETRO_DEVICE_ID_JOYPAD_LEFT] = down & PAD_BUTTON_LEFT;
      pad_state[i][RETRO_DEVICE_ID_JOYPAD_RIGHT] = down & PAD_BUTTON_RIGHT;
      pad_state[i][RETRO_DEVICE_ID_JOYPAD_A] = down & PAD_BUTTON_A;
      pad_state[i][RETRO_DEVICE_ID_JOYPAD_X] = down & PAD_BUTTON_X;
      pad_state[i][RETRO_DEVICE_ID_JOYPAD_L] = down & PAD_TRIGGER_L;
      pad_state[i][RETRO_DEVICE_ID_JOYPAD_R] = down & PAD_TRIGGER_R;
   }

#ifdef HW_RVL
   /* Wii Classic controller */
   for (unsigned i = 0; i < wpads; i++)
   {
      uint32_t down = WPAD_ButtonsHeld(i) | WPAD_ButtonsDown(i);
      down &= ~WPAD_ButtonsUp(i);

      wpad_state[i][RETRO_DEVICE_ID_JOYPAD_B] = down & (WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_B);
      wpad_state[i][RETRO_DEVICE_ID_JOYPAD_Y] = down & WPAD_CLASSIC_BUTTON_Y;
      wpad_state[i][RETRO_DEVICE_ID_JOYPAD_SELECT] = down & (WPAD_BUTTON_MINUS | WPAD_CLASSIC_BUTTON_MINUS);
      wpad_state[i][RETRO_DEVICE_ID_JOYPAD_START] = down & (WPAD_BUTTON_PLUS | WPAD_CLASSIC_BUTTON_PLUS);
      wpad_state[i][RETRO_DEVICE_ID_JOYPAD_UP] = down & (WPAD_BUTTON_UP | WPAD_CLASSIC_BUTTON_UP);
      wpad_state[i][RETRO_DEVICE_ID_JOYPAD_DOWN] = down & (WPAD_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_DOWN);
      wpad_state[i][RETRO_DEVICE_ID_JOYPAD_LEFT] = down & (WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT);
      wpad_state[i][RETRO_DEVICE_ID_JOYPAD_RIGHT] = down & (WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT);
      wpad_state[i][RETRO_DEVICE_ID_JOYPAD_A] = down & (WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A);
      wpad_state[i][RETRO_DEVICE_ID_JOYPAD_X] = down & WPAD_CLASSIC_BUTTON_X;
      wpad_state[i][RETRO_DEVICE_ID_JOYPAD_L] = down & WPAD_CLASSIC_BUTTON_FULL_L;
      wpad_state[i][RETRO_DEVICE_ID_JOYPAD_R] = down & WPAD_CLASSIC_BUTTON_FULL_R;

      if (down & (WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME) && i == 0)
         wpad_state[0][RETRO_DEVICE_ID_JOYPAD_L] = wpad_state[0][RETRO_DEVICE_ID_JOYPAD_R] =
            wpad_state[0][RETRO_DEVICE_ID_JOYPAD_START] = wpad_state[0][RETRO_DEVICE_ID_JOYPAD_SELECT] = true;
   }
#endif
}

static bool wii_key_pressed(void *data, int key)
{
   (void)data;
   switch (key)
   {
      case RARCH_QUIT_KEY:
      {
         bool r = g_quit ||
            (pad_state[0][RETRO_DEVICE_ID_JOYPAD_SELECT] &&
             pad_state[0][RETRO_DEVICE_ID_JOYPAD_START] &&
             pad_state[0][RETRO_DEVICE_ID_JOYPAD_L] &&
             pad_state[0][RETRO_DEVICE_ID_JOYPAD_R])
#ifdef HW_RVL
         ||
            (wpad_state[0][RETRO_DEVICE_ID_JOYPAD_SELECT] &&
             wpad_state[0][RETRO_DEVICE_ID_JOYPAD_START] &&
             wpad_state[0][RETRO_DEVICE_ID_JOYPAD_L] &&
             wpad_state[0][RETRO_DEVICE_ID_JOYPAD_R])
#endif
         ;
         g_quit = false;
         return r;
      }
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

