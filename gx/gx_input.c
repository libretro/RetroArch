/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
 *  Copyright (C) 2012 - Michael Lelli
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
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327
#endif

#include "gx_input.h"
#include "../driver.h"
#include "../libretro.h"
#include <stdlib.h>

#define GC_JOYSTICK_THRESHOLD (48 * 256)
#define WII_JOYSTICK_THRESHOLD (40 * 256)

#define MAX_PADS 4

static u32 pad_connect[MAX_PADS];
static u32 pad_type[MAX_PADS];
static u32 pad_detect_pending[MAX_PADS];
static uint64_t pad_state[MAX_PADS];
static int16_t analog_state[MAX_PADS][2][2];

const struct platform_bind platform_keys[] = {
   { GX_GC_A, "GC A button" },
   { GX_GC_B, "GC B button" },
   { GX_GC_X, "GC X button" },
   { GX_GC_Y, "GC Y button" },
   { GX_GC_UP, "GC D-Pad Up" },
   { GX_GC_DOWN, "GC D-Pad Down" },
   { GX_GC_LEFT, "GC D-Pad Left" },
   { GX_GC_RIGHT, "GC D-Pad Right" },
   { GX_GC_Z_TRIGGER, "GC Z Trigger" },
   { GX_GC_START, "GC Start button" },
   { GX_GC_L_TRIGGER, "GC Left Trigger" },
   { GX_GC_R_TRIGGER, "GC Right Trigger" },

#ifdef HW_RVL
   // CLASSIC CONTROLLER
   { GX_CLASSIC_A, "Classic A button" },
   { GX_CLASSIC_B, "Classic B button" },
   { GX_CLASSIC_X, "Classic X button" },
   { GX_CLASSIC_Y, "Classic Y button" },
   { GX_CLASSIC_UP, "Classic D-Pad Up" },
   { GX_CLASSIC_DOWN, "Classic D-Pad Down" },
   { GX_CLASSIC_LEFT, "Classic D-Pad Left" },
   { GX_CLASSIC_RIGHT, "Classic D-Pad Right" },
   { GX_CLASSIC_PLUS, "Classic Plus button" },
   { GX_CLASSIC_MINUS, "Classic Minus button" },
   { GX_CLASSIC_HOME, "Classic Home button" },
   { GX_CLASSIC_L_TRIGGER, "Classic L Trigger" },
   { GX_CLASSIC_R_TRIGGER, "Classic R Trigger" },
   { GX_CLASSIC_ZL_TRIGGER, "Classic ZL Trigger" },
   { GX_CLASSIC_ZR_TRIGGER, "Classic ZR Trigger" },

   // WIIMOTE (PLUS OPTIONAL NUNCHUK)
   { GX_WIIMOTE_A, "Wiimote A button" },
   { GX_WIIMOTE_B, "Wiimote B button" },
   { GX_WIIMOTE_1, "Wiimote 1 button" },
   { GX_WIIMOTE_2, "Wiimote 2 button" },
   { GX_WIIMOTE_UP, "Wiimote D-Pad Up" },
   { GX_WIIMOTE_DOWN, "Wiimote D-Pad Down" },
   { GX_WIIMOTE_LEFT, "Wiimote D-Pad Left" },
   { GX_WIIMOTE_RIGHT, "Wiimote D-Pad Right" },
   { GX_WIIMOTE_PLUS, "Wiimote Plus button" },
   { GX_WIIMOTE_MINUS, "Wiimote Minus button" },
   { GX_WIIMOTE_HOME, "Wiimote Home button" },
   { GX_NUNCHUK_Z, "Nunchuk Z button" },
   { GX_NUNCHUK_C, "Nunchuk C button" },
   { GX_NUNCHUK_LEFT, "Nunchuk Stick Left" },
   { GX_NUNCHUK_RIGHT, "Nunchuk Stick Right" },
   { GX_NUNCHUK_UP, "Nunchuk Stick Up" },
   { GX_NUNCHUK_DOWN, "Nunchuk Stick Down" },
#endif
};

static bool g_menu;

#ifdef HW_RVL
static bool g_quit;
#endif

static int16_t gx_input_state(void *data, const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   (void)data;

   if (port >= MAX_PADS)
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return (binds[port][id].joykey & pad_state[port]) ? 1 : 0;
      case RETRO_DEVICE_ANALOG:
         return analog_state[port][index][id];
      default:
         return 0;
   }
}

static void gx_input_free_input(void *data)
{
   (void)data;
}

static void reset_callback(void)
{
   g_menu = true;
}

#ifdef HW_RVL
static void power_callback(void)
{
   g_quit = true;
}
#endif

static void gx_input_set_keybinds(void *data, unsigned device, unsigned port,
      unsigned id, unsigned keybind_action)
{
   uint64_t *key = &g_settings.input.binds[port][id].joykey;
   size_t arr_size = sizeof(platform_keys) / sizeof(platform_keys[0]);

   (void)device;

   if (keybind_action & (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BIND))
      *key = g_settings.input.binds[port][id].def_joykey;

   if (keybind_action & (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS))
   {
      switch (device)
      {
#ifdef HW_RVL
         case DEVICE_WIIMOTE:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Wiimote", sizeof(g_settings.input.device_names[port]));
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].def_joykey      = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_1].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].def_joykey      = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_A].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].def_joykey = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_MINUS].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].def_joykey  = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_PLUS].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_UP].def_joykey     = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_UP].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_DOWN].def_joykey   = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_DOWN].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_LEFT].def_joykey   = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_LEFT].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_RIGHT].def_joykey  = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_RIGHT].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].def_joykey      = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_2].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].def_joykey      = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_B].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].def_joykey     = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L2].def_joykey     = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R2].def_joykey     = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L3].def_joykey     = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R3].def_joykey     = NO_BTN;
            break;
         case DEVICE_NUNCHUK:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Wiimote + Nunchuk", sizeof(g_settings.input.device_names[port]));
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].def_joykey      = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_B].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].def_joykey      = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_2].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].def_joykey = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_MINUS].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].def_joykey  = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_PLUS].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_UP].def_joykey     = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_UP].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_DOWN].def_joykey   = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_DOWN].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_LEFT].def_joykey   = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_LEFT].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_RIGHT].def_joykey  = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_RIGHT].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].def_joykey      = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_A].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].def_joykey      = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_1].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].def_joykey      = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_Z].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].def_joykey      = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_C].joykey;;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L2].def_joykey     = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R2].def_joykey     = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L3].def_joykey     = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R3].def_joykey     = NO_BTN; break;
         case DEVICE_CLASSIC:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Classic Controller", sizeof(g_settings.input.device_names[port]));
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].def_joykey      = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_B].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].def_joykey      = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_Y].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].def_joykey = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_MINUS].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].def_joykey  = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_PLUS].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_UP].def_joykey     = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_UP].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_DOWN].def_joykey   = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_DOWN].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_LEFT].def_joykey   = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_LEFT].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_RIGHT].def_joykey  = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_RIGHT].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].def_joykey      = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_A].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].def_joykey      = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_X].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].def_joykey      = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_L_TRIGGER].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].def_joykey      = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_R_TRIGGER].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L2].def_joykey     = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_ZL_TRIGGER].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R2].def_joykey     = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_ZR_TRIGGER].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L3].def_joykey     = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R3].def_joykey     = NO_BTN;
            break;
#endif
         case DEVICE_GAMECUBE:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Gamecube Controller", sizeof(g_settings.input.device_names[port]));
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].def_joykey      = platform_keys[GX_DEVICE_GC_ID_JOYPAD_B].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].def_joykey      = platform_keys[GX_DEVICE_GC_ID_JOYPAD_Y].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].def_joykey = platform_keys[GX_DEVICE_GC_ID_JOYPAD_Z_TRIGGER].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].def_joykey  = platform_keys[GX_DEVICE_GC_ID_JOYPAD_START].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_UP].def_joykey     = platform_keys[GX_DEVICE_GC_ID_JOYPAD_UP].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_DOWN].def_joykey   = platform_keys[GX_DEVICE_GC_ID_JOYPAD_DOWN].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_LEFT].def_joykey  = platform_keys[GX_DEVICE_GC_ID_JOYPAD_LEFT].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_RIGHT].def_joykey  = platform_keys[GX_DEVICE_GC_ID_JOYPAD_RIGHT].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].def_joykey      = platform_keys[GX_DEVICE_GC_ID_JOYPAD_A].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].def_joykey      = platform_keys[GX_DEVICE_GC_ID_JOYPAD_X].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].def_joykey      = platform_keys[GX_DEVICE_GC_ID_JOYPAD_L_TRIGGER].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].def_joykey      = platform_keys[GX_DEVICE_GC_ID_JOYPAD_R_TRIGGER].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L2].def_joykey     = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R2].def_joykey     = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L3].def_joykey     = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R3].def_joykey     = NO_BTN;
            break;
         default:
            break;
      }

      for (unsigned i = 0; i < RARCH_CUSTOM_BIND_LIST_END; i++)
      {
         g_settings.input.binds[port][i].id = i;
         g_settings.input.binds[port][i].joykey = g_settings.input.binds[port][i].def_joykey;
      }
   }
   
   if (keybind_action & (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL))
   {
      struct platform_bind *ret = (struct platform_bind*)data;

      if (ret->joykey == NO_BTN)
         strlcpy(ret->desc, "No button", sizeof(ret->desc));
      else
      {
         for (size_t i = 0; i < arr_size; i++)
         {
            if (platform_keys[i].joykey == ret->joykey)
            {
               strlcpy(ret->desc, platform_keys[i].desc, sizeof(ret->desc));
               return;
            }
         }
         strlcpy(ret->desc, "Unknown", sizeof(ret->desc));
      }
   }

   pad_detect_pending[port] = 1;
}

static void *gx_input_init(void)
{
   for (unsigned i = 0; i < MAX_PADS; i++)
   {
      pad_connect[i] = 0;
      pad_type[i] = 0;
      pad_detect_pending[i] = 1;
   }

   PAD_Init();
#ifdef HW_RVL
   WPAD_Init();
#endif
   SYS_SetResetCallback(reset_callback);
#ifdef HW_RVL
   SYS_SetPowerCallback(power_callback);
#endif

   return (void*)-1;
}

static void gx_input_poll(void *data)
{
   (void)data;

   pad_state[0] = 0;
   pad_state[1] = 0;
   pad_state[2] = 0;
   pad_state[3] = 0;
   analog_state[0][0][0] = analog_state[0][0][1] = analog_state[0][1][0] = analog_state[0][1][1] = 0;
   analog_state[1][0][0] = analog_state[1][0][1] = analog_state[1][1][0] = analog_state[1][1][1] = 0;
   analog_state[2][0][0] = analog_state[2][0][1] = analog_state[2][1][0] = analog_state[2][1][1] = 0;
   analog_state[3][0][0] = analog_state[3][0][1] = analog_state[3][1][0] = analog_state[3][1][1] = 0;

   PAD_ScanPads();

#ifdef HW_RVL
   WPAD_ReadPending(WPAD_CHAN_ALL, NULL);
#endif

   for (unsigned port = 0; port < MAX_PADS; port++)
   {
      uint32_t down = 0;
      uint64_t *state_cur = &pad_state[port];

#ifdef HW_RVL
      if (pad_detect_pending[port])
      {
         u32 *ptype = &pad_type[port];
         pad_connect[port] = WPAD_Probe(port, ptype);
         pad_detect_pending[port] = 0;
      }

      uint32_t connected = pad_connect[port];
      uint32_t type = pad_type[port];
      
      if (connected == WPAD_ERR_NONE)
      {
         WPADData *wpaddata = WPAD_Data(port);

         down = wpaddata->btns_h;

         *state_cur |= (down & WPAD_BUTTON_A) ? GX_WIIMOTE_A : 0;
         *state_cur |= (down & WPAD_BUTTON_B) ? GX_WIIMOTE_B : 0;
         *state_cur |= (down & WPAD_BUTTON_1) ? GX_WIIMOTE_1 : 0;
         *state_cur |= (down & WPAD_BUTTON_2) ? GX_WIIMOTE_2 : 0;
         *state_cur |= (down & WPAD_BUTTON_PLUS) ? GX_WIIMOTE_PLUS : 0;
         *state_cur |= (down & WPAD_BUTTON_MINUS) ? GX_WIIMOTE_MINUS : 0;
         *state_cur |= (down & WPAD_BUTTON_HOME) ? GX_WIIMOTE_HOME : 0;
         // rotated d-pad on Wiimote
         *state_cur |= (down & WPAD_BUTTON_UP) ? GX_WIIMOTE_LEFT : 0;
         *state_cur |= (down & WPAD_BUTTON_DOWN) ? GX_WIIMOTE_RIGHT : 0;
         *state_cur |= (down & WPAD_BUTTON_LEFT) ? GX_WIIMOTE_DOWN : 0;
         *state_cur |= (down & WPAD_BUTTON_RIGHT) ? GX_WIIMOTE_UP : 0;

         expansion_t *exp = &wpaddata->exp;

         if (type == WPAD_EXP_CLASSIC)
         {
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_A) ? GX_CLASSIC_A : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_B) ? GX_CLASSIC_B : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_X) ? GX_CLASSIC_X : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_Y) ? GX_CLASSIC_Y : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_UP) ? GX_CLASSIC_UP : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_DOWN) ? GX_CLASSIC_DOWN : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_LEFT) ? GX_CLASSIC_LEFT : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_RIGHT) ? GX_CLASSIC_RIGHT : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_PLUS) ? GX_CLASSIC_PLUS : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_MINUS) ? GX_CLASSIC_MINUS : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_HOME) ? GX_CLASSIC_HOME : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_FULL_L) ? GX_CLASSIC_L_TRIGGER : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_FULL_R) ? GX_CLASSIC_R_TRIGGER : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_ZL) ? GX_CLASSIC_ZL_TRIGGER : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_ZR) ? GX_CLASSIC_ZR_TRIGGER : 0;

            float ljs_mag = exp->classic.ljs.mag;
            float ljs_ang = exp->classic.ljs.ang;

            float rjs_mag = exp->classic.rjs.mag;
            float rjs_ang = exp->classic.rjs.ang;

            if (ljs_mag > 1.0f)
               ljs_mag = 1.0f;
            else if (ljs_mag < -1.0f)
               ljs_mag = -1.0f;

            if (rjs_mag > 1.0f)
               rjs_mag = 1.0f;
            else if (rjs_mag < -1.0f)
               rjs_mag = -1.0f;

            double ljs_val_x = ljs_mag * sin(M_PI * ljs_ang / 180.0);
            double ljs_val_y = -ljs_mag * cos(M_PI * ljs_ang / 180.0);

            double rjs_val_x = rjs_mag * sin(M_PI * rjs_ang / 180.0);
            double rjs_val_y = -rjs_mag * cos(M_PI * rjs_ang / 180.0);

            int16_t ls_x = (int16_t)(ljs_val_x * 32767.0f);
            int16_t ls_y = (int16_t)(ljs_val_y * 32767.0f);

            int16_t rs_x = (int16_t)(rjs_val_x * 32767.0f);
            int16_t rs_y = (int16_t)(rjs_val_y * 32767.0f);

            analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_X] = ls_x;
            analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_Y] = ls_y;
            analog_state[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_X] = rs_x;
            analog_state[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_Y] = rs_y;
         }
         else if (type == WPAD_EXP_NUNCHUK)
         {
            // wiimote is held upright with nunchuk, do not change d-pad orientation
            *state_cur |= (down & WPAD_BUTTON_UP) ? GX_WIIMOTE_UP : 0;
            *state_cur |= (down & WPAD_BUTTON_DOWN) ? GX_WIIMOTE_DOWN : 0;
            *state_cur |= (down & WPAD_BUTTON_LEFT) ? GX_WIIMOTE_LEFT : 0;
            *state_cur |= (down & WPAD_BUTTON_RIGHT) ? GX_WIIMOTE_RIGHT : 0;

            *state_cur |= (down & WPAD_NUNCHUK_BUTTON_Z) ? GX_NUNCHUK_Z : 0;
            *state_cur |= (down & WPAD_NUNCHUK_BUTTON_C) ? GX_NUNCHUK_C : 0;

            float js_mag = exp->nunchuk.js.mag;
            float js_ang = exp->nunchuk.js.ang;

            if (js_mag > 1.0f)
               js_mag = 1.0f;
            else if (js_mag < -1.0f)
               js_mag = -1.0f;

            double js_val_x = js_mag * sin(M_PI * js_ang / 180.0);
            double js_val_y = -js_mag * cos(M_PI * js_ang / 180.0);

            int16_t x = (int16_t)(js_val_x * 32767.0f);
            int16_t y = (int16_t)(js_val_y * 32767.0f);

            analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_X] = x;
            analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_Y] = y;
         }
      }
#endif

      if (SI_GetType(port) & SI_TYPE_GC)
      {
         down = PAD_ButtonsHeld(port);

         *state_cur |= (down & PAD_BUTTON_A) ? GX_GC_A : 0;
         *state_cur |= (down & PAD_BUTTON_B) ? GX_GC_B : 0;
         *state_cur |= (down & PAD_BUTTON_X) ? GX_GC_X : 0;
         *state_cur |= (down & PAD_BUTTON_Y) ? GX_GC_Y : 0;
         *state_cur |= (down & PAD_BUTTON_UP) ? GX_GC_UP : 0;
         *state_cur |= (down & PAD_BUTTON_DOWN) ? GX_GC_DOWN : 0;
         *state_cur |= (down & PAD_BUTTON_LEFT) ? GX_GC_LEFT : 0;
         *state_cur |= (down & PAD_BUTTON_RIGHT) ? GX_GC_RIGHT : 0;
         *state_cur |= (down & PAD_BUTTON_START) ? GX_GC_START : 0;
         *state_cur |= (down & PAD_TRIGGER_Z) ? GX_GC_Z_TRIGGER : 0;
         *state_cur |= ((down & PAD_TRIGGER_L) || PAD_TriggerL(port) > 127) ? GX_GC_L_TRIGGER : 0;
         *state_cur |= ((down & PAD_TRIGGER_R) || PAD_TriggerR(port) > 127) ? GX_GC_R_TRIGGER : 0;

         int16_t ls_x = (int16_t)PAD_StickX(port) * 256;
         int16_t ls_y = (int16_t)PAD_StickY(port) * -256;
         int16_t rs_x = (int16_t)PAD_SubStickX(port) * 256;
         int16_t rs_y = (int16_t)PAD_SubStickY(port) * -256;

         analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_X] = ls_x;
         analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_Y] = ls_y;
         analog_state[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_X] = rs_x;
         analog_state[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_Y] = rs_y;

         if ((*state_cur & (GX_GC_START | GX_GC_Z_TRIGGER | GX_GC_L_TRIGGER | GX_GC_R_TRIGGER)) == (GX_GC_START | GX_GC_Z_TRIGGER | GX_GC_L_TRIGGER | GX_GC_R_TRIGGER))
            *state_cur |= GX_WIIMOTE_HOME;
      }
   }

   uint64_t *state_p1 = &pad_state[0];
   uint64_t *lifecycle_state = &g_extern.lifecycle_state;

   *lifecycle_state &= ~(
         (1ULL << RARCH_FAST_FORWARD_HOLD_KEY) | 
         (1ULL << RARCH_LOAD_STATE_KEY) | 
         (1ULL << RARCH_SAVE_STATE_KEY) | 
         (1ULL << RARCH_STATE_SLOT_PLUS) | 
         (1ULL << RARCH_STATE_SLOT_MINUS) | 
         (1ULL << RARCH_REWIND) |
         (1ULL << RARCH_QUIT_KEY) |
         (1ULL << RARCH_MENU_TOGGLE));

   if (g_menu)
   {
      *state_p1 |= GX_WIIMOTE_HOME;
      g_menu = false;
   }

   if (*state_p1 & (GX_WIIMOTE_HOME
#ifdef HW_RVL
            | GX_CLASSIC_HOME
#endif
            ))
      *lifecycle_state |= (1ULL << RARCH_MENU_TOGGLE);
}

static bool gx_input_key_pressed(void *data, int key)
{
   return (g_extern.lifecycle_state & (1ULL << key));
}


const input_driver_t input_gx = {
   .init = gx_input_init,
   .poll = gx_input_poll,
   .input_state = gx_input_state,
   .key_pressed = gx_input_key_pressed,
   .free = gx_input_free_input,
   .set_keybinds = gx_input_set_keybinds,
   .ident = "gx",
};
