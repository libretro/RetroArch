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

typedef struct gx_input
{
   uint64_t pad_state[MAX_PADS];
   int16_t analog_state[MAX_PADS][2][2];
} gx_input_t;

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

extern const rarch_joypad_driver_t gx_joypad;

static bool g_menu;

#ifdef HW_RVL
static bool g_quit;
#endif

static bool gx_menu_input_state(uint64_t joykey, uint64_t state)
{
   switch (joykey)
   {
      case GX_MENU_A:
         return state & ((1ULL << GX_GC_A) | (1ULL << GX_WIIMOTE_A) | (1ULL << GX_CLASSIC_A) | (1ULL << GX_WIIMOTE_2));
      case GX_MENU_B:
         return state & ((1ULL << GX_GC_B) | (1ULL << GX_WIIMOTE_B) | (1ULL << GX_CLASSIC_B) | (1ULL << GX_WIIMOTE_1));
      case GX_MENU_X:
         return state & ((1ULL << GX_GC_X) | (1ULL << GX_CLASSIC_X));
      case GX_MENU_Y:
         return state & ((1ULL << GX_GC_Y) | (1ULL << GX_CLASSIC_Y));
      case GX_MENU_START:
         return state & ((1ULL << GX_GC_START) | (1ULL << GX_WIIMOTE_PLUS) | (1ULL << GX_CLASSIC_PLUS));
      case GX_MENU_SELECT:
         return state & ((1ULL << GX_GC_Z_TRIGGER) | (1ULL << GX_WIIMOTE_MINUS) | (1ULL << GX_CLASSIC_MINUS));
      case GX_MENU_UP:
         return state & ((1ULL << GX_GC_UP) | (1ULL << GX_WIIMOTE_UP) | (1ULL << GX_CLASSIC_UP) | (1ULL << GX_NUNCHUK_UP));
      case GX_MENU_DOWN:
         return state & ((1ULL << GX_GC_DOWN) | (1ULL << GX_WIIMOTE_DOWN) | (1ULL << GX_CLASSIC_DOWN) | (1ULL << GX_NUNCHUK_DOWN));
      case GX_MENU_LEFT:
         return state & ((1ULL << GX_GC_LEFT) | (1ULL << GX_WIIMOTE_LEFT) | (1ULL << GX_CLASSIC_LEFT) | (1ULL << GX_NUNCHUK_LEFT));
      case GX_MENU_RIGHT:
         return state & ((1ULL << GX_GC_RIGHT) | (1ULL << GX_WIIMOTE_RIGHT) | (1ULL << GX_CLASSIC_RIGHT) | (1ULL << GX_NUNCHUK_RIGHT));
      case GX_MENU_L:
         return state & ((1ULL << GX_GC_L_TRIGGER) | (1ULL << GX_CLASSIC_L_TRIGGER));
      case GX_MENU_R:
         return state & ((1ULL << GX_GC_R_TRIGGER) | (1ULL << GX_CLASSIC_R_TRIGGER));
      case GX_MENU_HOME:
         return state & ((1ULL << GX_WIIMOTE_HOME) | (1ULL << GX_CLASSIC_HOME));
      default:
         return false;
   }
}

static int16_t gx_input_state(void *data, const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   gx_input_t *gx = (gx_input_t*)data;

   if (port >= MAX_PADS)
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (binds[port][id].joykey >= GX_MENU_FIRST && binds[port][id].joykey <= GX_MENU_LAST)
            return gx_menu_input_state(binds[port][id].joykey, gx->pad_state[port]) ? 1 : 0;
         else
            return input_joypad_pressed(&gx_joypad, port, binds[port], id);;
      case RETRO_DEVICE_ANALOG:
         return input_joypad_analog(&gx_joypad, port, index, id, binds[port]);
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
   (void)data;

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
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].def_joykey       = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_1].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].def_joykey       = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_A].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].def_joykey  = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_MINUS].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].def_joykey   = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_PLUS].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_UP].def_joykey      = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_UP].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_DOWN].def_joykey    = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_DOWN].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_LEFT].def_joykey    = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_LEFT].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_RIGHT].def_joykey   = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_RIGHT].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].def_joykey       = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_2].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].def_joykey       = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_B].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].def_joykey       = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].def_joykey       = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L2].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R2].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L3].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R3].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_PLUS].def_joykey       = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_MINUS].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_PLUS].def_joykey       = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_MINUS].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_PLUS].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_MINUS].def_joykey     = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_PLUS].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_MINUS].def_joykey     = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].def_joyaxis = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].def_joyaxis  = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_UP].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_DOWN].def_joyaxis   = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_LEFT].def_joyaxis   = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_RIGHT].def_joyaxis  = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L2].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R2].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L3].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R3].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_PLUS].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_MINUS].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_PLUS].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_MINUS].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_PLUS].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_MINUS].def_joyaxis    = AXIS_NONE;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_PLUS].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_MINUS].def_joyaxis    = AXIS_NONE;
            break;
         case DEVICE_NUNCHUK:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Wiimote + Nunchuk", sizeof(g_settings.input.device_names[port]));
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].def_joykey       = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_B].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].def_joykey       = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_2].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].def_joykey  = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_MINUS].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].def_joykey   = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_PLUS].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_UP].def_joykey      = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_UP].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_DOWN].def_joykey    = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_DOWN].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_LEFT].def_joykey    = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_LEFT].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_RIGHT].def_joykey   = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_RIGHT].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].def_joykey       = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_A].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].def_joykey       = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_1].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].def_joykey       = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_Z].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].def_joykey       = platform_keys[GX_DEVICE_WIIMOTE_ID_JOYPAD_C].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L2].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R2].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L3].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R3].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_PLUS].def_joykey       = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_MINUS].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_PLUS].def_joykey       = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_MINUS].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_PLUS].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_MINUS].def_joykey     = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_PLUS].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_MINUS].def_joykey     = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].def_joyaxis = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].def_joyaxis  = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_UP].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_DOWN].def_joyaxis   = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_LEFT].def_joyaxis   = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_RIGHT].def_joyaxis  = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L2].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R2].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L3].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R3].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_PLUS].def_joyaxis      = AXIS_POS(0);
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_MINUS].def_joyaxis     = AXIS_NEG(0);
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_PLUS].def_joyaxis      = AXIS_POS(1);
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_MINUS].def_joyaxis     = AXIS_NEG(1);
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_PLUS].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_MINUS].def_joyaxis    = AXIS_NONE;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_PLUS].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_MINUS].def_joyaxis    = AXIS_NONE;
            break;
         case DEVICE_CLASSIC:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Classic Controller", sizeof(g_settings.input.device_names[port]));
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].def_joykey       = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_B].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].def_joykey       = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_Y].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].def_joykey  = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_MINUS].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].def_joykey   = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_PLUS].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_UP].def_joykey      = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_UP].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_DOWN].def_joykey    = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_DOWN].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_LEFT].def_joykey    = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_LEFT].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_RIGHT].def_joykey   = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_RIGHT].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].def_joykey       = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_A].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].def_joykey       = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_X].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].def_joykey       = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_L_TRIGGER].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].def_joykey       = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_R_TRIGGER].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L2].def_joykey      = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_ZL_TRIGGER].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R2].def_joykey      = platform_keys[GX_DEVICE_CLASSIC_ID_JOYPAD_ZR_TRIGGER].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L3].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R3].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_PLUS].def_joykey       = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_MINUS].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_PLUS].def_joykey       = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_MINUS].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_PLUS].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_MINUS].def_joykey     = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_PLUS].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_MINUS].def_joykey     = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].def_joyaxis = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].def_joyaxis  = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_UP].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_DOWN].def_joyaxis   = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_LEFT].def_joyaxis   = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_RIGHT].def_joyaxis  = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L2].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R2].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L3].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R3].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_PLUS].def_joyaxis      = AXIS_POS(0);
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_MINUS].def_joyaxis     = AXIS_NEG(0);
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_PLUS].def_joyaxis      = AXIS_POS(1);
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_MINUS].def_joyaxis     = AXIS_NEG(1);
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_PLUS].def_joyaxis     = AXIS_POS(2);
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_MINUS].def_joyaxis    = AXIS_NEG(2);
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_PLUS].def_joyaxis     = AXIS_POS(3);
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_MINUS].def_joyaxis    = AXIS_NEG(3);
            break;
#endif
         case DEVICE_GAMECUBE:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Gamecube Controller", sizeof(g_settings.input.device_names[port]));
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].def_joykey       = platform_keys[GX_DEVICE_GC_ID_JOYPAD_B].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].def_joykey       = platform_keys[GX_DEVICE_GC_ID_JOYPAD_Y].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].def_joykey  = platform_keys[GX_DEVICE_GC_ID_JOYPAD_Z_TRIGGER].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].def_joykey   = platform_keys[GX_DEVICE_GC_ID_JOYPAD_START].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_UP].def_joykey      = platform_keys[GX_DEVICE_GC_ID_JOYPAD_UP].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_DOWN].def_joykey    = platform_keys[GX_DEVICE_GC_ID_JOYPAD_DOWN].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_LEFT].def_joykey    = platform_keys[GX_DEVICE_GC_ID_JOYPAD_LEFT].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_RIGHT].def_joykey   = platform_keys[GX_DEVICE_GC_ID_JOYPAD_RIGHT].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].def_joykey       = platform_keys[GX_DEVICE_GC_ID_JOYPAD_A].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].def_joykey       = platform_keys[GX_DEVICE_GC_ID_JOYPAD_X].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].def_joykey       = platform_keys[GX_DEVICE_GC_ID_JOYPAD_L_TRIGGER].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].def_joykey       = platform_keys[GX_DEVICE_GC_ID_JOYPAD_R_TRIGGER].joykey;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L2].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R2].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L3].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R3].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_PLUS].def_joykey       = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_MINUS].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_PLUS].def_joykey       = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_MINUS].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_PLUS].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_MINUS].def_joykey     = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_PLUS].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_MINUS].def_joykey     = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].def_joyaxis = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].def_joyaxis  = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_UP].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_DOWN].def_joyaxis   = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_LEFT].def_joyaxis   = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_RIGHT].def_joyaxis  = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L2].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R2].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L3].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R3].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_PLUS].def_joyaxis      = AXIS_POS(0);
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_MINUS].def_joyaxis     = AXIS_NEG(0);
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_PLUS].def_joyaxis      = AXIS_POS(1);
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_MINUS].def_joyaxis     = AXIS_NEG(1);
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_PLUS].def_joyaxis     = AXIS_POS(2);
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_MINUS].def_joyaxis    = AXIS_NEG(2);
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_PLUS].def_joyaxis     = AXIS_POS(3);
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_MINUS].def_joyaxis    = AXIS_NEG(3);
            break;
         default:
            break;
      }

      for (unsigned i = 0; i < RARCH_CUSTOM_BIND_LIST_END; i++)
      {
         g_settings.input.binds[port][i].id = i;
         g_settings.input.binds[port][i].joykey = g_settings.input.binds[port][i].def_joykey;
         g_settings.input.binds[port][i].joyaxis = g_settings.input.binds[port][i].def_joyaxis;
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
}

static void *gx_input_init(void)
{
   gx_input_t *gx = (gx_input_t*)calloc(1, sizeof(*gx));
   if (!gx)
      return NULL;

   PAD_Init();
#ifdef HW_RVL
   WPAD_Init();
#endif
   SYS_SetResetCallback(reset_callback);
#ifdef HW_RVL
   SYS_SetPowerCallback(power_callback);
#endif

   return gx;
}

static void gx_input_poll(void *data)
{
   gx_input_t *gx = (gx_input_t*)data;

   gx->pad_state[0] = 0;
   gx->pad_state[1] = 0;
   gx->pad_state[2] = 0;
   gx->pad_state[3] = 0;
   gx->analog_state[0][0][0] = gx->analog_state[0][0][1] = gx->analog_state[0][1][0] = gx->analog_state[0][1][1] = 0;
   gx->analog_state[1][0][0] = gx->analog_state[1][0][1] = gx->analog_state[1][1][0] = gx->analog_state[1][1][1] = 0;
   gx->analog_state[2][0][0] = gx->analog_state[2][0][1] = gx->analog_state[2][1][0] = gx->analog_state[2][1][1] = 0;
   gx->analog_state[3][0][0] = gx->analog_state[3][0][1] = gx->analog_state[3][1][0] = gx->analog_state[3][1][1] = 0;

   PAD_ScanPads();

#ifdef HW_RVL
   WPAD_ReadPending(WPAD_CHAN_ALL, NULL);
#endif

   for (unsigned port = 0; port < MAX_PADS; port++)
   {
      uint32_t down = 0;
      uint64_t *state_cur = &gx->pad_state[port];

      if (SI_GetType(port) & SI_TYPE_GC)
      {
         down = PAD_ButtonsHeld(port);

         *state_cur |= (down & PAD_BUTTON_A) ? (1ULL << GX_GC_A) : 0;
         *state_cur |= (down & PAD_BUTTON_B) ? (1ULL << GX_GC_B) : 0;
         *state_cur |= (down & PAD_BUTTON_X) ? (1ULL << GX_GC_X) : 0;
         *state_cur |= (down & PAD_BUTTON_Y) ? (1ULL << GX_GC_Y) : 0;
         *state_cur |= (down & PAD_BUTTON_UP) ? (1ULL << GX_GC_UP) : 0;
         *state_cur |= (down & PAD_BUTTON_DOWN) ? (1ULL << GX_GC_DOWN) : 0;
         *state_cur |= (down & PAD_BUTTON_LEFT) ? (1ULL << GX_GC_LEFT) : 0;
         *state_cur |= (down & PAD_BUTTON_RIGHT) ? (1ULL << GX_GC_RIGHT) : 0;
         *state_cur |= (down & PAD_BUTTON_START) ? (1ULL << GX_GC_START) : 0;
         *state_cur |= (down & PAD_TRIGGER_Z) ? (1ULL << GX_GC_Z_TRIGGER) : 0;
         *state_cur |= ((down & PAD_TRIGGER_L) || PAD_TriggerL(port) > 127) ? (1ULL << GX_GC_L_TRIGGER) : 0;
         *state_cur |= ((down & PAD_TRIGGER_R) || PAD_TriggerR(port) > 127) ? (1ULL << GX_GC_R_TRIGGER) : 0;

         int16_t ls_x = (int16_t)PAD_StickX(port) * 256;
         int16_t ls_y = (int16_t)PAD_StickY(port) * -256;
         int16_t rs_x = (int16_t)PAD_SubStickX(port) * 256;
         int16_t rs_y = (int16_t)PAD_SubStickY(port) * -256;

         gx->analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_X] = ls_x;
         gx->analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_Y] = ls_y;
         gx->analog_state[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_X] = rs_x;
         gx->analog_state[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_Y] = rs_y;

         const uint64_t menu_combo = (1ULL << GX_GC_START) | (1ULL << GX_GC_Z_TRIGGER) | (1ULL << GX_GC_L_TRIGGER) | (1ULL << GX_GC_R_TRIGGER);
         if ((*state_cur & menu_combo) == menu_combo)
            *state_cur |= (1ULL << GX_WIIMOTE_HOME);

         if (g_settings.input.autodetect_enable)
         {
            if (strcmp(g_settings.input.device_names[port], "Gamecube Controller") != 0)
               gx_input_set_keybinds(NULL, DEVICE_GAMECUBE, port, 0, (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS));
         }
      }

#ifdef HW_RVL
      uint32_t ptype = 0;
      uint32_t connected = WPAD_Probe(port, &ptype);
      
      if (connected == WPAD_ERR_NONE)
      {
         WPADData *wpaddata = WPAD_Data(port);

         down = wpaddata->btns_h;

         *state_cur |= (down & WPAD_BUTTON_A) ? (1ULL << GX_WIIMOTE_A) : 0;
         *state_cur |= (down & WPAD_BUTTON_B) ? (1ULL << GX_WIIMOTE_B) : 0;
         *state_cur |= (down & WPAD_BUTTON_1) ? (1ULL << GX_WIIMOTE_1) : 0;
         *state_cur |= (down & WPAD_BUTTON_2) ? (1ULL << GX_WIIMOTE_2) : 0;
         *state_cur |= (down & WPAD_BUTTON_PLUS) ? (1ULL << GX_WIIMOTE_PLUS) : 0;
         *state_cur |= (down & WPAD_BUTTON_MINUS) ? (1ULL << GX_WIIMOTE_MINUS) : 0;
         *state_cur |= (down & WPAD_BUTTON_HOME) ? (1ULL << GX_WIIMOTE_HOME) : 0;
         // rotated d-pad on Wiimote
         *state_cur |= (down & WPAD_BUTTON_UP) ? (1ULL << GX_WIIMOTE_LEFT) : 0;
         *state_cur |= (down & WPAD_BUTTON_DOWN) ? (1ULL << GX_WIIMOTE_RIGHT) : 0;
         *state_cur |= (down & WPAD_BUTTON_LEFT) ? (1ULL << GX_WIIMOTE_DOWN) : 0;
         *state_cur |= (down & WPAD_BUTTON_RIGHT) ? (1ULL << GX_WIIMOTE_UP) : 0;

         expansion_t *exp = &wpaddata->exp;

         if (ptype == WPAD_EXP_CLASSIC)
         {
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_A) ? (1ULL << GX_CLASSIC_A) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_B) ? (1ULL << GX_CLASSIC_B) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_X) ? (1ULL << GX_CLASSIC_X) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_Y) ? (1ULL << GX_CLASSIC_Y) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_UP) ? (1ULL << GX_CLASSIC_UP) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_DOWN) ? (1ULL << GX_CLASSIC_DOWN) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_LEFT) ? (1ULL << GX_CLASSIC_LEFT) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_RIGHT) ? (1ULL << GX_CLASSIC_RIGHT) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_PLUS) ? (1ULL << GX_CLASSIC_PLUS) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_MINUS) ? (1ULL << GX_CLASSIC_MINUS) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_HOME) ? (1ULL << GX_CLASSIC_HOME) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_FULL_L) ? (1ULL << GX_CLASSIC_L_TRIGGER) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_FULL_R) ? (1ULL << GX_CLASSIC_R_TRIGGER) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_ZL) ? (1ULL << GX_CLASSIC_ZL_TRIGGER) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_ZR) ? (1ULL << GX_CLASSIC_ZR_TRIGGER) : 0;

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

            gx->analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_X] = ls_x;
            gx->analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_Y] = ls_y;
            gx->analog_state[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_X] = rs_x;
            gx->analog_state[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_Y] = rs_y;

            if (g_settings.input.autodetect_enable)
            {
               if (strcmp(g_settings.input.device_names[port], "Classic Controller") != 0)
                  gx_input_set_keybinds(NULL, DEVICE_CLASSIC, port, 0, (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS));
            }
         }
         else if (ptype == WPAD_EXP_NUNCHUK)
         {
            // wiimote is held upright with nunchuk, do not change d-pad orientation
            *state_cur |= (down & WPAD_BUTTON_UP) ? (1ULL << GX_WIIMOTE_UP) : 0;
            *state_cur |= (down & WPAD_BUTTON_DOWN) ? (1ULL << GX_WIIMOTE_DOWN) : 0;
            *state_cur |= (down & WPAD_BUTTON_LEFT) ? (1ULL << GX_WIIMOTE_LEFT) : 0;
            *state_cur |= (down & WPAD_BUTTON_RIGHT) ? (1ULL << GX_WIIMOTE_RIGHT) : 0;

            *state_cur |= (down & WPAD_NUNCHUK_BUTTON_Z) ? (1ULL << GX_NUNCHUK_Z) : 0;
            *state_cur |= (down & WPAD_NUNCHUK_BUTTON_C) ? (1ULL << GX_NUNCHUK_C) : 0;

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

            gx->analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_X] = x;
            gx->analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_Y] = y;

            if (g_settings.input.autodetect_enable)
            {
               if (strcmp(g_settings.input.device_names[port], "Wiimote + Nunchuk") != 0)
                  gx_input_set_keybinds(NULL, DEVICE_NUNCHUK, port, 0, (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS));
            }
         }
         else
         {
            //no attachment, assume standalone Wiimote
            if (g_settings.input.autodetect_enable)
            {
               if (strcmp(g_settings.input.device_names[port], "Wiimote") != 0)
                  gx_input_set_keybinds(NULL, DEVICE_WIIMOTE, port, 0, (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS));
            }
         }
      }

      for (int i = 0; i < 2; i++)
         for (int j = 0; j < 2; j++)
            if (gx->analog_state[port][i][j] == -0x8000)
               gx->analog_state[port][i][j] = -0x7fff;
#endif
   }

   uint64_t *state_p1 = &gx->pad_state[0];
   uint64_t *lifecycle_state = &g_extern.lifecycle_state;

   *lifecycle_state &= ~((1ULL << RARCH_MENU_TOGGLE));

   if (g_menu)
   {
      *state_p1 |= (1ULL << GX_WIIMOTE_HOME);
      g_menu = false;
   }

   if (*state_p1 & ((1ULL << GX_WIIMOTE_HOME)
#ifdef HW_RVL
            | (1ULL << GX_CLASSIC_HOME)
#endif
            ))
      *lifecycle_state |= (1ULL << RARCH_MENU_TOGGLE);
}

static bool gx_input_key_pressed(void *data, int key)
{
   return (g_extern.lifecycle_state & (1ULL << key));
}

static uint64_t gx_input_get_capabilities(void *data)
{
   uint64_t caps = 0;

   caps |= (1 << RETRO_DEVICE_JOYPAD);
   caps |= (1 << RETRO_DEVICE_ANALOG);

   return caps;
}

static const rarch_joypad_driver_t *gx_input_get_joypad_driver(void *data)
{
   return &gx_joypad;
}

const input_driver_t input_gx = {
   gx_input_init,
   gx_input_poll,
   gx_input_state,
   gx_input_key_pressed,
   gx_input_free_input,
   gx_input_set_keybinds,
   NULL,
   gx_input_get_capabilities,
   "gx",

   NULL,
   NULL,
   gx_input_get_joypad_driver,
};


static bool gx_joypad_init(void)
{
   return true;
}

static bool gx_joypad_button(unsigned port_num, uint16_t joykey)
{
   gx_input_t *gx = (gx_input_t*)driver.input_data;

   if (port_num >= MAX_PADS)
      return false;

   return gx->pad_state[port_num] & (1ULL << joykey);
}

static int16_t gx_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   gx_input_t *gx = (gx_input_t*)driver.input_data;
   if (joyaxis == AXIS_NONE || port_num >= MAX_PADS)
      return 0;

   int val = 0;

   int axis    = -1;
   bool is_neg = false;
   bool is_pos = false;

   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      axis = AXIS_NEG_GET(joyaxis);
      is_neg = true;
   }
   else if (AXIS_POS_GET(joyaxis) < 4)
   {
      axis = AXIS_POS_GET(joyaxis);
      is_pos = true;
   }

   switch (axis)
   {
      case 0: val = gx->analog_state[port_num][0][0]; break;
      case 1: val = gx->analog_state[port_num][0][1]; break;
      case 2: val = gx->analog_state[port_num][1][0]; break;
      case 3: val = gx->analog_state[port_num][1][1]; break;
   }

   if (is_neg && val > 0)
      val = 0;
   else if (is_pos && val < 0)
      val = 0;

   return val;
}

static void gx_joypad_poll(void)
{
}

static bool gx_joypad_query_pad(unsigned pad)
{
   gx_input_t *gx = (gx_input_t*)driver.input_data;
   return pad < MAX_PLAYERS && gx->pad_state[pad];
}

static const char *gx_joypad_name(unsigned pad)
{
   return NULL;
}

static void gx_joypad_destroy(void)
{
}

const rarch_joypad_driver_t gx_joypad = {
   gx_joypad_init,
   gx_joypad_query_pad,
   gx_joypad_destroy,
   gx_joypad_button,
   gx_joypad_axis,
   gx_joypad_poll,
   NULL,
   gx_joypad_name,
   "gx",
};
