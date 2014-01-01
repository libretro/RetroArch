/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
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


#include <unistd.h>

#include "apple_gamecontroller.h"
#include "input/input_common.h"
#include "apple_input.h"
#include "general.h"
#include "driver.h"

#include "keycode.inc"

extern const rarch_joypad_driver_t apple_joypad;
static const rarch_joypad_driver_t* const g_joydriver = &apple_joypad;

apple_input_data_t g_current_input_data;
apple_input_data_t g_polled_input_data;

#ifdef OSX // Taken from https://github.com/depp/keycode, check keycode.h for license
const unsigned char MAC_NATIVE_TO_HID[128] = {
  4, 22,  7,  9, 11, 10, 29, 27,  6, 25,255,  5, 20, 26,  8, 21,
 28, 23, 30, 31, 32, 33, 35, 34, 46, 38, 36, 45, 37, 39, 48, 18,
 24, 47, 12, 19, 40, 15, 13, 52, 14, 51, 49, 54, 56, 17, 16, 55,
 43, 44, 53, 42,255, 41,231,227,225, 57,226,224,229,230,228,255,
108, 99,255, 85,255, 87,255, 83,255,255,255, 84, 88,255, 86,109,
110,103, 98, 89, 90, 91, 92, 93, 94, 95,111, 96, 97,255,255,255,
 62, 63, 64, 60, 65, 66,255, 68,255,104,107,105,255, 67,255, 69,
255,106,117, 74, 75, 76, 61, 77, 59, 78, 58, 80, 79, 81, 82,255
};

#define HIDKEY(X) (X < 128) ? MAC_NATIVE_TO_HID[X] : 0
#else
#define HIDKEY(X) X
#endif

// Main thread interface
static bool icade_enabled;
static uint32_t icade_buttons;

static void handle_icade_event(unsigned keycode)
{
   static const struct
   {
      bool up;
      int button;
   }  icade_map[0x20] =
   {
      { false, -1 }, { false, -1 }, { false, -1 }, { false, -1 }, // 0
      { false,  2 }, { false, -1 }, { true ,  3 }, { false,  3 }, // 4
      { true ,  0 }, { true,   5 }, { true ,  7 }, { false,  8 }, // 8
      { false,  6 }, { false,  9 }, { false, 10 }, { false, 11 }, // C
      { true ,  6 }, { true ,  9 }, { false,  7 }, { true,  10 }, // 0
      { true ,  2 }, { true ,  8 }, { false, -1 }, { true ,  4 }, // 4
      { false,  5 }, { true , 11 }, { false,  0 }, { false,  1 }, // 8
      { false,  4 }, { true ,  1 }, { false, -1 }, { false, -1 }  // C
   };
      
   if (icade_enabled && (keycode < 0x20) && (icade_map[keycode].button >= 0))
   {
      const int button = icade_map[keycode].button;
      
      if (icade_map[keycode].up)
         icade_buttons &= ~(1 << button);
      else
         icade_buttons |=  (1 << button);
   }
}

void apple_input_enable_icade(bool on)
{
   icade_enabled = on;
   icade_buttons = 0;
}

uint32_t apple_input_get_icade_buttons(void)
{
   return icade_enabled ? icade_buttons : 0;
}

void apple_input_reset_icade_buttons(void)
{
   icade_buttons = 0;
}

void apple_input_handle_key_event(unsigned keycode, bool down)
{
   keycode = HIDKEY(keycode);

   if (icade_enabled)
      handle_icade_event(keycode);
   else if (keycode < MAX_KEYS)
      g_current_input_data.keys[keycode] = down;
}


int32_t apple_input_find_any_key(void)
{
   apple_gamecontroller_poll_all();
   input_init_keyboard_lut(apple_key_map_hidusage);

   for (int i = 0; apple_key_name_map[i].hid_id; i++)
      if (g_current_input_data.keys[apple_key_name_map[i].hid_id])
         return apple_key_name_map[i].hid_id;

   return 0;
}

int32_t apple_input_find_any_button(uint32_t port)
{
   apple_gamecontroller_poll_all();

   uint32_t buttons = g_current_input_data.pad_buttons[port] |
                      ((port == 0) ? apple_input_get_icade_buttons() : 0);

   if (buttons)
      for (int i = 0; i != 32; i ++)
         if (buttons & (1 << i))
            return i;

   return -1;
}

int32_t apple_input_find_any_axis(uint32_t port)
{
   apple_gamecontroller_poll_all();

   for (int i = 0; i < 4; i++)
   {
      int16_t value = g_current_input_data.pad_axis[port][i];
      
      if (abs(value) > 0x4000)
         return (value < 0) ? -(i + 1) : i + 1;
   }

   return 0;
}

// Game thread interface
static bool apple_key_pressed(enum retro_key key)
{
   if ((int)key >= 0 && key < RETROK_LAST)
      return g_polled_input_data.keys[input_translate_rk_to_keysym(key)];
   
   return false;
}

static bool apple_is_pressed(unsigned port_num, const struct retro_keybind *binds, unsigned key)
{
   return apple_key_pressed(binds[key].key) || input_joypad_pressed(g_joydriver, port_num, binds, key);
}

// Exported input driver
static void *apple_input_init(void)
{
   input_init_keyboard_lut(apple_key_map_hidusage);
   memset(&g_polled_input_data, 0, sizeof(g_polled_input_data));
   return (void*)-1;
}

static void apple_input_poll(void *data)
{
   apple_gamecontroller_poll_all();
   memcpy(&g_polled_input_data, &g_current_input_data, sizeof(apple_input_data_t));

   for (int i = 0; i != g_polled_input_data.touch_count; i ++)
   {
      input_translate_coord_viewport(g_polled_input_data.touches[i].screen_x, g_polled_input_data.touches[i].screen_y,
         &g_polled_input_data.touches[i].fixed_x, &g_polled_input_data.touches[i].fixed_y,
         &g_polled_input_data.touches[i].full_x, &g_polled_input_data.touches[i].full_y);
   }

   input_joypad_poll(g_joydriver);
   g_polled_input_data.pad_buttons[0] |= apple_input_get_icade_buttons();

   g_current_input_data.mouse_delta[0] = 0;
   g_current_input_data.mouse_delta[1] = 0;
}

static int16_t apple_input_state(void *data, const struct retro_keybind **binds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return (id < RARCH_BIND_LIST_END) ? apple_is_pressed(port, binds[port], id) : false;
         
      case RETRO_DEVICE_ANALOG:
         return input_joypad_analog(g_joydriver, port, index, id, binds[port]);
      
      case RETRO_DEVICE_KEYBOARD:
         return apple_key_pressed(id);

      case RETRO_DEVICE_MOUSE:
      {
         switch (id)
         {
            case RETRO_DEVICE_ID_MOUSE_X: return g_polled_input_data.mouse_delta[0];
            case RETRO_DEVICE_ID_MOUSE_Y: return g_polled_input_data.mouse_delta[1];
            case RETRO_DEVICE_ID_MOUSE_LEFT: return g_polled_input_data.mouse_buttons & 1;
            case RETRO_DEVICE_ID_MOUSE_RIGHT: return g_polled_input_data.mouse_buttons & 2;
         }
      }
      
      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
      {
         const bool want_full = device == RARCH_DEVICE_POINTER_SCREEN;
      
         if (index < g_polled_input_data.touch_count && index < MAX_TOUCHES)
         {
            const apple_touch_data_t* touch = &g_polled_input_data.touches[index];

            switch (id)
            {
               case RETRO_DEVICE_ID_POINTER_PRESSED: return 1;
               case RETRO_DEVICE_ID_POINTER_X: return want_full ? touch->full_x : touch->fixed_x;
               case RETRO_DEVICE_ID_POINTER_Y: return want_full ? touch->full_y : touch->fixed_y;
            }
         }
         
         return 0;
      }

      default:
         return 0;
   }
}

static bool apple_bind_button_pressed(void *data, int key)
{
   const struct retro_keybind *binds = g_settings.input.binds[0];
   return (key >= 0 && key < RARCH_BIND_LIST_END) ? apple_is_pressed(0, binds, key) : false;
}

static void apple_input_free_input(void *data)
{
   (void)data;
   g_joydriver->destroy();
}

static void apple_input_set_keybinds(void *data, unsigned device, unsigned port,
      unsigned id, unsigned keybind_action)
{
   (void)device;

#ifdef IOS
   if (keybind_action & (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS))
   {
      switch (device)
      {
         case DEVICE_NONE:
            break;
         case DEVICE_WIIMOTE:
            strlcpy(g_settings.input.device_names[port], "Wiimote + Classic",
               sizeof(g_settings.input.device_names[port]));
            g_settings.input.device[port] = device;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].joykey      = 22;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].joykey      = 21; 
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].joykey = 28;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].joykey  = 26;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_UP].joykey     = 16;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_DOWN].joykey   = 30;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_LEFT].joykey   = 17;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_RIGHT].joykey  = 31;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].joykey      = 20;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].joykey      = 19;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].joykey      = 29;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].joykey      = 25;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L2].joykey     = 23;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R2].joykey     = 18;
				g_settings.input.binds[port][RARCH_MENU_TOGGLE].joykey				 = 27;
            break;
         case DEVICE_SIXAXIS:
            strlcpy(g_settings.input.device_names[port], "SixAxis/DualShock3",
               sizeof(g_settings.input.device_names[port]));
            g_settings.input.device[port] = device;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].joykey      = 0;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].joykey      = 1; 
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].joykey = 2;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].joykey  = 3;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_UP].joykey     = 4;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_DOWN].joykey   = 5;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_LEFT].joykey   = 6;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_RIGHT].joykey  = 7;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].joykey      = 8;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].joykey      = 9;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].joykey      = 10;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].joykey      = 11;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L2].joykey     = 12; 
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R2].joykey     = 13;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L3].joykey     = 14;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R3].joykey     = 15;
            g_settings.input.binds[port][RARCH_MENU_TOGGLE].joykey             = 16;
            break;
      }
   }
#endif
}

static bool apple_input_set_rumble(void *data, unsigned port, enum retro_rumble_effect effect, uint16_t strength)
{
   return input_joypad_set_rumble(g_joydriver, port, effect, strength);
}

static uint64_t apple_input_get_capabilities(void *data)
{
   uint64_t caps = 0;

   caps |= (1 << RETRO_DEVICE_JOYPAD);
   caps |= (1 << RETRO_DEVICE_MOUSE);
   caps |= (1 << RETRO_DEVICE_KEYBOARD);
   caps |= (1 << RETRO_DEVICE_POINTER);
   caps |= (1 << RETRO_DEVICE_ANALOG);

   return caps;
}

const input_driver_t input_apple = {
   apple_input_init,
   apple_input_poll,
   apple_input_state,
   apple_bind_button_pressed,
   apple_input_free_input,
   apple_input_set_keybinds,
   NULL,
   apple_input_get_capabilities,
   "apple_input",
   NULL,
   apple_input_set_rumble,
   NULL
};
