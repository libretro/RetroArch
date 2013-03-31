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

#include <unistd.h>
#include "input/input_common.h"
#include "ios_input.h"
#include "general.h"
#include "driver.h"

extern const rarch_joypad_driver_t ios_joypad;
static const rarch_joypad_driver_t* const g_joydriver = &ios_joypad;

static const struct rarch_key_map rarch_key_map_hidusage[];

// Key event data, called in main.m
#define MAX_KEY_EVENTS 32

static struct
{
   bool down;
   unsigned keycode;
   uint32_t character;
   uint16_t modifiers;
}  g_key_events[MAX_KEY_EVENTS];
static int g_pending_key_events;

void ios_add_key_event(bool down, unsigned keycode, uint32_t character, uint16_t keyModifiers)
{
   if (!g_extern.system.key_event)
      return;

   if (g_pending_key_events == MAX_KEY_EVENTS)
   {
      RARCH_LOG("Key event buffer overlow");
      return;
   }
   
   g_key_events[g_pending_key_events].down = down;
   g_key_events[g_pending_key_events].keycode = keycode;
   g_key_events[g_pending_key_events].character = character;
   g_key_events[g_pending_key_events].modifiers = keyModifiers;
   g_pending_key_events ++;
}

// Non-exported helpers
static bool ios_key_pressed(enum retro_key key)
{
   if ((int)key >= 0 && key < RETROK_LAST)
      return ios_key_list[input_translate_rk_to_keysym(key)];
   
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
   g_pending_key_events = 0;
   return (void*)-1;
}

static void ios_input_poll(void *data)
{
   if (g_extern.system.key_event)
   {
      for (int i = 0; i < g_pending_key_events; i ++)
      {
         const enum retro_key keycode = input_translate_keysym_to_rk(g_key_events[i].keycode);
         g_extern.system.key_event(g_key_events[i].down, keycode, g_key_events[i].character, g_key_events[i].modifiers);
      }
   }
   
   g_pending_key_events = 0;


   for (int i = 0; i != ios_touch_count; i ++)
   {
      input_translate_coord_viewport(ios_touch_list[i].screen_x, ios_touch_list[i].screen_y,
         &ios_touch_list[i].fixed_x, &ios_touch_list[i].fixed_y,
         &ios_touch_list[i].full_x, &ios_touch_list[i].full_y);
   }

   input_joypad_poll(g_joydriver);
}

static int16_t ios_input_state(void *data, const struct retro_keybind **binds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return (id < RARCH_BIND_LIST_END) ? ios_is_pressed(port, &binds[port][id]) : false;
         
      case RETRO_DEVICE_ANALOG:
         return input_joypad_analog(g_joydriver, port, index, id, binds[port]);
      
      case RETRO_DEVICE_KEYBOARD:
         return ios_key_pressed(id);
      
      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
      {
         const bool want_full = device == RARCH_DEVICE_POINTER_SCREEN;
      
         if (index < ios_touch_count && index < MAX_TOUCHES)
         {
            const touch_data_t* touch = &ios_touch_list[index];

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

static bool ios_bind_button_pressed(void *data, int key)
{
   const struct retro_keybind *binds = g_settings.input.binds[0];
   return (key >= 0 && key < RARCH_BIND_LIST_END) ? ios_is_pressed(0, &binds[key]) : false;
}

static void ios_input_free_input(void *data)
{
   (void)data;
}

const input_driver_t input_ios = {
   ios_input_init,
   ios_input_poll,
   ios_input_state,
   ios_bind_button_pressed,
   ios_input_free_input,
   NULL,
   "ios_input",
};

// Key table
#include "keycode.h"
static const struct rarch_key_map rarch_key_map_hidusage[] = {
   { KEY_Delete, RETROK_BACKSPACE },
   { KEY_Tab, RETROK_TAB },
//   RETROK_CLEAR },
   { KEY_Enter, RETROK_RETURN },
   { KEY_Pause, RETROK_PAUSE },
   { KEY_Escape, RETROK_ESCAPE },
   { KEY_Space, RETROK_SPACE },
//   RETROK_EXCLAIM },
//   RETROK_QUOTEDBL },
//   RETROK_HASH },
//   RETROK_DOLLAR },
//   RETROK_AMPERSAND },
   { KEY_Quote, RETROK_QUOTE },
//   RETROK_LEFTPAREN },
//   RETROK_RIGHTPAREN },
//   RETROK_ASTERISK },
//   RETROK_PLUS },
   { KEY_Comma, RETROK_COMMA },
   { KEY_Minus, RETROK_MINUS },
   { KEY_Period, RETROK_PERIOD },
   { KEY_Slash, RETROK_SLASH },
   { KEY_0, RETROK_0 },
   { KEY_1, RETROK_1 },
   { KEY_2, RETROK_2 },
   { KEY_3, RETROK_3 },
   { KEY_4, RETROK_4 },
   { KEY_5, RETROK_5 },
   { KEY_6, RETROK_6 },
   { KEY_7, RETROK_7 },
   { KEY_8, RETROK_8 },
   { KEY_9, RETROK_9 },
//   RETROK_COLON },
   { KEY_Semicolon, RETROK_SEMICOLON },
//   RETROK_LESS },
   { KEY_Equals, RETROK_EQUALS },
//   RETROK_GREATER },
//   RETROK_QUESTION },
//   RETROK_AT },
   { KEY_LeftBracket, RETROK_LEFTBRACKET },
   { KEY_Backslash, RETROK_BACKSLASH },
   { KEY_RightBracket, RETROK_RIGHTBRACKET },
//   RETROK_CARET },
//   RETROK_UNDERSCORE },
   { KEY_Grave, RETROK_BACKQUOTE },
   { KEY_A, RETROK_a },
   { KEY_B, RETROK_b },
   { KEY_C, RETROK_c },
   { KEY_D, RETROK_d },
   { KEY_E, RETROK_e },
   { KEY_F, RETROK_f },
   { KEY_G, RETROK_g },
   { KEY_H, RETROK_h },
   { KEY_I, RETROK_i },
   { KEY_J, RETROK_j },
   { KEY_K, RETROK_k },
   { KEY_L, RETROK_l },
   { KEY_M, RETROK_m },
   { KEY_N, RETROK_n },
   { KEY_O, RETROK_o },
   { KEY_P, RETROK_p },
   { KEY_Q, RETROK_q },
   { KEY_R, RETROK_r },
   { KEY_S, RETROK_s },
   { KEY_T, RETROK_t },
   { KEY_U, RETROK_u },
   { KEY_V, RETROK_v },
   { KEY_W, RETROK_w },
   { KEY_X, RETROK_x },
   { KEY_Y, RETROK_y },
   { KEY_Z, RETROK_z },
   { KEY_DeleteForward, RETROK_DELETE },

   { KP_0, RETROK_KP0 },
   { KP_1, RETROK_KP1 },
   { KP_2, RETROK_KP2 },
   { KP_3, RETROK_KP3 },
   { KP_4, RETROK_KP4 },
   { KP_5, RETROK_KP5 },
   { KP_6, RETROK_KP6 },
   { KP_7, RETROK_KP7 },
   { KP_8, RETROK_KP8 },
   { KP_9, RETROK_KP9 },
   { KP_Point, RETROK_KP_PERIOD },
   { KP_Divide, RETROK_KP_DIVIDE },
   { KP_Multiply, RETROK_KP_MULTIPLY },
   { KP_Subtract, RETROK_KP_MINUS },
   { KP_Add, RETROK_KP_PLUS },
   { KP_Enter, RETROK_KP_ENTER },
   { KP_Equals, RETROK_KP_EQUALS },

   { KEY_Up, RETROK_UP },
   { KEY_Down, RETROK_DOWN },
   { KEY_Right, RETROK_RIGHT },
   { KEY_Left, RETROK_LEFT },
   { KEY_Insert, RETROK_INSERT },
   { KEY_Home, RETROK_HOME },
   { KEY_End, RETROK_END },
   { KEY_PageUp, RETROK_PAGEUP },
   { KEY_PageDown, RETROK_PAGEDOWN },

   { KEY_F1, RETROK_F1 },
   { KEY_F2, RETROK_F2 },
   { KEY_F3, RETROK_F3 },
   { KEY_F4, RETROK_F4 },
   { KEY_F5, RETROK_F5 },
   { KEY_F6, RETROK_F6 },
   { KEY_F7, RETROK_F7 },
   { KEY_F8, RETROK_F8 },
   { KEY_F9, RETROK_F9 },
   { KEY_F10, RETROK_F10 },
   { KEY_F11, RETROK_F11 },
   { KEY_F12, RETROK_F12 },
   { KEY_F13, RETROK_F13 },
   { KEY_F14, RETROK_F14 },
   { KEY_F15, RETROK_F15 },

//   RETROK_NUMLOCK },
   { KEY_CapsLock, RETROK_CAPSLOCK },
//   RETROK_SCROLLOCK },
   { KEY_RightShift, RETROK_RSHIFT },
   { KEY_LeftShift, RETROK_LSHIFT },
   { KEY_RightControl, RETROK_RCTRL },
   { KEY_LeftControl, RETROK_LCTRL },
   { KEY_RightAlt, RETROK_RALT },
   { KEY_LeftAlt, RETROK_LALT },
   { KEY_RightGUI, RETROK_RMETA },
   { KEY_LeftGUI, RETROK_RMETA },
//   RETROK_LSUPER },
//   RETROK_RSUPER },
//   RETROK_MODE },
//   RETROK_COMPOSE },

//   RETROK_HELP },
   { KEY_PrintScreen, RETROK_PRINT },
//   RETROK_SYSREQ },
//   RETROK_BREAK },
   { KEY_Menu, RETROK_MENU },
//   RETROK_POWER },
//   RETROK_EURO },
//   RETROK_UNDO },
   { 0, RETROK_UNKNOWN }
};
