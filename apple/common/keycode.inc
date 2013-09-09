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

#include "keycode.h"

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
