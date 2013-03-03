/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
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

#import "RAInputResponder.h"

#include <unistd.h>
#include "input/input_common.h"
#include "general.h"
#include "driver.h"

#ifdef WIIMOTE
extern const rarch_joypad_driver_t ios_joypad;
static const rarch_joypad_driver_t* const g_joydriver = &ios_joypad;
#else
static const rarch_joypad_driver_t* const g_joydriver = 0;
#endif

static const struct rarch_key_map rarch_key_map_hidusage[];

static RAInputResponder* g_input_driver;

// Non-exported helpers
static bool ios_key_pressed(enum retro_key key)
{
   if ((int)key >= 0 && key < RETROK_LAST)
   {
      return [g_input_driver isKeyPressed:input_translate_rk_to_keysym(key)];
   }
   
   return false;
}

static bool ios_is_pressed(unsigned port_num, const struct retro_keybind *key)
{
   return ios_key_pressed(key->key) || input_joypad_pressed(g_joydriver, port_num, key);
}

// Exported input driver
static void *ios_input_init(void)
{
   input_init_keyboard_lut(rarch_key_map_hidusage);
   g_input_driver = [RAInputResponder new];
   return (void*)-1;
}

static void ios_input_poll(void *data)
{
   [g_input_driver poll];
   input_joypad_poll(g_joydriver);
}

static int16_t ios_input_state(void *data, const struct retro_keybind **binds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return (id < RARCH_BIND_LIST_END) ? ios_is_pressed(port, &binds[port][id]) : false;

      case RARCH_DEVICE_POINTER_SCREEN:
      {
         const touch_data_t* touch = [g_input_driver getTouchDataAtIndex:index];

         switch (id)
         {
            case RETRO_DEVICE_ID_POINTER_X: return touch ? touch->full_x : 0;
            case RETRO_DEVICE_ID_POINTER_Y: return touch ? touch->full_y : 0;
            case RETRO_DEVICE_ID_POINTER_PRESSED: return touch ? 1 : 0;
         }
         
         return 0;
      }

      default:
         return 0;
   }
}

static bool ios_bind_button_pressed(void *data, int key)
{
   const struct retro_keybind *binds = g_settings.input.binds[0];
   return (key >= 0 && key < RARCH_BIND_LIST_END) ? ios_is_pressed(0, &binds[key]) : false;
}

static void ios_input_free_input(void *data)
{
   (void)data;
   g_input_driver = nil;
}

const input_driver_t input_ios = {
   ios_input_init,
   ios_input_poll,
   ios_input_state,
   ios_bind_button_pressed,
   ios_input_free_input,
   "ios_input",
};

// Key table
static const struct rarch_key_map rarch_key_map_hidusage[] = {
   { 0x50, RETROK_LEFT },
   { 0x4F, RETROK_RIGHT },
   { 0x52, RETROK_UP },
   { 0x51, RETROK_DOWN },
   { 0x28, RETROK_RETURN },
   { 0x2B, RETROK_TAB },
   { 0x49, RETROK_INSERT },
   { 0x4C, RETROK_DELETE },
   { 0xE5, RETROK_RSHIFT },
   { 0xE1, RETROK_LSHIFT },
   { 0xE0, RETROK_LCTRL },
   { 0x4D, RETROK_END },
   { 0x4A, RETROK_HOME },
   { 0x4E, RETROK_PAGEDOWN },
   { 0x4B, RETROK_PAGEUP },
   { 0xE2, RETROK_LALT },
   { 0x2C, RETROK_SPACE },
   { 0x29, RETROK_ESCAPE },
   { 0x2A, RETROK_BACKSPACE },
   { 0x58, RETROK_KP_ENTER },
   { 0x57, RETROK_KP_PLUS },
   { 0x56, RETROK_KP_MINUS },
   { 0x55, RETROK_KP_MULTIPLY },
   { 0x54, RETROK_KP_DIVIDE },
   { 0x35, RETROK_BACKQUOTE },
   { 0x48, RETROK_PAUSE },
   { 0x62, RETROK_KP0 },
   { 0x59, RETROK_KP1 },
   { 0x5A, RETROK_KP2 },
   { 0x5B, RETROK_KP3 },
   { 0x5C, RETROK_KP4 },
   { 0x5D, RETROK_KP5 },
   { 0x5E, RETROK_KP6 },
   { 0x5F, RETROK_KP7 },
   { 0x60, RETROK_KP8 },
   { 0x61, RETROK_KP9 },
   { 0x27, RETROK_0 },
   { 0x1E, RETROK_1 },
   { 0x1F, RETROK_2 },
   { 0x20, RETROK_3 },
   { 0x21, RETROK_4 },
   { 0x22, RETROK_5 },
   { 0x23, RETROK_6 },
   { 0x24, RETROK_7 },
   { 0x25, RETROK_8 },
   { 0x26, RETROK_9 },
   { 0x3A, RETROK_F1 },
   { 0x3B, RETROK_F2 },
   { 0x3C, RETROK_F3 },
   { 0x3D, RETROK_F4 },
   { 0x3E, RETROK_F5 },
   { 0x3F, RETROK_F6 },
   { 0x40, RETROK_F7 },
   { 0x41, RETROK_F8 },
   { 0x42, RETROK_F9 },
   { 0x43, RETROK_F10 },
   { 0x44, RETROK_F11 },
   { 0x45, RETROK_F12 },
   { 0x04, RETROK_a },
   { 0x05, RETROK_b },
   { 0x06, RETROK_c },
   { 0x07, RETROK_d },
   { 0x08, RETROK_e },
   { 0x09, RETROK_f },
   { 0x0A, RETROK_g },
   { 0x0B, RETROK_h },
   { 0x0C, RETROK_i },
   { 0x0D, RETROK_j },
   { 0x0E, RETROK_k },
   { 0x0F, RETROK_l },
   { 0x10, RETROK_m },
   { 0x11, RETROK_n },
   { 0x12, RETROK_o },
   { 0x13, RETROK_p },
   { 0x14, RETROK_q },
   { 0x15, RETROK_r },
   { 0x16, RETROK_s },
   { 0x17, RETROK_t },
   { 0x18, RETROK_u },
   { 0x19, RETROK_v },
   { 0x1A, RETROK_w },
   { 0x1B, RETROK_x },
   { 0x1C, RETROK_y },
   { 0x1D, RETROK_z },
   { 0, RETROK_UNKNOWN }
};
