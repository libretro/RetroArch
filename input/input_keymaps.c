/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "input_keymaps.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "../general.h"
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef __APPLE__
#include "drivers/apple_keycode.h"
#endif

#ifdef __linux__
#include <linux/input.h>
#include <linux/kd.h>
#endif

const struct input_key_map input_config_key_map[] = {
   { "left", RETROK_LEFT },
   { "right", RETROK_RIGHT },
   { "up", RETROK_UP },
   { "down", RETROK_DOWN },
   { "enter", RETROK_RETURN },
   { "kp_enter", RETROK_KP_ENTER },
   { "tab", RETROK_TAB },
   { "insert", RETROK_INSERT },
   { "del", RETROK_DELETE },
   { "end", RETROK_END },
   { "home", RETROK_HOME },
   { "rshift", RETROK_RSHIFT },
   { "shift", RETROK_LSHIFT },
   { "ctrl", RETROK_LCTRL },
   { "alt", RETROK_LALT },
   { "space", RETROK_SPACE },
   { "escape", RETROK_ESCAPE },
   { "add", RETROK_KP_PLUS },
   { "subtract", RETROK_KP_MINUS },
   { "kp_plus", RETROK_KP_PLUS },
   { "kp_minus", RETROK_KP_MINUS },
   { "f1", RETROK_F1 },
   { "f2", RETROK_F2 },
   { "f3", RETROK_F3 },
   { "f4", RETROK_F4 },
   { "f5", RETROK_F5 },
   { "f6", RETROK_F6 },
   { "f7", RETROK_F7 },
   { "f8", RETROK_F8 },
   { "f9", RETROK_F9 },
   { "f10", RETROK_F10 },
   { "f11", RETROK_F11 },
   { "f12", RETROK_F12 },
   { "num0", RETROK_0 },
   { "num1", RETROK_1 },
   { "num2", RETROK_2 },
   { "num3", RETROK_3 },
   { "num4", RETROK_4 },
   { "num5", RETROK_5 },
   { "num6", RETROK_6 },
   { "num7", RETROK_7 },
   { "num8", RETROK_8 },
   { "num9", RETROK_9 },
   { "pageup", RETROK_PAGEUP },
   { "pagedown", RETROK_PAGEDOWN },
   { "keypad0", RETROK_KP0 },
   { "keypad1", RETROK_KP1 },
   { "keypad2", RETROK_KP2 },
   { "keypad3", RETROK_KP3 },
   { "keypad4", RETROK_KP4 },
   { "keypad5", RETROK_KP5 },
   { "keypad6", RETROK_KP6 },
   { "keypad7", RETROK_KP7 },
   { "keypad8", RETROK_KP8 },
   { "keypad9", RETROK_KP9 },
   { "period", RETROK_PERIOD },
   { "capslock", RETROK_CAPSLOCK },
   { "numlock", RETROK_NUMLOCK },
   { "backspace", RETROK_BACKSPACE },
   { "multiply", RETROK_KP_MULTIPLY },
   { "divide", RETROK_KP_DIVIDE },
   { "print_screen", RETROK_PRINT },
   { "scroll_lock", RETROK_SCROLLOCK },
   { "tilde", RETROK_BACKQUOTE },
   { "backquote", RETROK_BACKQUOTE },
   { "pause", RETROK_PAUSE },

   /* Keys that weren't mappable before */
   { "quote", RETROK_QUOTE },
   { "comma", RETROK_COMMA },
   { "minus", RETROK_MINUS },
   { "slash", RETROK_SLASH },
   { "semicolon", RETROK_SEMICOLON },
   { "equals", RETROK_EQUALS },
   { "leftbracket", RETROK_LEFTBRACKET },
   { "backslash", RETROK_BACKSLASH },
   { "rightbracket", RETROK_RIGHTBRACKET },
   { "kp_period", RETROK_KP_PERIOD },
   { "kp_equals", RETROK_KP_EQUALS },
   { "rctrl", RETROK_RCTRL },
   { "ralt", RETROK_RALT },

   /* Keys not referenced in any keyboard mapping 
    * (except perhaps rarch_key_map_apple_hid) */
   { "caret", RETROK_CARET },
   { "underscore", RETROK_UNDERSCORE },
   { "exclaim", RETROK_EXCLAIM },
   { "quotedbl", RETROK_QUOTEDBL },
   { "hash", RETROK_HASH },
   { "dollar", RETROK_DOLLAR },
   { "ampersand", RETROK_AMPERSAND },
   { "leftparen", RETROK_LEFTPAREN },
   { "rightparen", RETROK_RIGHTPAREN },
   { "asterisk", RETROK_ASTERISK },
   { "plus", RETROK_PLUS },
   { "colon", RETROK_COLON },
   { "less", RETROK_LESS },
   { "greater", RETROK_GREATER },
   { "question", RETROK_QUESTION },
   { "at", RETROK_AT },

   { "f13", RETROK_F13 },
   { "f14", RETROK_F14 },
   { "f15", RETROK_F15 },

   { "rmeta", RETROK_RMETA },
   { "lmeta", RETROK_LMETA },
   { "lsuper", RETROK_LSUPER },
   { "rsuper", RETROK_RSUPER },
   { "mode", RETROK_MODE },
   { "compose", RETROK_COMPOSE },

   { "help", RETROK_HELP },
   { "sysreq", RETROK_SYSREQ },
   { "break", RETROK_BREAK },
   { "menu", RETROK_MENU },
   { "power", RETROK_POWER },
   { "euro", RETROK_EURO },
   { "undo", RETROK_UNDO },
   { "clear", RETROK_CLEAR },

   { "nul", RETROK_UNKNOWN },
   { NULL, RETROK_UNKNOWN },
};

#ifdef EMSCRIPTEN
const struct rarch_key_map rarch_key_map_rwebinput[] = {
   { 37, RETROK_LEFT },
   { 39, RETROK_RIGHT },
   { 38, RETROK_UP },
   { 40, RETROK_DOWN },
   { 13, RETROK_RETURN },
   { 9, RETROK_TAB },
   { 45, RETROK_INSERT },
   { 46, RETROK_DELETE },
   { 16, RETROK_RSHIFT },
   { 16, RETROK_LSHIFT },
   { 17, RETROK_LCTRL },
   { 35, RETROK_END },
   { 36, RETROK_HOME },
   { 34, RETROK_PAGEDOWN },
   { 33, RETROK_PAGEUP },
   { 18, RETROK_LALT },
   { 32, RETROK_SPACE },
   { 27, RETROK_ESCAPE },
   { 8, RETROK_BACKSPACE },
   { 13, RETROK_KP_ENTER },
   { 107, RETROK_KP_PLUS },
   { 109, RETROK_KP_MINUS },
   { 106, RETROK_KP_MULTIPLY },
   { 111, RETROK_KP_DIVIDE },
   { 192, RETROK_BACKQUOTE },
   { 19, RETROK_PAUSE },
   { 96, RETROK_KP0 },
   { 97, RETROK_KP1 },
   { 98, RETROK_KP2 },
   { 99, RETROK_KP3 },
   { 100, RETROK_KP4 },
   { 101, RETROK_KP5 },
   { 102, RETROK_KP6 },
   { 103, RETROK_KP7 },
   { 104, RETROK_KP8 },
   { 105, RETROK_KP9 },
   { 48, RETROK_0 },
   { 49, RETROK_1 },
   { 50, RETROK_2 },
   { 51, RETROK_3 },
   { 52, RETROK_4 },
   { 53, RETROK_5 },
   { 54, RETROK_6 },
   { 55, RETROK_7 },
   { 56, RETROK_8 },
   { 57, RETROK_9 },
   { 112, RETROK_F1 },
   { 113, RETROK_F2 },
   { 114, RETROK_F3 },
   { 115, RETROK_F4 },
   { 116, RETROK_F5 },
   { 117, RETROK_F6 },
   { 118, RETROK_F7 },
   { 119, RETROK_F8 },
   { 120, RETROK_F9 },
   { 121, RETROK_F10 },
   { 122, RETROK_F11 },
   { 123, RETROK_F12 },
   { 65, RETROK_a },
   { 66, RETROK_b },
   { 67, RETROK_c },
   { 68, RETROK_d },
   { 69, RETROK_e },
   { 70, RETROK_f },
   { 71, RETROK_g },
   { 72, RETROK_h },
   { 73, RETROK_i },
   { 74, RETROK_j },
   { 75, RETROK_k },
   { 76, RETROK_l },
   { 77, RETROK_m },
   { 78, RETROK_n },
   { 79, RETROK_o },
   { 80, RETROK_p },
   { 81, RETROK_q },
   { 82, RETROK_r },
   { 83, RETROK_s },
   { 84, RETROK_t },
   { 85, RETROK_u },
   { 86, RETROK_v },
   { 87, RETROK_w },
   { 88, RETROK_x },
   { 89, RETROK_y },
   { 90, RETROK_z },
   { 222, RETROK_QUOTE },
   { 188, RETROK_COMMA },
   { 173, RETROK_MINUS },
   { 191, RETROK_SLASH },
   { 59, RETROK_SEMICOLON },
   { 61, RETROK_EQUALS },
   { 219, RETROK_LEFTBRACKET },
   { 220, RETROK_BACKSLASH },
   { 221, RETROK_RIGHTBRACKET },
   { 188, RETROK_KP_PERIOD },
   { 17, RETROK_RCTRL },
   { 18, RETROK_RALT },
   { 190, RETROK_PERIOD },
   { 145, RETROK_SCROLLOCK },
   { 20, RETROK_CAPSLOCK },
   { 144, RETROK_NUMLOCK },
   { 0, RETROK_UNKNOWN },
};
#endif

#ifdef __linux__
const struct rarch_key_map rarch_key_map_linux[] = {
   { KEY_BACKSPACE, RETROK_BACKSPACE },
   { KEY_TAB, RETROK_TAB },
   { KEY_CLEAR, RETROK_CLEAR },
   { KEY_ENTER, RETROK_RETURN },
   { KEY_PAUSE, RETROK_PAUSE },
   { KEY_ESC, RETROK_ESCAPE },
   { KEY_SPACE, RETROK_SPACE },
   /* { ?, RETROK_EXCLAIM }, */
   /* { ?, RETROK_QUOTEDBL }, */
   /* { ?, RETROK_HASH }, */
#ifndef ANDROID
   { KEY_DOLLAR, RETROK_DOLLAR },
#endif
   /* { ?, RETROK_AMPERSAND }, */
   { KEY_APOSTROPHE, RETROK_QUOTE },
   { KEY_KPLEFTPAREN, RETROK_LEFTPAREN },
   { KEY_KPRIGHTPAREN, RETROK_RIGHTPAREN },
   { KEY_KPASTERISK, RETROK_ASTERISK },
   { KEY_KPPLUS, RETROK_PLUS },
   { KEY_COMMA, RETROK_COMMA },
   { KEY_MINUS, RETROK_MINUS },
   { KEY_DOT, RETROK_PERIOD },
   { KEY_SLASH, RETROK_SLASH },
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
   /* { KEY_COLON, RETROK_COLON }, */
   { KEY_SEMICOLON, RETROK_SEMICOLON },
   /* { KEY_LESS, RETROK_LESS }, */
   { KEY_EQUAL, RETROK_EQUALS },
   /* { KEY_GREATER, RETROK_GREATER }, */
   { KEY_QUESTION, RETROK_QUESTION },
   /* { KEY_AT, RETROK_AT }, */
   { KEY_LEFTBRACE, RETROK_LEFTBRACKET },
   { KEY_BACKSLASH, RETROK_BACKSLASH },
   { KEY_RIGHTBRACE, RETROK_RIGHTBRACKET },
   /* { ?, RETROK_CARET }, */
   /* { ?, RETROK_UNDERSCORE }, */
   { KEY_GRAVE, RETROK_BACKQUOTE },
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
   { KEY_DELETE, RETROK_DELETE },
   { KEY_KP0, RETROK_KP0 },
   { KEY_KP1, RETROK_KP1 },
   { KEY_KP2, RETROK_KP2 },
   { KEY_KP3, RETROK_KP3 },
   { KEY_KP4, RETROK_KP4 },
   { KEY_KP5, RETROK_KP5 },
   { KEY_KP6, RETROK_KP6 },
   { KEY_KP7, RETROK_KP7 },
   { KEY_KP8, RETROK_KP8 },
   { KEY_KP9, RETROK_KP9 },
   { KEY_KPDOT, RETROK_KP_PERIOD },
   { KEY_KPSLASH, RETROK_KP_DIVIDE },
   { KEY_KPASTERISK, RETROK_KP_MULTIPLY },
   { KEY_KPMINUS, RETROK_KP_MINUS },
   { KEY_KPPLUS, RETROK_KP_PLUS },
   { KEY_KPENTER, RETROK_KP_ENTER },
   { KEY_KPEQUAL, RETROK_KP_EQUALS },
   { KEY_UP, RETROK_UP },
   { KEY_DOWN, RETROK_DOWN },
   { KEY_RIGHT, RETROK_RIGHT },
   { KEY_LEFT, RETROK_LEFT },
   { KEY_INSERT, RETROK_INSERT },
   { KEY_HOME, RETROK_HOME },
   { KEY_END, RETROK_END },
   { KEY_PAGEUP, RETROK_PAGEUP },
   { KEY_PAGEDOWN, RETROK_PAGEDOWN },
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
   { KEY_NUMLOCK, RETROK_NUMLOCK },
   { KEY_CAPSLOCK, RETROK_CAPSLOCK },
   { KEY_SCROLLLOCK, RETROK_SCROLLOCK },
   { KEY_RIGHTSHIFT, RETROK_RSHIFT },
   { KEY_LEFTSHIFT, RETROK_LSHIFT },
   { KEY_RIGHTCTRL, RETROK_RCTRL },
   { KEY_LEFTCTRL, RETROK_LCTRL },
   { KEY_RIGHTALT, RETROK_RALT },
   { KEY_LEFTALT, RETROK_LALT },
   /* { ?, RETROK_RMETA }, */
   /* { ?, RETROK_LMETA }, */
   { KEY_LEFTMETA, RETROK_LSUPER },
   { KEY_RIGHTMETA, RETROK_RSUPER },
   { KEY_MODE, RETROK_MODE },
   { KEY_COMPOSE, RETROK_COMPOSE },
   { KEY_HELP, RETROK_HELP },
   { KEY_PRINT, RETROK_PRINT },
   { KEY_SYSRQ, RETROK_SYSREQ },
   { KEY_BREAK, RETROK_BREAK },
   { KEY_MENU, RETROK_MENU },
   { KEY_POWER, RETROK_POWER },
#ifndef ANDROID
   { KEY_EURO, RETROK_EURO },
#endif
   { KEY_UNDO, RETROK_UNDO },
   { 0, RETROK_UNKNOWN },
};
#endif

#ifdef __APPLE__
const struct rarch_key_map rarch_key_map_apple_hid[] = {
   { KEY_Delete, RETROK_BACKSPACE },
   { KEY_Tab, RETROK_TAB },
   /* { ?, RETROK_CLEAR }, */
   { KEY_Enter, RETROK_RETURN },
   { KEY_Pause, RETROK_PAUSE },
   { KEY_Escape, RETROK_ESCAPE },
   { KEY_Space, RETROK_SPACE },
   /* { ?, RETROK_EXCLAIM }, */
   /* { ?, RETROK_QUOTEDBL }, */
   /* { ?, RETROK_HASH }, */
   /* { ?, RETROK_DOLLAR }, */
   /* { ?, RETROK_AMPERSAND }, */
   { KEY_Quote, RETROK_QUOTE },
   /* { ?, RETROK_LEFTPAREN }, */
   /* { ?, RETROK_RIGHTPAREN }, */
   /* { ?, RETROK_ASTERISK }, */
   /* { ?, RETROK_PLUS }, */
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
   /* { ?, RETROK_COLON }, */
   { KEY_Semicolon, RETROK_SEMICOLON },
   /* { ?, RETROK_LESS }, */
   { KEY_Equals, RETROK_EQUALS },
   /* { ?, RETROK_GREATER }, */
   /* { ?, RETROK_QUESTION }, */
   /* { ?, RETROK_AT }, */
   { KEY_LeftBracket, RETROK_LEFTBRACKET },
   { KEY_Backslash, RETROK_BACKSLASH },
   { KEY_RightBracket, RETROK_RIGHTBRACKET },
   /* { ?, RETROK_CARET }, */
   /* { ?, RETROK_UNDERSCORE }, */
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

   /* { ?, RETROK_NUMLOCK }, */
   { KEY_CapsLock, RETROK_CAPSLOCK },
   /* { ?, RETROK_SCROLLOCK }, */
   { KEY_RightShift, RETROK_RSHIFT },
   { KEY_LeftShift, RETROK_LSHIFT },
   { KEY_RightControl, RETROK_RCTRL },
   { KEY_LeftControl, RETROK_LCTRL },
   { KEY_RightAlt, RETROK_RALT },
   { KEY_LeftAlt, RETROK_LALT },
   { KEY_RightGUI, RETROK_RMETA },
   { KEY_LeftGUI, RETROK_RMETA },
   /* { ?, RETROK_LSUPER }, */
   /* { ?, RETROK_RSUPER }, */
   /* { ?, RETROK_MODE }, */
   /* { ?, RETROK_COMPOSE }, */

   /* { ?, RETROK_HELP }, */
   { KEY_PrintScreen, RETROK_PRINT },
   /* { ?, RETROK_SYSREQ }, */
   /* { ?, RETROK_BREAK }, */
   { KEY_Menu, RETROK_MENU },
   /* { ?, RETROK_POWER }, */
   /* { ?, RETROK_EURO }, */
   /* { ?, RETROK_UNDO }, */
   { 0, RETROK_UNKNOWN }
};
#endif

static enum retro_key rarch_keysym_lut[RETROK_LAST];

/**
 * input_keymaps_init_keyboard_lut:
 * @map                   : Keyboard map.
 *
 * Initializes and sets the keyboard layout to a keyboard map (@map).
 **/
void input_keymaps_init_keyboard_lut(const struct rarch_key_map *map)
{
   memset(rarch_keysym_lut, 0, sizeof(rarch_keysym_lut));

   for (; map->rk != RETROK_UNKNOWN; map++)
      rarch_keysym_lut[map->rk] = (enum retro_key)map->sym;
}

/**
 * input_keymaps_translate_keysym_to_rk:
 * @sym                   : Key symbol.
 *
 * Translates a key symbol from the keyboard layout table
 * to an associated retro key identifier.
 *
 * Returns: Retro key identifier.
 **/
enum retro_key input_keymaps_translate_keysym_to_rk(unsigned sym)
{
   unsigned i;

   for (i = 0; i < ARRAY_SIZE(rarch_keysym_lut); i++)
   {
      if (rarch_keysym_lut[i] != sym)
         continue;

      return (enum retro_key)i;
   }

   return RETROK_UNKNOWN;
}

/**
 * input_keymaps_translate_rk_to_str:
 * @key                   : Retro key identifier.
 * @buf                   : Buffer.
 * @size                  : Size of @buf.
 *
 * Translates a retro key identifier to a human-readable 
 * identifier string.
 **/
void input_keymaps_translate_rk_to_str(enum retro_key key, char *buf, size_t size)
{
   unsigned i;

   rarch_assert(size >= 2);
   *buf = '\0';

   if (key >= RETROK_a && key <= RETROK_z)
   {
      buf[0] = (key - RETROK_a) + 'a';
      buf[1] = '\0';
      return;
   }

   for (i = 0; input_config_key_map[i].str; i++)
   {
      if (input_config_key_map[i].key != key)
         continue;

      strlcpy(buf, input_config_key_map[i].str, size);
      break;
   }
}

/**
 * input_keymaps_translate_rk_to_keysym:
 * @key                   : Retro key identifier
 *
 * Translates a retro key identifier to a key symbol
 * from the keyboard layout table.
 *
 * Returns: key symbol from the keyboard layout table.
 **/
unsigned input_keymaps_translate_rk_to_keysym(enum retro_key key)
{
   return rarch_keysym_lut[key];
}
