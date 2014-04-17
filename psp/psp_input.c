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

enum input_devices
{
   DEVICE_PSP = 0,
   DEVICE_LAST
};

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

extern const rarch_joypad_driver_t psp_joypad;

typedef struct psp_input
{
   uint64_t pad_state;
   int16_t analog_state[1][2][2];
} psp_input_t;

static void psp_input_set_keybinds(void *data, unsigned device, unsigned port,
      unsigned id, unsigned keybind_action);

static void psp_input_poll(void *data)
{
   int32_t ret;
   uint64_t *lifecycle_state = (uint64_t*)&g_extern.lifecycle_state;
   SceCtrlData state_tmp;
   psp_input_t *psp = (psp_input_t*)data;

#ifdef PSP
   sceCtrlSetSamplingCycle(0);
#endif
   sceCtrlSetSamplingMode(DEFAULT_SAMPLING_MODE);
   ret = CtrlPeekBufferPositive(0, &state_tmp, 1);
   (void)ret;

   psp->analog_state[0][0][0] = psp->analog_state[0][0][1] = psp->analog_state[0][1][0] = psp->analog_state[0][1][1] = 0;
   psp->pad_state = 0;
   psp->pad_state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_LEFT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
   psp->pad_state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_DOWN) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
   psp->pad_state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_RIGHT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
   psp->pad_state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_UP) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
   psp->pad_state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_START) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_START) : 0;
   psp->pad_state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_SELECT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0;
   psp->pad_state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_TRIANGLE) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_X) : 0;
   psp->pad_state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_SQUARE) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_Y) : 0;
   psp->pad_state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_CROSS) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_B) : 0;
   psp->pad_state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_CIRCLE) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_A) : 0;
   psp->pad_state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_R) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R) : 0;
   psp->pad_state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_L) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L) : 0;

   psp->analog_state[0][RETRO_DEVICE_INDEX_ANALOG_LEFT] [RETRO_DEVICE_ID_ANALOG_X] = (int16_t)(STATE_ANALOGLX(state_tmp)-128) * 256;
   psp->analog_state[0][RETRO_DEVICE_INDEX_ANALOG_LEFT] [RETRO_DEVICE_ID_ANALOG_Y] = (int16_t)(STATE_ANALOGLY(state_tmp)-128) * 256;
#ifdef SN_TARGET_PSP2
   psp->analog_state[0][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_X] = (int16_t)(STATE_ANALOGRX(state_tmp)-128) * 256;
   psp->analog_state[0][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_Y] = (int16_t)(STATE_ANALOGRY(state_tmp)-128) * 256;
#endif

   for (int i = 0; i < 2; i++)
      for (int j = 0; j < 2; j++)
         if (psp->analog_state[0][i][j] == -0x8000)
            psp->analog_state[0][i][j] = -0x7fff;

   *lifecycle_state &= ~((1ULL << RARCH_MENU_TOGGLE));

   if (
            (psp->pad_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_L))
         && (psp->pad_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_R))
         && (psp->pad_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT))
         && (psp->pad_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_START))
         )
      *lifecycle_state |= (1ULL << RARCH_MENU_TOGGLE);

   if (g_settings.input.autodetect_enable)
   {
      if (strcmp(g_settings.input.device_names[0], "PSP") != 0)
         psp_input_set_keybinds(NULL, DEVICE_PSP, 0, 0, (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS));
   }
}

static bool psp_menu_input_state(uint64_t joykey, uint64_t state)
{
   switch (joykey)
   {
      case CONSOLE_MENU_A:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_A);
      case CONSOLE_MENU_B:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_B);
      case CONSOLE_MENU_X:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_X);
      case CONSOLE_MENU_Y:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_Y);
      case CONSOLE_MENU_START:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_START);
      case CONSOLE_MENU_SELECT:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT);
      case CONSOLE_MENU_UP:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_UP);
      case CONSOLE_MENU_DOWN:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN);
      case CONSOLE_MENU_LEFT:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT);
      case CONSOLE_MENU_RIGHT:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT);
      case CONSOLE_MENU_L:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_L);
      case CONSOLE_MENU_R:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_R);
      case CONSOLE_MENU_HOME:
         return (state & (1ULL << RETRO_DEVICE_ID_JOYPAD_L3)) && (state & (1ULL << RETRO_DEVICE_ID_JOYPAD_R3));
      case CONSOLE_MENU_L2:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_L2);
      case CONSOLE_MENU_R2:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_R2);
      case CONSOLE_MENU_L3:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_L3);
      case CONSOLE_MENU_R3:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_R3);
      default:
         return false;
   }
}

static int16_t psp_input_state(void *data, const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   psp_input_t *psp = (psp_input_t*)data;

   if (port > 0)
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (binds[port][id].joykey >= CONSOLE_MENU_FIRST && binds[port][id].joykey <= CONSOLE_MENU_LAST)
            return psp_menu_input_state(binds[port][id].joykey, psp->pad_state) ? 1 : 0;
         else
            return input_joypad_pressed(&psp_joypad, port, binds[port], id);
      case RETRO_DEVICE_ANALOG:
         return input_joypad_analog(&psp_joypad, port, index, id, binds[port]);
   }

   return 0;
}

static void psp_input_free_input(void *data)
{
   (void)data;
}

static void psp_input_set_keybinds(void *data, unsigned device, unsigned port,
      unsigned id, unsigned keybind_action)
{
   (void)device;
   uint64_t *key = &g_settings.input.binds[port][id].joykey;
   size_t arr_size = sizeof(platform_keys) / sizeof(platform_keys[0]);
   (void)device;

   if (keybind_action & (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BIND))
      *key = g_settings.input.binds[port][id].def_joykey;

   if (keybind_action & (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS))
   {
      strlcpy(g_settings.input.device_names[port], "PSP", sizeof(g_settings.input.device_names[port]));
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].def_joykey       = (RETRO_DEVICE_ID_JOYPAD_B);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].def_joykey       = (RETRO_DEVICE_ID_JOYPAD_Y);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].def_joykey  = (RETRO_DEVICE_ID_JOYPAD_SELECT);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].def_joykey   = (RETRO_DEVICE_ID_JOYPAD_START);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_UP].def_joykey      = (RETRO_DEVICE_ID_JOYPAD_UP);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_DOWN].def_joykey    = (RETRO_DEVICE_ID_JOYPAD_DOWN);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_LEFT].def_joykey    = (RETRO_DEVICE_ID_JOYPAD_LEFT);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_RIGHT].def_joykey   = (RETRO_DEVICE_ID_JOYPAD_RIGHT);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].def_joykey       = (RETRO_DEVICE_ID_JOYPAD_A);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].def_joykey       = (RETRO_DEVICE_ID_JOYPAD_X);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].def_joykey       = (RETRO_DEVICE_ID_JOYPAD_L);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].def_joykey       = (RETRO_DEVICE_ID_JOYPAD_R);
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

      for (int i = 0; i < RARCH_CUSTOM_BIND_LIST_END; i++)
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

static void* psp_input_initialize(void)
{
   psp_input_t *psp = (psp_input_t*)calloc(1, sizeof(*psp));
   if (!psp)
      return NULL;

   return psp;
}

static bool psp_input_key_pressed(void *data, int key)
{
   return (g_extern.lifecycle_state & (1ULL << key)) || input_joypad_pressed(&psp_joypad, 0, g_settings.input.binds[0], key);
}

static uint64_t psp_input_get_capabilities(void *data)
{
   uint64_t caps = 0;

   caps |= (1 << RETRO_DEVICE_JOYPAD);
   caps |= (1 << RETRO_DEVICE_ANALOG);

   return caps;
}

static const rarch_joypad_driver_t *psp_input_get_joypad_driver(void *data)
{
   return &psp_joypad;
}

static unsigned psp_input_devices_size(void *data)
{
   return DEVICE_LAST;
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
   psp_input_devices_size,
   "psp",

   NULL,
   NULL,
   psp_input_get_joypad_driver,
};

static bool psp_joypad_init(void)
{
   return true;
}

static bool psp_joypad_button(unsigned port_num, uint16_t joykey)
{
   psp_input_t *psp = (psp_input_t*)driver.input_data;

   if (port_num >= MAX_PADS)
      return false;

   return (psp->pad_state & (1ULL << joykey));
}

static int16_t psp_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   psp_input_t *psp = (psp_input_t*)driver.input_data;
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
      case 0: val = psp->analog_state[port_num][0][0]; break;
      case 1: val = psp->analog_state[port_num][0][1]; break;
      case 2: val = psp->analog_state[port_num][1][0]; break;
      case 3: val = psp->analog_state[port_num][1][1]; break;
   }

   if (is_neg && val > 0)
      val = 0;
   else if (is_pos && val < 0)
      val = 0;

   return val;
}

static void psp_joypad_poll(void)
{
}

static bool psp_joypad_query_pad(unsigned pad)
{
   psp_input_t *psp = (psp_input_t*)driver.input_data;
   return pad < MAX_PLAYERS && psp->pad_state;
}

static const char *psp_joypad_name(unsigned pad)
{
   return NULL;
}

static void psp_joypad_destroy(void)
{
}

const rarch_joypad_driver_t psp_joypad = {
   psp_joypad_init,
   psp_joypad_query_pad,
   psp_joypad_destroy,
   psp_joypad_button,
   psp_joypad_axis,
   psp_joypad_poll,
   NULL,
   psp_joypad_name,
   "psp",
};
