/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2020      - neil4 (reverse LUT keyboard)
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

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <compat/strl.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "../config.def.h"

#ifdef ANDROID
#include <android/keycodes.h>
#include "drivers_keyboard/keyboard_event_android.h"
#endif

#ifdef DJGPP
#include "drivers_keyboard/keyboard_event_dos.h"
#endif

#ifdef __QNX__
#include <sys/keycodes.h>
#endif

#ifdef __PSL1GHT__
#include <io/kb.h>
#endif

#if defined(HAVE_SDL) || defined(HAVE_SDL2)
#include "SDL.h"
#endif

#if defined(__linux__) || defined(HAVE_WAYLAND)
#if defined(__linux__)
#include <linux/input.h>
#include <linux/kd.h>
#elif defined(__FreeBSD__)
#include <dev/evdev/input.h>
#endif
#endif

#ifdef HAVE_X11
#include "input/include/xfree86_keycodes.h"
#endif

#ifdef HAVE_DINPUT
#undef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800

#include <dinput.h>
#endif

#include "input_keymaps.h"

#ifdef __APPLE__
#include "drivers_keyboard/keyboard_event_apple.h"
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
   { "oem102", RETROK_OEM_102 },

   { "nul", RETROK_UNKNOWN },
   { NULL, RETROK_UNKNOWN },
};

#ifdef HAVE_LIBNX
const struct rarch_key_map rarch_key_map_switch[] = {
   { HidKeyboardKey_A, RETROK_a },
   { HidKeyboardKey_B, RETROK_b },
   { HidKeyboardKey_C, RETROK_c },
   { HidKeyboardKey_D, RETROK_d },
   { HidKeyboardKey_E, RETROK_e },
   { HidKeyboardKey_F, RETROK_f },
   { HidKeyboardKey_G, RETROK_g },
   { HidKeyboardKey_H, RETROK_h },
   { HidKeyboardKey_I, RETROK_i },
   { HidKeyboardKey_J, RETROK_j },
   { HidKeyboardKey_K, RETROK_k },
   { HidKeyboardKey_L, RETROK_l },
   { HidKeyboardKey_M, RETROK_m },
   { HidKeyboardKey_N, RETROK_n },
   { HidKeyboardKey_O, RETROK_o },
   { HidKeyboardKey_P, RETROK_p },
   { HidKeyboardKey_Q, RETROK_q },
   { HidKeyboardKey_R, RETROK_r },
   { HidKeyboardKey_S, RETROK_s },
   { HidKeyboardKey_T, RETROK_t },
   { HidKeyboardKey_U, RETROK_u },
   { HidKeyboardKey_V, RETROK_v },
   { HidKeyboardKey_W, RETROK_w },
   { HidKeyboardKey_X, RETROK_x },
   { HidKeyboardKey_Y, RETROK_y },
   { HidKeyboardKey_Z, RETROK_z },
   { HidKeyboardKey_Backspace, RETROK_BACKSPACE },
   { HidKeyboardKey_Tab, RETROK_TAB },
   { HidKeyboardKey_Return, RETROK_RETURN },
   { HidKeyboardKey_Pause, RETROK_PAUSE },
   { HidKeyboardKey_Escape, RETROK_ESCAPE },
   { HidKeyboardKey_Space, RETROK_SPACE },
   { HidKeyboardKey_Tilde, RETROK_HASH },
   { HidKeyboardKey_Quote, RETROK_QUOTE },
   { HidKeyboardKey_Comma, RETROK_COMMA },
   { HidKeyboardKey_Minus, RETROK_MINUS },
   { HidKeyboardKey_Period, RETROK_PERIOD },
   { HidKeyboardKey_Slash, RETROK_SLASH },
   { HidKeyboardKey_D0, RETROK_0 },
   { HidKeyboardKey_D1, RETROK_1 },
   { HidKeyboardKey_D2, RETROK_2 },
   { HidKeyboardKey_D3, RETROK_3 },
   { HidKeyboardKey_D4, RETROK_4 },
   { HidKeyboardKey_D5, RETROK_5 },
   { HidKeyboardKey_D6, RETROK_6 },
   { HidKeyboardKey_D7, RETROK_7 },
   { HidKeyboardKey_D8, RETROK_8 },
   { HidKeyboardKey_D9, RETROK_9 },
   { HidKeyboardKey_Semicolon, RETROK_SEMICOLON },
   { HidKeyboardKey_Plus, RETROK_EQUALS },
   { HidKeyboardKey_OpenBracket, RETROK_LEFTBRACKET },
   { HidKeyboardKey_Pipe, RETROK_BACKSLASH },
   { HidKeyboardKey_CloseBracket, RETROK_RIGHTBRACKET },
   { HidKeyboardKey_Delete, RETROK_DELETE },
   { HidKeyboardKey_NumPad0, RETROK_KP0 },
   { HidKeyboardKey_NumPad1, RETROK_KP1 },
   { HidKeyboardKey_NumPad2, RETROK_KP2 },
   { HidKeyboardKey_NumPad3, RETROK_KP3 },
   { HidKeyboardKey_NumPad4, RETROK_KP4 },
   { HidKeyboardKey_NumPad5, RETROK_KP5 },
   { HidKeyboardKey_NumPad6, RETROK_KP6 },
   { HidKeyboardKey_NumPad7, RETROK_KP7 },
   { HidKeyboardKey_NumPad8, RETROK_KP8 },
   { HidKeyboardKey_NumPad9, RETROK_KP9 },
   { HidKeyboardKey_NumPadDot, RETROK_KP_PERIOD },
   { HidKeyboardKey_NumPadDivide, RETROK_KP_DIVIDE },
   { HidKeyboardKey_NumPadMultiply, RETROK_KP_MULTIPLY },
   { HidKeyboardKey_NumPadSubtract, RETROK_KP_MINUS },
   { HidKeyboardKey_NumPadAdd, RETROK_KP_PLUS },
   { HidKeyboardKey_NumPadEnter, RETROK_KP_ENTER },
   { HidKeyboardKey_NumPadEquals, RETROK_KP_EQUALS },
   { HidKeyboardKey_UpArrow, RETROK_UP },
   { HidKeyboardKey_DownArrow, RETROK_DOWN },
   { HidKeyboardKey_RightArrow, RETROK_RIGHT },
   { HidKeyboardKey_LeftArrow, RETROK_LEFT },
   { HidKeyboardKey_Insert, RETROK_INSERT },
   { HidKeyboardKey_Home, RETROK_HOME },
   { HidKeyboardKey_End, RETROK_END },
   { HidKeyboardKey_PageUp, RETROK_PAGEUP },
   { HidKeyboardKey_PageDown, RETROK_PAGEDOWN },
   { HidKeyboardKey_F1, RETROK_F1 },
   { HidKeyboardKey_F2, RETROK_F2 },
   { HidKeyboardKey_F3, RETROK_F3 },
   { HidKeyboardKey_F4, RETROK_F4 },
   { HidKeyboardKey_F5, RETROK_F5 },
   { HidKeyboardKey_F6, RETROK_F6 },
   { HidKeyboardKey_F7, RETROK_F7 },
   { HidKeyboardKey_F8, RETROK_F8 },
   { HidKeyboardKey_F9, RETROK_F9 },
   { HidKeyboardKey_F10, RETROK_F10 },
   { HidKeyboardKey_F11, RETROK_F11 },
   { HidKeyboardKey_F12, RETROK_F12 },
   { HidKeyboardKey_F13, RETROK_F13 },
   { HidKeyboardKey_F14, RETROK_F14 },
   { HidKeyboardKey_F15, RETROK_F15 },
   { HidKeyboardKey_NumLock, RETROK_NUMLOCK },
   { HidKeyboardKey_CapsLock, RETROK_CAPSLOCK },
   { HidKeyboardKey_ScrollLock, RETROK_SCROLLOCK },
   { HidKeyboardKey_RightShift, RETROK_RSHIFT },
   { HidKeyboardKey_LeftShift, RETROK_LSHIFT },
   { HidKeyboardKey_RightControl, RETROK_RCTRL },
   { HidKeyboardKey_LeftControl, RETROK_LCTRL },
   { HidKeyboardKey_RightAlt, RETROK_RALT },
   { HidKeyboardKey_LeftAlt, RETROK_LALT },
   { HidKeyboardKey_LeftGui, RETROK_LMETA },
   { HidKeyboardKey_RightGui, RETROK_RMETA },
   { HidKeyboardKey_Application, RETROK_COMPOSE },
   { HidKeyboardKey_Pause, RETROK_BREAK },
   { HidKeyboardKey_Power, RETROK_POWER },
   { 0, RETROK_UNKNOWN }
};
#endif

#ifdef VITA
/* Vita scancodes are identical to USB 2.0 standard, e.g. SDL2 */
const struct rarch_key_map rarch_key_map_vita[] = {
   { 0x02A, RETROK_BACKSPACE },
   { 0x02B, RETROK_TAB },
   { 0x09C, RETROK_CLEAR },
   { 0x028, RETROK_RETURN },
   { 0x048, RETROK_PAUSE },
   { 0x029, RETROK_ESCAPE },
   { 0x02C, RETROK_SPACE },
   /*{ ?, RETROK_EXCLAIM },*/
   /*{ ?, RETROK_QUOTEDBL },*/
   /*{ ?, RETROK_HASH },*/
   /*{ ?, RETROK_DOLLAR },*/
   /*{ ?, RETROK_AMPERSAND },*/
   { 0x034, RETROK_QUOTE },
   /*{ ?, RETROK_LEFTPAREN },*/
   /*{ ?, RETROK_RIGHTPAREN },*/
   /*{ ?, RETROK_ASTERISK },*/
   /*{ ?, RETROK_PLUS },*/
   { 0x036, RETROK_COMMA },
   { 0x02D, RETROK_MINUS },
   { 0x037, RETROK_PERIOD },
   { 0x038, RETROK_SLASH },
   { 0x027, RETROK_0 },
   { 0x01E, RETROK_1 },
   { 0x01F, RETROK_2 },
   { 0x020, RETROK_3 },
   { 0x021, RETROK_4 },
   { 0x022, RETROK_5 },
   { 0x023, RETROK_6 },
   { 0x024, RETROK_7 },
   { 0x025, RETROK_8 },
   { 0x026, RETROK_9 },
   /*{ ?, RETROK_COLON },*/
   { 0x033, RETROK_SEMICOLON },
   /*{ ?, RETROK_OEM_102 },*/
   { 0x02E, RETROK_EQUALS },
   /*{ ?, RETROK_GREATER },*/
   /*{ ?, RETROK_QUESTION },*/
   /*{ ?, RETROK_AT },*/
   { 0x02F, RETROK_LEFTBRACKET },
   { 0x031, RETROK_BACKSLASH },
   { 0x030, RETROK_RIGHTBRACKET },
   /*{ ?, RETROK_CARET },*/
   /*{ ?, RETROK_UNDERSCORE },*/
   { 0x035, RETROK_BACKQUOTE },
   { 0x004, RETROK_a },
   { 0x005, RETROK_b },
   { 0x006, RETROK_c },
   { 0x007, RETROK_d },
   { 0x008, RETROK_e },
   { 0x009, RETROK_f },
   { 0x00A, RETROK_g },
   { 0x00B, RETROK_h },
   { 0x00C, RETROK_i },
   { 0x00D, RETROK_j },
   { 0x00E, RETROK_k },
   { 0x00F, RETROK_l },
   { 0x010, RETROK_m },
   { 0x011, RETROK_n },
   { 0x012, RETROK_o },
   { 0x013, RETROK_p },
   { 0x014, RETROK_q },
   { 0x015, RETROK_r },
   { 0x016, RETROK_s },
   { 0x017, RETROK_t },
   { 0x018, RETROK_u },
   { 0x019, RETROK_v },
   { 0x01A, RETROK_w },
   { 0x01B, RETROK_x },
   { 0x01C, RETROK_y },
   { 0x01D, RETROK_z },
   { 0x04C, RETROK_DELETE },
   { 0x062, RETROK_KP0 },
   { 0x059, RETROK_KP1 },
   { 0x05A, RETROK_KP2 },
   { 0x05B, RETROK_KP3 },
   { 0x05C, RETROK_KP4 },
   { 0x05D, RETROK_KP5 },
   { 0x05E, RETROK_KP6 },
   { 0x05F, RETROK_KP7 },
   { 0x060, RETROK_KP8 },
   { 0x061, RETROK_KP9 },
   { 0x063, RETROK_KP_PERIOD },
   { 0x054, RETROK_KP_DIVIDE },
   { 0x055, RETROK_KP_MULTIPLY },
   { 0x056, RETROK_KP_MINUS },
   { 0x057, RETROK_KP_PLUS },
   { 0x058, RETROK_KP_ENTER },
   { 0x067, RETROK_KP_EQUALS },
   { 0x052, RETROK_UP },
   { 0x051, RETROK_DOWN },
   { 0x04F, RETROK_RIGHT },
   { 0x050, RETROK_LEFT },
   { 0x049, RETROK_INSERT },
   { 0x04A, RETROK_HOME },
   { 0x04D, RETROK_END },
   { 0x04B, RETROK_PAGEUP },
   { 0x04E, RETROK_PAGEDOWN },
   { 0x03A, RETROK_F1 },
   { 0x03B, RETROK_F2 },
   { 0x03C, RETROK_F3 },
   { 0x03D, RETROK_F4 },
   { 0x03E, RETROK_F5 },
   { 0x03F, RETROK_F6 },
   { 0x040, RETROK_F7 },
   { 0x041, RETROK_F8 },
   { 0x042, RETROK_F9 },
   { 0x043, RETROK_F10 },
   { 0x044, RETROK_F11 },
   { 0x045, RETROK_F12 },
   { 0x068, RETROK_F13 },
   { 0x069, RETROK_F14 },
   { 0x06A, RETROK_F15 },
   { 0x053, RETROK_NUMLOCK },
   { 0x039, RETROK_CAPSLOCK },
   { 0x047, RETROK_SCROLLOCK },
   { 0x0E5, RETROK_RSHIFT },
   { 0x0E1, RETROK_LSHIFT },
   { 0x0E4, RETROK_RCTRL },
   { 0x0E0, RETROK_LCTRL },
   { 0x0E6, RETROK_RALT },
   { 0x0E2, RETROK_LALT },
   /* { ?, RETROK_RMETA }, */
   /* { ?, RETROK_LMETA }, */
   { 0x0E3, RETROK_LSUPER },
   { 0x0E7, RETROK_RSUPER },
   /* { ?, RETROK_MODE },*/
   { 0x075, RETROK_HELP },
   { 0x046, RETROK_PRINT },
   { 0x09A, RETROK_SYSREQ },
   { 0x048, RETROK_BREAK },
   { 0x076, RETROK_MENU },
   { 0x066, RETROK_POWER },
   /*{ ?, RETROK_EURO },*/
   { 0x07A, RETROK_UNDO },
   { 0, RETROK_UNKNOWN },
};
#endif

#if defined(HAVE_SDL) || defined(HAVE_SDL2)
const struct rarch_key_map rarch_key_map_sdl[] = {
   { SDLK_BACKSPACE, RETROK_BACKSPACE },
   { SDLK_TAB, RETROK_TAB },
   { SDLK_CLEAR, RETROK_CLEAR },
   { SDLK_RETURN, RETROK_RETURN },
   { SDLK_PAUSE, RETROK_PAUSE },
   { SDLK_ESCAPE, RETROK_ESCAPE },
   { SDLK_SPACE, RETROK_SPACE },
   { SDLK_EXCLAIM, RETROK_EXCLAIM },
   { SDLK_QUOTEDBL, RETROK_QUOTEDBL },
   { SDLK_HASH, RETROK_HASH },
   { SDLK_DOLLAR, RETROK_DOLLAR },
   { SDLK_AMPERSAND, RETROK_AMPERSAND },
   { SDLK_QUOTE, RETROK_QUOTE },
   { SDLK_LEFTPAREN, RETROK_LEFTPAREN },
   { SDLK_RIGHTPAREN, RETROK_RIGHTPAREN },
   { SDLK_ASTERISK, RETROK_ASTERISK },
   { SDLK_PLUS, RETROK_PLUS },
   { SDLK_COMMA, RETROK_COMMA },
   { SDLK_MINUS, RETROK_MINUS },
   { SDLK_PERIOD, RETROK_PERIOD },
   { SDLK_SLASH, RETROK_SLASH },
   { SDLK_0, RETROK_0 },
   { SDLK_1, RETROK_1 },
   { SDLK_2, RETROK_2 },
   { SDLK_3, RETROK_3 },
   { SDLK_4, RETROK_4 },
   { SDLK_5, RETROK_5 },
   { SDLK_6, RETROK_6 },
   { SDLK_7, RETROK_7 },
   { SDLK_8, RETROK_8 },
   { SDLK_9, RETROK_9 },
   { SDLK_COLON, RETROK_COLON },
   { SDLK_SEMICOLON, RETROK_SEMICOLON },
   { SDLK_LESS, RETROK_OEM_102 },
   { SDLK_EQUALS, RETROK_EQUALS },
   { SDLK_GREATER, RETROK_GREATER },
   { SDLK_QUESTION, RETROK_QUESTION },
   { SDLK_AT, RETROK_AT },
   { SDLK_LEFTBRACKET, RETROK_LEFTBRACKET },
   { SDLK_BACKSLASH, RETROK_BACKSLASH },
   { SDLK_RIGHTBRACKET, RETROK_RIGHTBRACKET },
   { SDLK_CARET, RETROK_CARET },
   { SDLK_UNDERSCORE, RETROK_UNDERSCORE },
   { SDLK_BACKQUOTE, RETROK_BACKQUOTE },
   { SDLK_a, RETROK_a },
   { SDLK_b, RETROK_b },
   { SDLK_c, RETROK_c },
   { SDLK_d, RETROK_d },
   { SDLK_e, RETROK_e },
   { SDLK_f, RETROK_f },
   { SDLK_g, RETROK_g },
   { SDLK_h, RETROK_h },
   { SDLK_i, RETROK_i },
   { SDLK_j, RETROK_j },
   { SDLK_k, RETROK_k },
   { SDLK_l, RETROK_l },
   { SDLK_m, RETROK_m },
   { SDLK_n, RETROK_n },
   { SDLK_o, RETROK_o },
   { SDLK_p, RETROK_p },
   { SDLK_q, RETROK_q },
   { SDLK_r, RETROK_r },
   { SDLK_s, RETROK_s },
   { SDLK_t, RETROK_t },
   { SDLK_u, RETROK_u },
   { SDLK_v, RETROK_v },
   { SDLK_w, RETROK_w },
   { SDLK_x, RETROK_x },
   { SDLK_y, RETROK_y },
   { SDLK_z, RETROK_z },
   { SDLK_DELETE, RETROK_DELETE },
#ifdef HAVE_SDL2
   { SDLK_KP_0, RETROK_KP0 },
   { SDLK_KP_1, RETROK_KP1 },
   { SDLK_KP_2, RETROK_KP2 },
   { SDLK_KP_3, RETROK_KP3 },
   { SDLK_KP_4, RETROK_KP4 },
   { SDLK_KP_5, RETROK_KP5 },
   { SDLK_KP_6, RETROK_KP6 },
   { SDLK_KP_7, RETROK_KP7 },
   { SDLK_KP_8, RETROK_KP8 },
   { SDLK_KP_9, RETROK_KP9 },
#else
   { SDLK_KP0, RETROK_KP0 },
   { SDLK_KP1, RETROK_KP1 },
   { SDLK_KP2, RETROK_KP2 },
   { SDLK_KP3, RETROK_KP3 },
   { SDLK_KP4, RETROK_KP4 },
   { SDLK_KP5, RETROK_KP5 },
   { SDLK_KP6, RETROK_KP6 },
   { SDLK_KP7, RETROK_KP7 },
   { SDLK_KP8, RETROK_KP8 },
   { SDLK_KP9, RETROK_KP9 },
#endif
   { SDLK_KP_PERIOD, RETROK_KP_PERIOD },
   { SDLK_KP_DIVIDE, RETROK_KP_DIVIDE },
   { SDLK_KP_MULTIPLY, RETROK_KP_MULTIPLY },
   { SDLK_KP_MINUS, RETROK_KP_MINUS },
   { SDLK_KP_PLUS, RETROK_KP_PLUS },
   { SDLK_KP_ENTER, RETROK_KP_ENTER },
   { SDLK_KP_EQUALS, RETROK_KP_EQUALS },
   { SDLK_UP, RETROK_UP },
   { SDLK_DOWN, RETROK_DOWN },
   { SDLK_RIGHT, RETROK_RIGHT },
   { SDLK_LEFT, RETROK_LEFT },
   { SDLK_INSERT, RETROK_INSERT },
   { SDLK_HOME, RETROK_HOME },
   { SDLK_END, RETROK_END },
   { SDLK_PAGEUP, RETROK_PAGEUP },
   { SDLK_PAGEDOWN, RETROK_PAGEDOWN },
   { SDLK_F1, RETROK_F1 },
   { SDLK_F2, RETROK_F2 },
   { SDLK_F3, RETROK_F3 },
   { SDLK_F4, RETROK_F4 },
   { SDLK_F5, RETROK_F5 },
   { SDLK_F6, RETROK_F6 },
   { SDLK_F7, RETROK_F7 },
   { SDLK_F8, RETROK_F8 },
   { SDLK_F9, RETROK_F9 },
   { SDLK_F10, RETROK_F10 },
   { SDLK_F11, RETROK_F11 },
   { SDLK_F12, RETROK_F12 },
   { SDLK_F13, RETROK_F13 },
   { SDLK_F14, RETROK_F14 },
   { SDLK_F15, RETROK_F15 },
#ifdef HAVE_SDL2
   { SDLK_NUMLOCKCLEAR, RETROK_NUMLOCK },
#else
   { SDLK_NUMLOCK, RETROK_NUMLOCK },
#endif
   { SDLK_CAPSLOCK, RETROK_CAPSLOCK },
#ifdef HAVE_SDL2
   { SDLK_SCROLLLOCK, RETROK_SCROLLOCK },
#else
   { SDLK_SCROLLOCK, RETROK_SCROLLOCK },
#endif
   { SDLK_RSHIFT, RETROK_RSHIFT },
   { SDLK_LSHIFT, RETROK_LSHIFT },
   { SDLK_RCTRL, RETROK_RCTRL },
   { SDLK_LCTRL, RETROK_LCTRL },
   { SDLK_RALT, RETROK_RALT },
   { SDLK_LALT, RETROK_LALT },
#ifdef HAVE_SDL2
   /* { ?, RETROK_RMETA }, */
   /* { ?, RETROK_LMETA }, */
   { SDLK_LGUI, RETROK_LSUPER },
   { SDLK_RGUI, RETROK_RSUPER },
#else
   { SDLK_RMETA, RETROK_RMETA },
   { SDLK_LMETA, RETROK_LMETA },
   { SDLK_LSUPER, RETROK_LSUPER },
   { SDLK_RSUPER, RETROK_RSUPER },
#endif
   { SDLK_MODE, RETROK_MODE },
#ifndef HAVE_SDL2
   { SDLK_COMPOSE, RETROK_COMPOSE },
#endif
   { SDLK_HELP, RETROK_HELP },
#ifdef HAVE_SDL2
   { SDLK_PRINTSCREEN, RETROK_PRINT },
#else
   { SDLK_PRINT, RETROK_PRINT },
#endif
   { SDLK_SYSREQ, RETROK_SYSREQ },
   { SDLK_PAUSE, RETROK_BREAK },
   { SDLK_MENU, RETROK_MENU },
   { SDLK_POWER, RETROK_POWER },

#ifndef HAVE_SDL2
   { SDLK_EURO, RETROK_EURO },
#endif
   { SDLK_UNDO, RETROK_UNDO },

   { 0, RETROK_UNKNOWN },
};
#endif

#ifdef HAVE_DINPUT
const struct rarch_key_map rarch_key_map_dinput[] = {
   { DIK_LEFT, RETROK_LEFT },
   { DIK_RIGHT, RETROK_RIGHT },
   { DIK_UP, RETROK_UP },
   { DIK_DOWN, RETROK_DOWN },
   { DIK_RETURN, RETROK_RETURN },
   { DIK_TAB, RETROK_TAB },
   { DIK_INSERT, RETROK_INSERT },
   { DIK_DELETE, RETROK_DELETE },
   { DIK_RSHIFT, RETROK_RSHIFT },
   { DIK_LSHIFT, RETROK_LSHIFT },
   { DIK_RCONTROL, RETROK_RCTRL },
   { DIK_LCONTROL, RETROK_LCTRL },
   { DIK_RMENU, RETROK_RALT },
   { DIK_LALT, RETROK_LALT },
   { DIK_LWIN, RETROK_LSUPER },
   { DIK_RWIN, RETROK_RSUPER },
   { DIK_APPS, RETROK_MENU },
   { DIK_END, RETROK_END },
   { DIK_HOME, RETROK_HOME },
   { DIK_NEXT, RETROK_PAGEDOWN },
   { DIK_PRIOR, RETROK_PAGEUP },
   { DIK_SPACE, RETROK_SPACE },
   { DIK_ESCAPE, RETROK_ESCAPE },
   { DIK_BACKSPACE, RETROK_BACKSPACE },
   { DIK_NUMPADENTER, RETROK_KP_ENTER },
   { DIK_NUMPADPLUS, RETROK_KP_PLUS },
   { DIK_NUMPADMINUS, RETROK_KP_MINUS },
   { DIK_NUMPADSTAR, RETROK_KP_MULTIPLY },
   { DIK_DIVIDE, RETROK_KP_DIVIDE },
   { DIK_GRAVE, RETROK_BACKQUOTE },
   { DIK_PAUSE, RETROK_PAUSE },
   { DIK_NUMPAD0, RETROK_KP0 },
   { DIK_NUMPAD1, RETROK_KP1 },
   { DIK_NUMPAD2, RETROK_KP2 },
   { DIK_NUMPAD3, RETROK_KP3 },
   { DIK_NUMPAD4, RETROK_KP4 },
   { DIK_NUMPAD5, RETROK_KP5 },
   { DIK_NUMPAD6, RETROK_KP6 },
   { DIK_NUMPAD7, RETROK_KP7 },
   { DIK_NUMPAD8, RETROK_KP8 },
   { DIK_NUMPAD9, RETROK_KP9 },
   { DIK_0, RETROK_0 },
   { DIK_1, RETROK_1 },
   { DIK_2, RETROK_2 },
   { DIK_3, RETROK_3 },
   { DIK_4, RETROK_4 },
   { DIK_5, RETROK_5 },
   { DIK_6, RETROK_6 },
   { DIK_7, RETROK_7 },
   { DIK_8, RETROK_8 },
   { DIK_9, RETROK_9 },
   { DIK_F1, RETROK_F1 },
   { DIK_F2, RETROK_F2 },
   { DIK_F3, RETROK_F3 },
   { DIK_F4, RETROK_F4 },
   { DIK_F5, RETROK_F5 },
   { DIK_F6, RETROK_F6 },
   { DIK_F7, RETROK_F7 },
   { DIK_F8, RETROK_F8 },
   { DIK_F9, RETROK_F9 },
   { DIK_F10, RETROK_F10 },
   { DIK_F11, RETROK_F11 },
   { DIK_F12, RETROK_F12 },
   { DIK_F13, RETROK_F13 },
   { DIK_F14, RETROK_F14 },
   { DIK_F15, RETROK_F15 },
   { DIK_A, RETROK_a },
   { DIK_B, RETROK_b },
   { DIK_C, RETROK_c },
   { DIK_D, RETROK_d },
   { DIK_E, RETROK_e },
   { DIK_F, RETROK_f },
   { DIK_G, RETROK_g },
   { DIK_H, RETROK_h },
   { DIK_I, RETROK_i },
   { DIK_J, RETROK_j },
   { DIK_K, RETROK_k },
   { DIK_L, RETROK_l },
   { DIK_M, RETROK_m },
   { DIK_N, RETROK_n },
   { DIK_O, RETROK_o },
   { DIK_P, RETROK_p },
   { DIK_Q, RETROK_q },
   { DIK_R, RETROK_r },
   { DIK_S, RETROK_s },
   { DIK_T, RETROK_t },
   { DIK_U, RETROK_u },
   { DIK_V, RETROK_v },
   { DIK_W, RETROK_w },
   { DIK_X, RETROK_x },
   { DIK_Y, RETROK_y },
   { DIK_Z, RETROK_z },
   { DIK_APOSTROPHE, RETROK_QUOTE },
   { DIK_COMMA, RETROK_COMMA },
   { DIK_MINUS, RETROK_MINUS },
   { DIK_SLASH, RETROK_SLASH },
   { DIK_SEMICOLON, RETROK_SEMICOLON },
   { DIK_EQUALS, RETROK_EQUALS },
   { DIK_LBRACKET, RETROK_LEFTBRACKET },
   { DIK_BACKSLASH, RETROK_BACKSLASH },
   { DIK_RBRACKET, RETROK_RIGHTBRACKET },
   { DIK_DECIMAL, RETROK_KP_PERIOD },
   { DIK_PERIOD, RETROK_PERIOD },
   { DIK_SCROLL, RETROK_SCROLLOCK },
   { DIK_CAPSLOCK, RETROK_CAPSLOCK },
   { DIK_NUMLOCK, RETROK_NUMLOCK },
   { DIK_OEM_102, RETROK_OEM_102 },
   { 0, RETROK_UNKNOWN },
};
#endif

#ifdef EMSCRIPTEN
/* this is generated at runtime, so it isn't constant */
struct rarch_key_map rarch_key_map_rwebinput[RARCH_KEY_MAP_RWEBINPUT_SIZE];
#endif

#ifdef WIIU
const struct rarch_key_map rarch_key_map_wiiu[] = {
   { 4, RETROK_a },
   { 5, RETROK_b },
   { 6, RETROK_c },
   { 7, RETROK_d },
   { 8, RETROK_e },
   { 9, RETROK_f },
   { 10, RETROK_g },
   { 11, RETROK_h },
   { 12, RETROK_i },
   { 13, RETROK_j },
   { 14, RETROK_k },
   { 15, RETROK_l },
   { 16, RETROK_m },
   { 17, RETROK_n },
   { 18, RETROK_o },
   { 19, RETROK_p },
   { 20, RETROK_q },
   { 21, RETROK_r },
   { 22, RETROK_s },
   { 23, RETROK_t },
   { 24, RETROK_u },
   { 25, RETROK_v },
   { 26, RETROK_w },
   { 27, RETROK_x },
   { 28, RETROK_y },
   { 29, RETROK_z },
   { 30, RETROK_1 },
   { 31, RETROK_2 },
   { 32, RETROK_3 },
   { 33, RETROK_4 },
   { 34, RETROK_5 },
   { 35, RETROK_6 },
   { 36, RETROK_7 },
   { 37, RETROK_8 },
   { 38, RETROK_9 },
   { 39, RETROK_0 },
   { 40, RETROK_RETURN },
   { 41, RETROK_ESCAPE },
   { 42, RETROK_BACKSPACE },
   { 43, RETROK_TAB },
   { 44, RETROK_SPACE },
   { 45, RETROK_MINUS },
   { 46, RETROK_EQUALS },
   { 47, RETROK_LEFTBRACKET },
   { 48, RETROK_RIGHTBRACKET },
   { 49, RETROK_BACKSLASH },
   { 51, RETROK_SEMICOLON },
   { 52, RETROK_QUOTE },
   { 53, RETROK_BACKQUOTE },
   { 54, RETROK_COMMA },
   { 55, RETROK_PERIOD },
   { 56, RETROK_SLASH },
   { 57, RETROK_CAPSLOCK },
   { 58, RETROK_F1 },
   { 59, RETROK_F2 },
   { 60, RETROK_F3 },
   { 61, RETROK_F4 },
   { 62, RETROK_F5 },
   { 63, RETROK_F6 },
   { 64, RETROK_F7 },
   { 65, RETROK_F8 },
   { 66, RETROK_F9 },
   { 67, RETROK_F10 },
   { 68, RETROK_F11 },
   { 69, RETROK_F12 },
   { 71, RETROK_SCROLLOCK },
   { 72, RETROK_PAUSE },
   { 73, RETROK_INSERT },
   { 74, RETROK_HOME },
   { 75, RETROK_PAGEUP },
   { 76, RETROK_DELETE },
   { 77, RETROK_END },
   { 78, RETROK_PAGEDOWN },
   { 79, RETROK_RIGHT },
   { 80, RETROK_LEFT },
   { 81, RETROK_DOWN },
   { 82, RETROK_UP },
   { 83, RETROK_NUMLOCK },
   { 84, RETROK_KP_DIVIDE },
   { 85, RETROK_KP_MULTIPLY },
   { 86, RETROK_KP_MINUS },
   { 87, RETROK_KP_PLUS },
   { 88, RETROK_KP_ENTER },
   { 89, RETROK_KP1 },
   { 90, RETROK_KP2 },
   { 91, RETROK_KP3 },
   { 92, RETROK_KP4 },
   { 93, RETROK_KP5 },
   { 94, RETROK_KP6 },
   { 95, RETROK_KP7 },
   { 96, RETROK_KP8 },
   { 97, RETROK_KP9 },
   { 98, RETROK_KP0 },
   { 99, RETROK_KP_PERIOD },
   { 224, RETROK_LCTRL },
   { 225, RETROK_LSHIFT },
   { 226, RETROK_LALT },
   { 228, RETROK_RCTRL },
   { 229, RETROK_RSHIFT },
   { 230, RETROK_RALT },
   { 0, RETROK_UNKNOWN },
};
#endif

#ifdef HAVE_X11

const struct rarch_key_map rarch_key_map_x11[] = {

   { XFVK_ESC, RETROK_ESCAPE },
   { XFVK_FK01, RETROK_F1 },
   { XFVK_FK02, RETROK_F2 },
   { XFVK_FK03, RETROK_F3 },
   { XFVK_FK04, RETROK_F4 },
   { XFVK_FK05, RETROK_F5 },
   { XFVK_FK06, RETROK_F6 },
   { XFVK_FK07, RETROK_F7 },
   { XFVK_FK08, RETROK_F8 },
   { XFVK_FK09, RETROK_F9 },
   { XFVK_FK10, RETROK_F10 },
   { XFVK_FK11, RETROK_F11 },
   { XFVK_FK12, RETROK_F12 },

   { XFVK_TLDE, RETROK_BACKQUOTE },
   { XFVK_AE01, RETROK_1 },
   { XFVK_AE02, RETROK_2 },
   { XFVK_AE03, RETROK_3 },
   { XFVK_AE04, RETROK_4 },
   { XFVK_AE05, RETROK_5 },
   { XFVK_AE06, RETROK_6 },
   { XFVK_AE07, RETROK_7 },
   { XFVK_AE08, RETROK_8 },
   { XFVK_AE09, RETROK_9 },
   { XFVK_AE10, RETROK_0 },
   { XFVK_AE11, RETROK_MINUS },
   { XFVK_AE12, RETROK_EQUALS },
   { XFVK_BKSP, RETROK_BACKSPACE },

   { XFVK_TAB, RETROK_TAB },
   { XFVK_AD01, RETROK_q },
   { XFVK_AD02, RETROK_w },
   { XFVK_AD03, RETROK_e },
   { XFVK_AD04, RETROK_r },
   { XFVK_AD05, RETROK_t },
   { XFVK_AD06, RETROK_y },
   { XFVK_AD07, RETROK_u },
   { XFVK_AD08, RETROK_i },
   { XFVK_AD09, RETROK_o },
   { XFVK_AD10, RETROK_p },
   { XFVK_AD11, RETROK_LEFTBRACKET },
   { XFVK_AD12, RETROK_RIGHTBRACKET },
   { XFVK_RTRN, RETROK_RETURN },

   { XFVK_CAPS, RETROK_CAPSLOCK },
   { XFVK_AC01, RETROK_a },
   { XFVK_AC02, RETROK_s },
   { XFVK_AC03, RETROK_d },
   { XFVK_AC04, RETROK_f },
   { XFVK_AC05, RETROK_g },
   { XFVK_AC06, RETROK_h },
   { XFVK_AC07, RETROK_j },
   { XFVK_AC08, RETROK_k },
   { XFVK_AC09, RETROK_l },
   { XFVK_AC10, RETROK_SEMICOLON },
   { XFVK_AC11, RETROK_QUOTE },
   { XFVK_AC12, RETROK_BACKSLASH },

   { XFVK_LFSH, RETROK_LSHIFT },
   { XFVK_AB01, RETROK_z },
   { XFVK_AB02, RETROK_x },
   { XFVK_AB03, RETROK_c },
   { XFVK_AB04, RETROK_v },
   { XFVK_AB05, RETROK_b },
   { XFVK_AB06, RETROK_n },
   { XFVK_AB07, RETROK_m },
   { XFVK_AB08, RETROK_COMMA },
   { XFVK_AB09, RETROK_PERIOD },
   { XFVK_AB10, RETROK_SLASH },
   { XFVK_RTSH, RETROK_RSHIFT },

   { XFVK_LALT, RETROK_LALT },
   { XFVK_LCTL, RETROK_LCTRL },
   { XFVK_SPCE, RETROK_SPACE },
   { XFVK_RCTL, RETROK_RCTRL },
   { XFVK_RALT, RETROK_RALT },

   { XFVK_LSGT, RETROK_OEM_102 },
   { XFVK_MENU, RETROK_MENU },
   { XFVK_LWIN, RETROK_LSUPER },
   { XFVK_RWIN, RETROK_RSUPER },
   { XFVK_CALC, RETROK_HELP },

   { XFVK_PRSC, RETROK_PRINT },
   { XFVK_SCLK, RETROK_SCROLLOCK },
   { XFVK_PAUS, RETROK_PAUSE },
   { XFVK_INS, RETROK_INSERT },
   { XFVK_HOME, RETROK_HOME },
   { XFVK_PGUP, RETROK_PAGEUP },
   { XFVK_DELE, RETROK_DELETE },
   { XFVK_END, RETROK_END },
   { XFVK_PGDN, RETROK_PAGEDOWN },
   { XFVK_UP, RETROK_UP },
   { XFVK_LEFT, RETROK_LEFT },
   { XFVK_DOWN, RETROK_DOWN },
   { XFVK_RGHT, RETROK_RIGHT },

   { XFVK_NMLK, RETROK_NUMLOCK },
   { XFVK_KPDV, RETROK_KP_DIVIDE },
   { XFVK_KPMU, RETROK_KP_MULTIPLY },
   { XFVK_KPSU, RETROK_KP_MINUS },
   { XFVK_KP7, RETROK_KP7 },
   { XFVK_KP8, RETROK_KP8 },
   { XFVK_KP9, RETROK_KP9 },
   { XFVK_KPAD, RETROK_KP_PLUS },
   { XFVK_KP4, RETROK_KP4 },
   { XFVK_KP5, RETROK_KP5 },
   { XFVK_KP6, RETROK_KP6 },
   { XFVK_KP1, RETROK_KP1 },
   { XFVK_KP2, RETROK_KP2 },
   { XFVK_KP3, RETROK_KP3 },
   { XFVK_KPEN, RETROK_KP_ENTER },
   { XFVK_KP0, RETROK_KP0 },
   { XFVK_KPDL, RETROK_KP_PERIOD },
   { XFVK_KPEQ, RETROK_KP_EQUALS },

   { 0, RETROK_UNKNOWN },
};
#endif

#if defined(__linux__) || defined(HAVE_WAYLAND)
/* Note: Only one input can be mapped to each
 * RETROK_* key. If several physical inputs
 * correspond to the same key, these inputs
 * must be merged at the input driver level */
const struct rarch_key_map rarch_key_map_linux[] = {
   { KEY_BACKSPACE, RETROK_BACKSPACE },
   { KEY_TAB, RETROK_TAB },
   { KEY_CLEAR, RETROK_CLEAR },
   /* { KEY_EXIT, RETROK_CLEAR }, */     /* Duplicate - Skip */
   { KEY_ENTER, RETROK_RETURN },
   /* { KEY_OK, RETROK_RETURN }, */      /* Duplicate - Skip */
   /* { KEY_SELECT, RETROK_RETURN }, */  /* Duplicate - Skip */
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
   { KEY_102ND, RETROK_OEM_102 },
   { 0, RETROK_UNKNOWN },
};
#endif

#ifdef ANDROID
const struct rarch_key_map rarch_key_map_android[] = {
   { AKEYCODE_DEL, RETROK_BACKSPACE },
   { AKEYCODE_TAB, RETROK_TAB },
   { AKEYCODE_CLEAR, RETROK_CLEAR },
   { AKEYCODE_ENTER, RETROK_RETURN },
   { AKEYCODE_BREAK, RETROK_PAUSE },
   { AKEYCODE_ESCAPE, RETROK_ESCAPE },
   { AKEYCODE_SPACE, RETROK_SPACE },
   { AKEYCODE_APOSTROPHE, RETROK_QUOTE },
   { AKEYCODE_NUMPAD_LEFT_PAREN, RETROK_LEFTPAREN },
   { AKEYCODE_NUMPAD_RIGHT_PAREN, RETROK_RIGHTPAREN },
   { AKEYCODE_NUMPAD_MULTIPLY, RETROK_ASTERISK },
   { AKEYCODE_NUMPAD_ADD, RETROK_PLUS },
   { AKEYCODE_COMMA, RETROK_COMMA },
   { AKEYCODE_MINUS, RETROK_MINUS },
   { AKEYCODE_PERIOD, RETROK_PERIOD },
   { AKEYCODE_SLASH, RETROK_SLASH },
   { AKEYCODE_0, RETROK_0 },
   { AKEYCODE_1, RETROK_1 },
   { AKEYCODE_2, RETROK_2 },
   { AKEYCODE_3, RETROK_3 },
   { AKEYCODE_4, RETROK_4 },
   { AKEYCODE_5, RETROK_5 },
   { AKEYCODE_6, RETROK_6 },
   { AKEYCODE_7, RETROK_7 },
   { AKEYCODE_8, RETROK_8 },
   { AKEYCODE_9, RETROK_9 },
   { AKEYCODE_SEMICOLON, RETROK_SEMICOLON },
   { AKEYCODE_EQUALS, RETROK_EQUALS },
   { AKEYCODE_LEFT_BRACKET, RETROK_LEFTBRACKET },
   { AKEYCODE_BACKSLASH, RETROK_BACKSLASH },
   { AKEYCODE_RIGHT_BRACKET, RETROK_RIGHTBRACKET },
   { AKEYCODE_GRAVE, RETROK_BACKQUOTE },
   { AKEYCODE_A, RETROK_a },
   { AKEYCODE_B, RETROK_b },
   { AKEYCODE_C, RETROK_c },
   { AKEYCODE_D, RETROK_d },
   { AKEYCODE_E, RETROK_e },
   { AKEYCODE_F, RETROK_f },
   { AKEYCODE_G, RETROK_g },
   { AKEYCODE_H, RETROK_h },
   { AKEYCODE_I, RETROK_i },
   { AKEYCODE_J, RETROK_j },
   { AKEYCODE_K, RETROK_k },
   { AKEYCODE_L, RETROK_l },
   { AKEYCODE_M, RETROK_m },
   { AKEYCODE_N, RETROK_n },
   { AKEYCODE_O, RETROK_o },
   { AKEYCODE_P, RETROK_p },
   { AKEYCODE_Q, RETROK_q },
   { AKEYCODE_R, RETROK_r },
   { AKEYCODE_S, RETROK_s },
   { AKEYCODE_T, RETROK_t },
   { AKEYCODE_U, RETROK_u },
   { AKEYCODE_V, RETROK_v },
   { AKEYCODE_W, RETROK_w },
   { AKEYCODE_X, RETROK_x },
   { AKEYCODE_Y, RETROK_y },
   { AKEYCODE_Z, RETROK_z },
   { AKEYCODE_DEL, RETROK_DELETE },
   { AKEYCODE_NUMPAD_0, RETROK_KP0 },
   { AKEYCODE_NUMPAD_1, RETROK_KP1 },
   { AKEYCODE_NUMPAD_2, RETROK_KP2 },
   { AKEYCODE_NUMPAD_3, RETROK_KP3 },
   { AKEYCODE_NUMPAD_4, RETROK_KP4 },
   { AKEYCODE_NUMPAD_5, RETROK_KP5 },
   { AKEYCODE_NUMPAD_6, RETROK_KP6 },
   { AKEYCODE_NUMPAD_7, RETROK_KP7 },
   { AKEYCODE_NUMPAD_8, RETROK_KP8 },
   { AKEYCODE_NUMPAD_9, RETROK_KP9 },
   { AKEYCODE_NUMPAD_DOT, RETROK_KP_PERIOD },
   { AKEYCODE_NUMPAD_DIVIDE, RETROK_KP_DIVIDE },
   { AKEYCODE_NUMPAD_MULTIPLY, RETROK_KP_MULTIPLY },
   { AKEYCODE_NUMPAD_SUBTRACT, RETROK_KP_MINUS },
   { AKEYCODE_NUMPAD_ADD, RETROK_KP_PLUS },
   { AKEYCODE_NUMPAD_ENTER, RETROK_KP_ENTER },
   { AKEYCODE_NUMPAD_EQUALS, RETROK_KP_EQUALS },
   { AKEYCODE_DPAD_UP, RETROK_UP },
   { AKEYCODE_DPAD_DOWN, RETROK_DOWN },
   { AKEYCODE_DPAD_RIGHT, RETROK_RIGHT },
   { AKEYCODE_DPAD_LEFT, RETROK_LEFT },
   { AKEYCODE_INSERT, RETROK_INSERT },
   { AKEYCODE_MOVE_HOME, RETROK_HOME },
   { AKEYCODE_MOVE_END, RETROK_END },
   { AKEYCODE_PAGE_UP, RETROK_PAGEUP },
   { AKEYCODE_PAGE_DOWN, RETROK_PAGEDOWN },
   { AKEYCODE_F1, RETROK_F1 },
   { AKEYCODE_F2, RETROK_F2 },
   { AKEYCODE_F3, RETROK_F3 },
   { AKEYCODE_F4, RETROK_F4 },
   { AKEYCODE_F5, RETROK_F5 },
   { AKEYCODE_F6, RETROK_F6 },
   { AKEYCODE_F7, RETROK_F7 },
   { AKEYCODE_F8, RETROK_F8 },
   { AKEYCODE_F9, RETROK_F9 },
   { AKEYCODE_F10, RETROK_F10 },
   { AKEYCODE_F11, RETROK_F11 },
   { AKEYCODE_F12, RETROK_F12 },
   { AKEYCODE_NUM_LOCK, RETROK_NUMLOCK },
   { AKEYCODE_CAPS_LOCK, RETROK_CAPSLOCK },
   { AKEYCODE_SCROLL_LOCK, RETROK_SCROLLOCK },
   { AKEYCODE_SHIFT_LEFT, RETROK_RSHIFT },
   { AKEYCODE_SHIFT_RIGHT, RETROK_LSHIFT },
   { AKEYCODE_CTRL_RIGHT, RETROK_RCTRL },
   { AKEYCODE_CTRL_LEFT, RETROK_LCTRL },
   { AKEYCODE_ALT_RIGHT, RETROK_RALT },
   { AKEYCODE_ALT_LEFT, RETROK_LALT },
   { 0, RETROK_UNKNOWN },
};
#endif

#ifdef __QNX__
const struct rarch_key_map rarch_key_map_qnx[] = {
   { KEYCODE_BACKSPACE, RETROK_BACKSPACE },
   { KEYCODE_RETURN, RETROK_RETURN },
   { KEYCODE_SPACE, RETROK_SPACE },
   { KEYCODE_UP, RETROK_UP },
   { KEYCODE_DOWN, RETROK_DOWN },
   { KEYCODE_LEFT, RETROK_LEFT },
   { KEYCODE_RIGHT, RETROK_RIGHT },
   { KEYCODE_A, RETROK_a },
   { KEYCODE_B, RETROK_b },
   { KEYCODE_C, RETROK_c },
   { KEYCODE_D, RETROK_d },
   { KEYCODE_E, RETROK_e },
   { KEYCODE_F, RETROK_f },
   { KEYCODE_G, RETROK_g },
   { KEYCODE_H, RETROK_h },
   { KEYCODE_I, RETROK_i },
   { KEYCODE_J, RETROK_j },
   { KEYCODE_K, RETROK_k },
   { KEYCODE_L, RETROK_l },
   { KEYCODE_M, RETROK_m },
   { KEYCODE_N, RETROK_n },
   { KEYCODE_O, RETROK_o },
   { KEYCODE_P, RETROK_p },
   { KEYCODE_Q, RETROK_q },
   { KEYCODE_R, RETROK_r },
   { KEYCODE_S, RETROK_s },
   { KEYCODE_T, RETROK_t },
   { KEYCODE_U, RETROK_u },
   { KEYCODE_V, RETROK_v },
   { KEYCODE_W, RETROK_w },
   { KEYCODE_X, RETROK_x },
   { KEYCODE_Y, RETROK_y },
   { KEYCODE_Z, RETROK_z },
   { KEYCODE_ZERO, RETROK_0 },
   { KEYCODE_ONE, RETROK_1 },
   { KEYCODE_TWO, RETROK_2 },
   { KEYCODE_THREE, RETROK_3 },
   { KEYCODE_FOUR, RETROK_4 },
   { KEYCODE_FIVE, RETROK_5 },
   { KEYCODE_SIX, RETROK_6 },
   { KEYCODE_SEVEN, RETROK_7 },
   { KEYCODE_EIGHT, RETROK_8 },
   { KEYCODE_NINE, RETROK_9 },
   { KEYCODE_INSERT, RETROK_INSERT },
   { KEYCODE_HOME, RETROK_HOME },
   { KEYCODE_END, RETROK_END },
   { KEYCODE_PG_UP, RETROK_PAGEUP },
   { KEYCODE_PG_DOWN, RETROK_PAGEDOWN },
   { KEYCODE_F1, RETROK_F1 },
   { KEYCODE_F2, RETROK_F2 },
   { KEYCODE_F3, RETROK_F3 },
   { KEYCODE_F4, RETROK_F4 },
   { KEYCODE_F5, RETROK_F5 },
   { KEYCODE_F6, RETROK_F6 },
   { KEYCODE_F7, RETROK_F7 },
   { KEYCODE_F8, RETROK_F8 },
   { KEYCODE_F9, RETROK_F9 },
   { KEYCODE_F10, RETROK_F10 },
   { KEYCODE_F11, RETROK_F11 },
   { KEYCODE_F12, RETROK_F12 },
   { KEYCODE_LEFT_SHIFT, RETROK_LSHIFT },
   { KEYCODE_RIGHT_SHIFT, RETROK_RSHIFT },
   { KEYCODE_LEFT_CTRL, RETROK_LCTRL },
   { KEYCODE_RIGHT_CTRL, RETROK_RCTRL },
   { KEYCODE_LEFT_ALT, RETROK_LALT },
   { KEYCODE_RIGHT_ALT, RETROK_RALT },
   // TODO/FIXME: Code for 'sym' key on BB keyboards. Figure out which sys/keycodes.h define this maps to.
   { 61651, RETROK_RSUPER },
   { KEYCODE_DOLLAR, RETROK_DOLLAR },
   { KEYCODE_MENU, RETROK_MENU },
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
   { KEY_NonUSBackslash, RETROK_OEM_102 },
   { 0, RETROK_UNKNOWN }
};
#endif

#ifdef DJGPP
const struct rarch_key_map rarch_key_map_dos[] = {
   { DOSKEY_ESCAPE, RETROK_ESCAPE },
   { DOSKEY_F1, RETROK_F1 },
   { DOSKEY_F2, RETROK_F2 },
   { DOSKEY_F3, RETROK_F3 },
   { DOSKEY_F4, RETROK_F4 },
   { DOSKEY_F5, RETROK_F5 },
   { DOSKEY_F6, RETROK_F6 },
   { DOSKEY_F7, RETROK_F7 },
   { DOSKEY_F8, RETROK_F8 },
   { DOSKEY_F9, RETROK_F9 },
   { DOSKEY_F10, RETROK_F10 },

   { DOSKEY_BACKQUOTE, RETROK_BACKQUOTE },
   { DOSKEY_1, RETROK_1 },
   { DOSKEY_2, RETROK_2 },
   { DOSKEY_3, RETROK_3 },
   { DOSKEY_4, RETROK_4 },
   { DOSKEY_5, RETROK_5 },
   { DOSKEY_6, RETROK_6 },
   { DOSKEY_7, RETROK_7 },
   { DOSKEY_8, RETROK_8 },
   { DOSKEY_9, RETROK_9 },
   { DOSKEY_0, RETROK_0 },
   { DOSKEY_MINUS, RETROK_MINUS },
   { DOSKEY_EQUAL, RETROK_EQUALS },
   { DOSKEY_BACKSPACE, RETROK_BACKSPACE },

   { DOSKEY_TAB, RETROK_TAB },
   { DOSKEY_q, RETROK_q },
   { DOSKEY_w, RETROK_w },
   { DOSKEY_e, RETROK_e },
   { DOSKEY_r, RETROK_r },
   { DOSKEY_t, RETROK_t },
   { DOSKEY_y, RETROK_y },
   { DOSKEY_u, RETROK_u },
   { DOSKEY_i, RETROK_i },
   { DOSKEY_o, RETROK_o },
   { DOSKEY_p, RETROK_p },
   { DOSKEY_LBRACKET, RETROK_LEFTBRACKET },
   { DOSKEY_RBRACKET, RETROK_RIGHTBRACKET },
   { DOSKEY_BACKSLASH, RETROK_BACKSLASH },

   { DOSKEY_CAPSLOCK, RETROK_CAPSLOCK },
   { DOSKEY_a, RETROK_a },
   { DOSKEY_s, RETROK_s },
   { DOSKEY_d, RETROK_d },
   { DOSKEY_f, RETROK_f },
   { DOSKEY_g, RETROK_g },
   { DOSKEY_h, RETROK_h },
   { DOSKEY_j, RETROK_j },
   { DOSKEY_k, RETROK_k },
   { DOSKEY_l, RETROK_l },
   { DOSKEY_SEMICOLON, RETROK_SEMICOLON },
   { DOSKEY_QUOTE, RETROK_QUOTE },
   { DOSKEY_RETURN, RETROK_RETURN },

   { DOSKEY_LSHIFT, RETROK_LSHIFT },
   { DOSKEY_z, RETROK_z },
   { DOSKEY_x, RETROK_x },
   { DOSKEY_c, RETROK_c },
   { DOSKEY_v, RETROK_v },
   { DOSKEY_b, RETROK_b },
   { DOSKEY_n, RETROK_n },
   { DOSKEY_m, RETROK_m },
   { DOSKEY_COMMA, RETROK_COMMA },
   { DOSKEY_PERIOD, RETROK_PERIOD },
   { DOSKEY_SLASH, RETROK_SLASH },
   { DOSKEY_RSHIFT, RETROK_RSHIFT },

   { DOSKEY_LCTRL, RETROK_LCTRL },
   { DOSKEY_LSUPER, RETROK_LSUPER },
   { DOSKEY_LALT, RETROK_LALT },
   { DOSKEY_SPACE, RETROK_SPACE },
   { DOSKEY_RALT, RETROK_RALT },
   { DOSKEY_RSUPER, RETROK_RSUPER },
   { DOSKEY_MENU, RETROK_MENU },
   { DOSKEY_RCTRL, RETROK_RCTRL },

   { DOSKEY_UP, RETROK_UP },
   { DOSKEY_DOWN, RETROK_DOWN },
   { DOSKEY_LEFT, RETROK_LEFT },
   { DOSKEY_RIGHT, RETROK_RIGHT },

   { DOSKEY_HOME, RETROK_HOME },
   { DOSKEY_END, RETROK_END },
   { DOSKEY_PGUP, RETROK_PAGEUP },
   { DOSKEY_PGDN, RETROK_PAGEDOWN },

   { 0, RETROK_UNKNOWN }
};
#endif

#ifdef __PSL1GHT__
const struct rarch_key_map rarch_key_map_psl1ght[] = {
   { KB_RAWKEY_A, RETROK_a },
   { KB_RAWKEY_B, RETROK_b },
   { KB_RAWKEY_C, RETROK_c },
   { KB_RAWKEY_D, RETROK_d },
   { KB_RAWKEY_E, RETROK_e },
   { KB_RAWKEY_F, RETROK_f },
   { KB_RAWKEY_G, RETROK_g },
   { KB_RAWKEY_H, RETROK_h },
   { KB_RAWKEY_I, RETROK_i },
   { KB_RAWKEY_J, RETROK_j },
   { KB_RAWKEY_K, RETROK_k },
   { KB_RAWKEY_L, RETROK_l },
   { KB_RAWKEY_M, RETROK_m },
   { KB_RAWKEY_N, RETROK_n },
   { KB_RAWKEY_O, RETROK_o },
   { KB_RAWKEY_P, RETROK_p },
   { KB_RAWKEY_Q, RETROK_q },
   { KB_RAWKEY_R, RETROK_r },
   { KB_RAWKEY_S, RETROK_s },
   { KB_RAWKEY_T, RETROK_t },
   { KB_RAWKEY_U, RETROK_u },
   { KB_RAWKEY_V, RETROK_v },
   { KB_RAWKEY_W, RETROK_w },
   { KB_RAWKEY_X, RETROK_x },
   { KB_RAWKEY_Y, RETROK_y },
   { KB_RAWKEY_Z, RETROK_z },
   { KB_RAWKEY_BS, RETROK_BACKSPACE },
   { KB_RAWKEY_TAB, RETROK_TAB },
   { KB_RAWKEY_ENTER, RETROK_RETURN },
   { KB_RAWKEY_PAUSE, RETROK_PAUSE },
   { KB_RAWKEY_ESCAPE, RETROK_ESCAPE },
   { KB_RAWKEY_SPACE, RETROK_SPACE },
   { KB_RAWKEY_QUOTATION_101, RETROK_QUOTE },
   { KB_RAWKEY_COMMA, RETROK_COMMA },
   { KB_RAWKEY_MINUS, RETROK_MINUS },
   { KB_RAWKEY_PERIOD, RETROK_PERIOD },
   { KB_RAWKEY_SLASH, RETROK_SLASH },
   { KB_RAWKEY_0, RETROK_0 },
   { KB_RAWKEY_1, RETROK_1 },
   { KB_RAWKEY_2, RETROK_2 },
   { KB_RAWKEY_3, RETROK_3 },
   { KB_RAWKEY_4, RETROK_4 },
   { KB_RAWKEY_5, RETROK_5 },
   { KB_RAWKEY_6, RETROK_6 },
   { KB_RAWKEY_7, RETROK_7 },
   { KB_RAWKEY_8, RETROK_8 },
   { KB_RAWKEY_9, RETROK_9 },
   { KB_RAWKEY_SEMICOLON, RETROK_SEMICOLON },
   { KB_RAWKEY_EQUAL_101, RETROK_EQUALS },
   { KB_RAWKEY_LEFT_BRACKET_101, RETROK_LEFTBRACKET },
   { KB_RAWKEY_BACKSLASH_101, RETROK_BACKSLASH },
   { KB_RAWKEY_RIGHT_BRACKET_101, RETROK_RIGHTBRACKET },
   { KB_RAWKEY_DELETE, RETROK_DELETE },
   { KB_RAWKEY_KPAD_0, RETROK_KP0 },
   { KB_RAWKEY_KPAD_1, RETROK_KP1 },
   { KB_RAWKEY_KPAD_2, RETROK_KP2 },
   { KB_RAWKEY_KPAD_3, RETROK_KP3 },
   { KB_RAWKEY_KPAD_4, RETROK_KP4 },
   { KB_RAWKEY_KPAD_5, RETROK_KP5 },
   { KB_RAWKEY_KPAD_6, RETROK_KP6 },
   { KB_RAWKEY_KPAD_7, RETROK_KP7 },
   { KB_RAWKEY_KPAD_8, RETROK_KP8 },
   { KB_RAWKEY_KPAD_9, RETROK_KP9 },
   { KB_RAWKEY_KPAD_PERIOD, RETROK_KP_PERIOD },
   { KB_RAWKEY_KPAD_SLASH, RETROK_KP_DIVIDE },
   { KB_RAWKEY_KPAD_ASTERISK, RETROK_KP_MULTIPLY },
   { KB_RAWKEY_KPAD_MINUS, RETROK_KP_MINUS },
   { KB_RAWKEY_KPAD_PLUS, RETROK_KP_PLUS },
   { KB_RAWKEY_KPAD_ENTER, RETROK_KP_ENTER },
   { KB_RAWKEY_UP_ARROW, RETROK_UP },
   { KB_RAWKEY_DOWN_ARROW, RETROK_DOWN },
   { KB_RAWKEY_RIGHT_ARROW, RETROK_RIGHT },
   { KB_RAWKEY_LEFT_ARROW, RETROK_LEFT },
   { KB_RAWKEY_INSERT, RETROK_INSERT },
   { KB_RAWKEY_HOME, RETROK_HOME },
   { KB_RAWKEY_END, RETROK_END },
   { KB_RAWKEY_PAGE_UP, RETROK_PAGEUP },
   { KB_RAWKEY_PAGE_DOWN, RETROK_PAGEDOWN },
   { KB_RAWKEY_F1, RETROK_F1 },
   { KB_RAWKEY_F2, RETROK_F2 },
   { KB_RAWKEY_F3, RETROK_F3 },
   { KB_RAWKEY_F4, RETROK_F4 },
   { KB_RAWKEY_F5, RETROK_F5 },
   { KB_RAWKEY_F6, RETROK_F6 },
   { KB_RAWKEY_F7, RETROK_F7 },
   { KB_RAWKEY_F8, RETROK_F8 },
   { KB_RAWKEY_F9, RETROK_F9 },
   { KB_RAWKEY_F10, RETROK_F10 },
   { KB_RAWKEY_F11, RETROK_F11 },
   { KB_RAWKEY_F12, RETROK_F12 },
   { KB_RAWKEY_KPAD_NUMLOCK, RETROK_NUMLOCK },
   { KB_RAWKEY_CAPS_LOCK, RETROK_CAPSLOCK },
   { KB_RAWKEY_SCROLL_LOCK, RETROK_SCROLLOCK },
   { KB_RAWKEY_PAUSE, RETROK_BREAK },

   /* 
   { KB_RAWKEY_HASHTILDE, RETROK_HASH },
   { KB_RAWKEY_KPLEFTPAREN, RETROK_LEFTPAREN },
   { KB_RAWKEY_KPRIGHTPAREN, RETROK_RIGHTPAREN },
   { KB_RAWKEY_LEFTMETA, RETROK_LMETA },
   { KB_RAWKEY_RIGHTMETA, RETROK_RMETA },
   { KB_RAWKEY_COMPOSE, RETROK_COMPOSE },
   { KB_RAWKEY_HELP, RETROK_HELP },
   { KB_RAWKEY_POWER, RETROK_POWER },
   { KB_RAWKEY_UNDO, RETROK_UNDO },
   { KB_RAWKEY_KPAD_EQUAL, RETROK_KP_EQUALS },

 KB_RAWKEY_PRINTSCREEN
 KB_RAWKEY_APPLICATION

 KB_RAWKEY_106_KANJI
 KB_RAWKEY_KANA
 KB_RAWKEY_HENKAN
 KB_RAWKEY_MUHENKAN
 KB_RAWKEY_ACCENT_CIRCONFLEX_106
 KB_RAWKEY_ATMARK_106
 KB_RAWKEY_LEFT_BRACKET_106
 KB_RAWKEY_RIGHT_BRACKET_106
 KB_RAWKEY_COLON_106
 KB_RAWKEY_BACKSLASH_106
 KB_RAWKEY_YEN_106 */

   { 0, RETROK_UNKNOWN }
};
#endif

#if defined(_WIN32) && _WIN32_WINNT >= 0x0501 && !defined(__WINRT__)
const struct rarch_key_map rarch_key_map_winraw[] = {
   { SC_BACKSPACE, RETROK_BACKSPACE },
   { SC_TAB, RETROK_TAB },
   { SC_CLEAR, RETROK_CLEAR },
   { SC_RETURN, RETROK_RETURN },
   { SC_PAUSE, RETROK_PAUSE },
   { SC_ESCAPE, RETROK_ESCAPE },
   { SC_SPACE, RETROK_SPACE },
   { SC_PAGEUP, RETROK_PAGEUP },
   { SC_PAGEDOWN, RETROK_PAGEDOWN },
   { SC_END, RETROK_END },
   { SC_HOME, RETROK_HOME },
   { SC_LEFT, RETROK_LEFT },
   { SC_UP, RETROK_UP },
   { SC_RIGHT, RETROK_RIGHT },
   { SC_DOWN, RETROK_DOWN },
   { SC_PRINT, RETROK_PRINT },
   { SC_INSERT, RETROK_INSERT },
   { SC_DELETE, RETROK_DELETE },
   { SC_HELP, RETROK_HELP },
   { SC_0, RETROK_0 },
   { SC_1, RETROK_1 },
   { SC_2, RETROK_2 },
   { SC_3, RETROK_3 },
   { SC_4, RETROK_4 },
   { SC_5, RETROK_5 },
   { SC_6, RETROK_6 },
   { SC_7, RETROK_7 },
   { SC_8, RETROK_8 },
   { SC_9, RETROK_9 },
   { SC_a, RETROK_a },
   { SC_b, RETROK_b },
   { SC_c, RETROK_c },
   { SC_d, RETROK_d },
   { SC_e, RETROK_e },
   { SC_f, RETROK_f },
   { SC_g, RETROK_g },
   { SC_h, RETROK_h },
   { SC_i, RETROK_i },
   { SC_j, RETROK_j },
   { SC_k, RETROK_k },
   { SC_l, RETROK_l },
   { SC_m, RETROK_m },
   { SC_n, RETROK_n },
   { SC_o, RETROK_o },
   { SC_p, RETROK_p },
   { SC_q, RETROK_q },
   { SC_r, RETROK_r },
   { SC_s, RETROK_s },
   { SC_t, RETROK_t },
   { SC_u, RETROK_u },
   { SC_v, RETROK_v },
   { SC_w, RETROK_w },
   { SC_x, RETROK_x },
   { SC_y, RETROK_y },
   { SC_z, RETROK_z },
   { SC_LSUPER, RETROK_LSUPER },
   { SC_RSUPER, RETROK_RSUPER },
   { SC_MENU, RETROK_MENU },
   { SC_KP0, RETROK_KP0 },
   { SC_KP1, RETROK_KP1 },
   { SC_KP2, RETROK_KP2 },
   { SC_KP3, RETROK_KP3 },
   { SC_KP4, RETROK_KP4 },
   { SC_KP5, RETROK_KP5 },
   { SC_KP6, RETROK_KP6 },
   { SC_KP7, RETROK_KP7 },
   { SC_KP8, RETROK_KP8 },
   { SC_KP9, RETROK_KP9 },
   { SC_KP_MULTIPLY, RETROK_KP_MULTIPLY },
   { SC_KP_PLUS, RETROK_KP_PLUS },
   { SC_KP_MINUS, RETROK_KP_MINUS },
   { SC_KP_PERIOD, RETROK_KP_PERIOD },
   { SC_KP_DIVIDE, RETROK_KP_DIVIDE },
   { SC_F1, RETROK_F1 },
   { SC_F2, RETROK_F2 },
   { SC_F3, RETROK_F3 },
   { SC_F4, RETROK_F4 },
   { SC_F5, RETROK_F5 },
   { SC_F6, RETROK_F6 },
   { SC_F7, RETROK_F7 },
   { SC_F8, RETROK_F8 },
   { SC_F9, RETROK_F9 },
   { SC_F10, RETROK_F10 },
   { SC_F11, RETROK_F11 },
   { SC_F12, RETROK_F12 },
   { SC_F13, RETROK_F13 },
   { SC_F14, RETROK_F14 },
   { SC_F15, RETROK_F15 },
   { SC_NUMLOCK, RETROK_NUMLOCK },
   { SC_SCROLLLOCK, RETROK_SCROLLOCK },
   { SC_LSHIFT, RETROK_LSHIFT },
   { SC_RSHIFT, RETROK_RSHIFT },
   { SC_LCTRL, RETROK_LCTRL },
   { SC_RCTRL, RETROK_RCTRL },
   { SC_LALT, RETROK_LALT },
   { SC_RALT, RETROK_RALT },
   { SC_KP_ENTER, RETROK_KP_ENTER },
   { SC_CAPSLOCK, RETROK_CAPSLOCK },
   { SC_COMMA, RETROK_COMMA },
   { SC_PERIOD, RETROK_PERIOD },
   { SC_MINUS, RETROK_MINUS },
   { SC_EQUALS, RETROK_EQUALS },
   { SC_LEFTBRACKET, RETROK_LEFTBRACKET },
   { SC_RIGHTBRACKET, RETROK_RIGHTBRACKET },
   { SC_SEMICOLON, RETROK_SEMICOLON },
   { SC_BACKQUOTE, RETROK_BACKQUOTE },
   { SC_BACKSLASH, RETROK_BACKSLASH },
   { SC_SLASH, RETROK_SLASH },
   { SC_APOSTROPHE, RETROK_QUOTE },
   { SC_ANGLEBRACKET, RETROK_OEM_102 },
   { 0, RETROK_UNKNOWN }
};
#endif

#ifdef __WINRT__
/* Refer to uwp_main.cpp - on WinRT these constants are defined as C++ enum classes
 * so they can't be placed in a C source file */
#endif

/* TODO/FIXME - global */
enum retro_key rarch_keysym_lut[RETROK_LAST];

/* TODO/FIXME - static globals */
static unsigned *rarch_keysym_rlut           = NULL;
static unsigned rarch_keysym_rlut_size       = 0;

/**
 * input_keymaps_init_keyboard_lut:
 * @map                   : Keyboard map.
 *
 * Initializes and sets the keyboard layout to a keyboard map (@map).
 **/
void input_keymaps_init_keyboard_lut(const struct rarch_key_map *map)
{
   const struct rarch_key_map *map_start = map;
   memset(rarch_keysym_lut, 0, sizeof(rarch_keysym_lut));
   rarch_keysym_rlut_size = 0;

   for (; map->rk != RETROK_UNKNOWN; map++)
   {
      rarch_keysym_lut[map->rk] = (enum retro_key)map->sym;
      if (map->sym > rarch_keysym_rlut_size)
         rarch_keysym_rlut_size = map->sym;
   }

   if (rarch_keysym_rlut_size < 65536)
   {
      if (rarch_keysym_rlut)
         free(rarch_keysym_rlut);

      rarch_keysym_rlut = (unsigned*)calloc(++rarch_keysym_rlut_size, sizeof(unsigned));

      for (map = map_start; map->rk != RETROK_UNKNOWN; map++)
         rarch_keysym_rlut[map->sym] = (enum retro_key)map->rk;
   }
   else
      rarch_keysym_rlut_size = 0;
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

   /* Fast path */
   if (rarch_keysym_rlut && sym < rarch_keysym_rlut_size)
      return (enum retro_key)rarch_keysym_rlut[sym];

   /* Slow path */
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

   retro_assert(size >= 2);
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
