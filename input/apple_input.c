/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include "input_common.h"
#include "apple_input.h"
#include "../general.h"
#include "../driver.h"

#include "apple_keycode.h"

const struct apple_key_name_map_entry apple_key_name_map[] =
{
   { "left", KEY_Left },               { "right", KEY_Right },
   { "up", KEY_Up },                   { "down", KEY_Down },
   { "enter", KEY_Enter },             { "kp_enter", KP_Enter },
   { "space", KEY_Space },             { "tab", KEY_Tab },
   { "shift", KEY_LeftShift },         { "rshift", KEY_RightShift },
   { "ctrl", KEY_LeftControl },        { "alt", KEY_LeftAlt },
   { "escape", KEY_Escape },           { "backspace", KEY_DeleteForward },
   { "backquote", KEY_Grave },         { "pause", KEY_Pause },

   { "f1", KEY_F1 },                   { "f2", KEY_F2 },
   { "f3", KEY_F3 },                   { "f4", KEY_F4 },
   { "f5", KEY_F5 },                   { "f6", KEY_F6 },
   { "f7", KEY_F7 },                   { "f8", KEY_F8 },
   { "f9", KEY_F9 },                   { "f10", KEY_F10 },
   { "f11", KEY_F11 },                 { "f12", KEY_F12 },

   { "num0", KEY_0 },                  { "num1", KEY_1 },
   { "num2", KEY_2 },                  { "num3", KEY_3 },
   { "num4", KEY_4 },                  { "num5", KEY_5 },
   { "num6", KEY_6 },                  { "num7", KEY_7 },
   { "num8", KEY_8 },                  { "num9", KEY_9 },

   { "insert", KEY_Insert },           { "del", KEY_DeleteForward },
   { "home", KEY_Home },               { "end", KEY_End },
   { "pageup", KEY_PageUp },           { "pagedown", KEY_PageDown },

   { "add", KP_Add },                  { "subtract", KP_Subtract },
   { "multiply", KP_Multiply },        { "divide", KP_Divide },
   { "keypad0", KP_0 },                { "keypad1", KP_1 },
   { "keypad2", KP_2 },                { "keypad3", KP_3 },
   { "keypad4", KP_4 },                { "keypad5", KP_5 },
   { "keypad6", KP_6 },                { "keypad7", KP_7 },
   { "keypad8", KP_8 },                { "keypad9", KP_9 },

   { "period", KEY_Period },           { "capslock", KEY_CapsLock },
   { "numlock", KP_NumLock },          { "print_screen", KEY_PrintScreen },
   { "scroll_lock", KEY_ScrollLock },

   { "a", KEY_A }, { "b", KEY_B }, { "c", KEY_C }, { "d", KEY_D },
   { "e", KEY_E }, { "f", KEY_F }, { "g", KEY_G }, { "h", KEY_H },
   { "i", KEY_I }, { "j", KEY_J }, { "k", KEY_K }, { "l", KEY_L },
   { "m", KEY_M }, { "n", KEY_N }, { "o", KEY_O }, { "p", KEY_P },
   { "q", KEY_Q }, { "r", KEY_R }, { "s", KEY_S }, { "t", KEY_T },
   { "u", KEY_U }, { "v", KEY_V }, { "w", KEY_W }, { "x", KEY_X },
   { "y", KEY_Y }, { "z", KEY_Z },

   { "nul", 0x00},
};

const struct rarch_key_map apple_key_map_hidusage[] =
{
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

const char* apple_keycode_hidusage_to_name(uint32_t hid_usage)
{
   for (int i = 0; apple_key_name_map[i].hid_id; i ++)
      if (apple_key_name_map[i].hid_id == hid_usage)
         return apple_key_name_map[i].keyname;

   return "nul";
}

#ifdef IOS
extern void apple_gamecontroller_poll_all(void);

enum input_devices
{
   DEVICE_NONE = 0,
   DEVICE_WIIMOTE,
   DEVICE_SIXAXIS,
   DEVICE_MFI,
   DEVICE_LAST
};

#endif

static const rarch_joypad_driver_t *joypad;

apple_input_data_t g_current_input_data;

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

static bool icade_enabled;
static bool small_keyboard_enabled;
static bool small_keyboard_active;
static uint32_t icade_buttons;

static bool handle_small_keyboard(unsigned* code, bool down)
{
   static const struct { uint8_t orig; uint8_t mod; } mapping_def[] =
   {
      { KEY_Grave,      KEY_Escape     }, { KEY_1,          KEY_F1         },
      { KEY_2,          KEY_F2         }, { KEY_3,          KEY_F3         },
      { KEY_4,          KEY_F4         }, { KEY_5,          KEY_F5         },
      { KEY_6,          KEY_F6         }, { KEY_7,          KEY_F7         },
      { KEY_8,          KEY_F8         }, { KEY_9,          KEY_F9         },
      { KEY_0,          KEY_F10        }, { KEY_Minus,      KEY_F11        },
      { KEY_Equals,     KEY_F12        }, { KEY_Up,         KEY_PageUp     },
      { KEY_Down,       KEY_PageDown   }, { KEY_Left,       KEY_Home       },
      { KEY_Right,      KEY_End        }, { KEY_Q,          KP_7           },
      { KEY_W,          KP_8           }, { KEY_E,          KP_9           },
      { KEY_A,          KP_4           }, { KEY_S,          KP_5           },
      { KEY_D,          KP_6           }, { KEY_Z,          KP_1           },
      { KEY_X,          KP_2           }, { KEY_C,          KP_3           },
      { 0 }
   };
   
   static uint8_t mapping[128];
   static bool map_initialized;
   
   if (!map_initialized)
   {
      for (int i = 0; mapping_def[i].orig; i ++)
         mapping[mapping_def[i].orig] = mapping_def[i].mod;
      map_initialized = true;
   }

   if (*code == KEY_RightShift)
   {
      small_keyboard_active = down;
      *code = 0;
      return true;
   }
   
   unsigned translated_code = (*code < 128) ? mapping[*code] : 0;
   
   // Allow old keys to be released
   if (!down && g_current_input_data.keys[*code])
      return false;

   if ((!down && g_current_input_data.keys[translated_code]) ||
       small_keyboard_active)
   {
      *code = translated_code;
      return true;
   }

   return false;
}

void apple_input_enable_small_keyboard(bool on)
{
   small_keyboard_enabled = on;
}

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

void apple_input_keyboard_event(bool down, unsigned code, uint32_t character, uint32_t mod)
{
   code = HIDKEY(code);

   if (icade_enabled)
   {
      handle_icade_event(code);
      return;
   }

   if (small_keyboard_enabled && handle_small_keyboard(&code, down))
      character = 0;
   
   if (code == 0 || code >= MAX_KEYS)
      return;

   g_current_input_data.keys[code] = down;
   
   /* This is copied here as it isn't defined in any standard iOS header */
   enum
   {
      NSAlphaShiftKeyMask = 1 << 16,
      NSShiftKeyMask      = 1 << 17,
      NSControlKeyMask    = 1 << 18,
      NSAlternateKeyMask  = 1 << 19,
      NSCommandKeyMask    = 1 << 20,
      NSNumericPadKeyMask = 1 << 21,
      NSHelpKeyMask       = 1 << 22,
      NSFunctionKeyMask   = 1 << 23,
      NSDeviceIndependentModifierFlagsMask = 0xffff0000U
   };

   enum retro_mod mods = RETROKMOD_NONE;
   mods |= (mod & NSAlphaShiftKeyMask) ? RETROKMOD_CAPSLOCK : 0;
   mods |= (mod & NSShiftKeyMask)      ? RETROKMOD_SHIFT : 0;
   mods |= (mod & NSControlKeyMask)    ? RETROKMOD_CTRL : 0;
   mods |= (mod & NSAlternateKeyMask)  ? RETROKMOD_ALT : 0;
   mods |= (mod & NSCommandKeyMask)    ? RETROKMOD_META : 0;
   mods |= (mod & NSNumericPadKeyMask) ? RETROKMOD_NUMLOCK : 0;

   input_keyboard_event(down, input_translate_keysym_to_rk(code), character, mods);
}


int32_t apple_input_find_any_key(void)
{
   unsigned i;

#ifdef IOS
   apple_gamecontroller_poll_all();
#endif
   input_init_keyboard_lut(apple_key_map_hidusage);

   for (i = 0; apple_key_name_map[i].hid_id; i++)
      if (g_current_input_data.keys[apple_key_name_map[i].hid_id])
         return apple_key_name_map[i].hid_id;

   return 0;
}

int32_t apple_input_find_any_button(uint32_t port)
{
   unsigned i, buttons;
#ifdef IOS
   apple_gamecontroller_poll_all();
#endif

   buttons = g_current_input_data.pad_buttons[port] |
      ((port == 0) ? apple_input_get_icade_buttons() : 0);

   if (buttons)
      for (i = 0; i != 32; i ++)
         if (buttons & (1 << i))
            return i;

   return -1;
}

int32_t apple_input_find_any_axis(uint32_t port)
{
#ifdef IOS
   apple_gamecontroller_poll_all();
#endif

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
      return g_current_input_data.keys[input_translate_rk_to_keysym(key)];
   
   return false;
}

static bool apple_is_pressed(unsigned port_num, const struct retro_keybind *binds, unsigned key)
{
   return apple_key_pressed(binds[key].key) || input_joypad_pressed(joypad, port_num, binds, key);
}

// Exported input driver
static void *apple_input_init(void)
{
   if (driver.input_data)
       return driver.input_data;

   input_init_keyboard_lut(apple_key_map_hidusage);
   memset(&g_current_input_data, 0, sizeof(g_current_input_data));

   joypad = input_joypad_init_driver(g_settings.input.joypad_driver);
    
   driver.input_data_own = true;
   return (void*)-1;
}

static void apple_input_poll(void *data)
{
   int i;
   (void)data;

#ifdef IOS
   apple_gamecontroller_poll_all();
#endif

   for (i = 0; i < g_current_input_data.touch_count; i ++)
   {
      input_translate_coord_viewport(g_current_input_data.touches[i].screen_x, g_current_input_data.touches[i].screen_y,
         &g_current_input_data.touches[i].fixed_x, &g_current_input_data.touches[i].fixed_y,
         &g_current_input_data.touches[i].full_x, &g_current_input_data.touches[i].full_y);
   }

   if (joypad)
      joypad->poll();

   g_current_input_data.pad_buttons[0] |= apple_input_get_icade_buttons();

   g_current_input_data.mouse_delta[0] = 0;
   g_current_input_data.mouse_delta[1] = 0;
}

static int16_t apple_input_state(void *data, const struct retro_keybind **binds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   (void)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return (id < RARCH_BIND_LIST_END) ? apple_is_pressed(port, binds[port], id) : 0;
         
      case RETRO_DEVICE_ANALOG:
         return input_joypad_analog(joypad, port, index, id, binds[port]);
      
      case RETRO_DEVICE_KEYBOARD:
         return apple_key_pressed(id);

      case RETRO_DEVICE_MOUSE:
      {
         switch (id)
         {
            case RETRO_DEVICE_ID_MOUSE_X:
               return g_current_input_data.mouse_delta[0];
            case RETRO_DEVICE_ID_MOUSE_Y:
               return g_current_input_data.mouse_delta[1];
            case RETRO_DEVICE_ID_MOUSE_LEFT:
               return g_current_input_data.mouse_buttons & 1;
            case RETRO_DEVICE_ID_MOUSE_RIGHT:
               return g_current_input_data.mouse_buttons & 2;
         }
      }
      
      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
      {
         const bool want_full = device == RARCH_DEVICE_POINTER_SCREEN;
      
         if (index < g_current_input_data.touch_count && index < MAX_TOUCHES)
         {
            const apple_touch_data_t *touch = (const apple_touch_data_t *)&g_current_input_data.touches[index];
            int16_t x = want_full ? touch->full_x : touch->fixed_x;
            int16_t y = want_full ? touch->full_y : touch->fixed_y;

            switch (id)
            {
               case RETRO_DEVICE_ID_POINTER_PRESSED:
                  return (x != -0x8000) && (y != -0x8000);
               case RETRO_DEVICE_ID_POINTER_X:
                  return x;
               case RETRO_DEVICE_ID_POINTER_Y:
                  return y;
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
   (void)data;

   return (key >= 0 && key < RARCH_BIND_LIST_END) ? apple_is_pressed(0, binds, key) : false;
}

static void apple_input_free_input(void *data)
{
   (void)data;
    
   if (joypad)
      joypad->destroy();
}

#ifdef IOS
static void apple_input_set_keybinds(void *data, unsigned device, unsigned port,
      unsigned id, unsigned keybind_action)
{
   (void)device;

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
          case DEVICE_MFI:
              strlcpy(g_settings.input.device_names[port], "MFi Gamepad", sizeof(g_settings.input.device_names[port]));
              g_settings.input.device[port] = device;
              g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].joykey      = RETRO_DEVICE_ID_JOYPAD_B;
              g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].joykey      = RETRO_DEVICE_ID_JOYPAD_Y;
              g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].joykey = RETRO_DEVICE_ID_JOYPAD_SELECT;
              g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].joykey  = RETRO_DEVICE_ID_JOYPAD_START;
              g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_UP].joykey     = RETRO_DEVICE_ID_JOYPAD_UP;
              g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_DOWN].joykey   = RETRO_DEVICE_ID_JOYPAD_DOWN;
              g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_LEFT].joykey   = RETRO_DEVICE_ID_JOYPAD_LEFT;
              g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_RIGHT].joykey  = RETRO_DEVICE_ID_JOYPAD_RIGHT;
              g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].joykey      = RETRO_DEVICE_ID_JOYPAD_A;
              g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].joykey      = RETRO_DEVICE_ID_JOYPAD_X;
              g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].joykey      = RETRO_DEVICE_ID_JOYPAD_L;
              g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].joykey      = RETRO_DEVICE_ID_JOYPAD_R;
              g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L2].joykey     = RETRO_DEVICE_ID_JOYPAD_L2;
              g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R2].joykey     = RETRO_DEVICE_ID_JOYPAD_R2;
              g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L3].joykey     = RETRO_DEVICE_ID_JOYPAD_L3;
              g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R3].joykey     = RETRO_DEVICE_ID_JOYPAD_R3;
              g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_PLUS].def_joyaxis      = AXIS_POS(0);
              g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_MINUS].def_joyaxis    = AXIS_NEG(0);
              g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_PLUS].def_joyaxis      = AXIS_NEG(1);
              g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_MINUS].def_joyaxis    = AXIS_POS(1);
              g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_PLUS].def_joyaxis    = AXIS_POS(2);
              g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_MINUS].def_joyaxis    = AXIS_NEG(2);
              g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_PLUS].def_joyaxis    = AXIS_NEG(3);
              g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_MINUS].def_joyaxis    = AXIS_POS(3);
              
              for (int i = 0; i < RARCH_CUSTOM_BIND_LIST_END; i++) {
                  g_settings.input.binds[port][i].joyaxis = g_settings.input.binds[port][i].def_joyaxis;
              }
              
              break;
      }
   }

   if (keybind_action & (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL))
   {
      struct platform_bind *ret = (struct platform_bind*)data;

      if (ret && ret->joykey == NO_BTN)
         strlcpy(ret->desc, "No button", sizeof(ret->desc));
      else
         snprintf(ret->desc, sizeof(ret->desc), "Button %llu", ret->joykey);
   }
}
#endif

static bool apple_input_set_rumble(void *data, unsigned port, enum retro_rumble_effect effect, uint16_t strength)
{
   (void)data;
   return input_joypad_set_rumble(joypad, port, effect, strength);
}

static uint64_t apple_input_get_capabilities(void *data)
{
   uint64_t caps = 0;

   (void)data;

   caps |= (1 << RETRO_DEVICE_JOYPAD);
   caps |= (1 << RETRO_DEVICE_MOUSE);
   caps |= (1 << RETRO_DEVICE_KEYBOARD);
   caps |= (1 << RETRO_DEVICE_POINTER);
   caps |= (1 << RETRO_DEVICE_ANALOG);

   return caps;
}

#ifdef IOS
static unsigned apple_devices_size(void *data)
{
   return DEVICE_LAST;
}
#endif

const rarch_joypad_driver_t *apple_get_joypad_driver(void *data)
{
   return joypad;
}

const input_driver_t input_apple = {
   apple_input_init,
   apple_input_poll,
   apple_input_state,
   apple_bind_button_pressed,
   apple_input_free_input,
#ifdef IOS
   apple_input_set_keybinds,
#else
   NULL,
#endif
   NULL,
   NULL,
   apple_input_get_capabilities,
#ifdef IOS
   apple_devices_size,
#else
    NULL,
#endif
   "apple_input",
   NULL,
   apple_input_set_rumble,
   apple_get_joypad_driver
};
