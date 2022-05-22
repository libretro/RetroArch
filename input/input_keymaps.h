/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef INPUT_KEYMAPS_H__
#define INPUT_KEYMAPS_H__

#include <stdint.h>

#include <retro_common_api.h>
#include <libretro.h>

RETRO_BEGIN_DECLS

struct rarch_key_map
{
   unsigned sym;
   enum retro_key rk;
};

struct input_key_map
{
   const char *str;
   enum retro_key key;
};

#define RARCH_KEY_MAP_RWEBINPUT_SIZE 111

extern const struct input_key_map input_config_key_map[];

extern const struct rarch_key_map rarch_key_map_x11[];
extern const struct rarch_key_map rarch_key_map_sdl[];
extern const struct rarch_key_map rarch_key_map_sdl2[];
extern const struct rarch_key_map rarch_key_map_dinput[];

 /* is generated at runtime so can't be const */
extern struct rarch_key_map rarch_key_map_rwebinput[RARCH_KEY_MAP_RWEBINPUT_SIZE];

extern const struct rarch_key_map rarch_key_map_linux[];
extern const struct rarch_key_map rarch_key_map_apple_hid[];
extern const struct rarch_key_map rarch_key_map_android[];
extern const struct rarch_key_map rarch_key_map_qnx[];
extern const struct rarch_key_map rarch_key_map_dos[];
extern const struct rarch_key_map rarch_key_map_wiiu[];
extern const struct rarch_key_map rarch_key_map_winraw[];
#ifdef HAVE_LIBNX
extern const struct rarch_key_map rarch_key_map_switch[];
#endif
#ifdef VITA
extern const struct rarch_key_map rarch_key_map_vita[];
#endif
#ifdef ORBIS
extern const struct rarch_key_map rarch_key_map_ps4[];
#endif

#if defined(_WIN32) && _WIN32_WINNT >= 0x0501 && !defined(__WINRT__)
enum winraw_scancodes {
   SC_ESCAPE = 0x01,
   SC_1 = 0x02,
   SC_2 = 0x03,
   SC_3 = 0x04,
   SC_4 = 0x05,
   SC_5 = 0x06,
   SC_6 = 0x07,
   SC_7 = 0x08,
   SC_8 = 0x09,
   SC_9 = 0x0A,
   SC_0 = 0x0B,
   SC_MINUS = 0x0C,
   SC_EQUALS = 0x0D,
   SC_BACKSPACE = 0x0E,
   SC_TAB = 0x0F,
   SC_q = 0x10,
   SC_w = 0x11,
   SC_e = 0x12,
   SC_r = 0x13,
   SC_t = 0x14,
   SC_y = 0x15,
   SC_u = 0x16,
   SC_i = 0x17,
   SC_o = 0x18,
   SC_p = 0x19,
   SC_LEFTBRACKET = 0x1A,
   SC_RIGHTBRACKET = 0x1B,
   SC_RETURN = 0x1C,
   SC_LCTRL = 0x1D,
   SC_a = 0x1E,
   SC_s = 0x1F,
   SC_d = 0x20,
   SC_f = 0x21,
   SC_g = 0x22,
   SC_h = 0x23,
   SC_j = 0x24,
   SC_k = 0x25,
   SC_l = 0x26,
   SC_SEMICOLON = 0x27,
   SC_APOSTROPHE = 0x28,
   SC_BACKQUOTE = 0x29,
   SC_LSHIFT = 0x2A,
   SC_BACKSLASH = 0x2B,
   SC_z = 0x2C,
   SC_x = 0x2D,
   SC_c = 0x2E,
   SC_v = 0x2F,
   SC_b = 0x30,
   SC_n = 0x31,
   SC_m = 0x32,
   SC_COMMA = 0x33,
   SC_PERIOD = 0x34,
   SC_SLASH = 0x35,
   SC_RSHIFT = 0x36,
   SC_KP_MULTIPLY = 0x37,
   SC_LALT = 0x38,
   SC_SPACE = 0x39,
   SC_CAPSLOCK = 0x3A,
   SC_F1 = 0x3B,
   SC_F2 = 0x3C,
   SC_F3 = 0x3D,
   SC_F4 = 0x3E,
   SC_F5 = 0x3F,
   SC_F6 = 0x40,
   SC_F7 = 0x41,
   SC_F8 = 0x42,
   SC_F9 = 0x43,
   SC_F10 = 0x44,
   SC_NUMLOCK = 0x45,
   SC_SCROLLLOCK = 0x46,
   SC_KP7 = 0x47,
   SC_KP8 = 0x48,
   SC_KP9 = 0x49,
   SC_KP_MINUS = 0x4A,
   SC_KP4 = 0x4B,
   SC_KP5 = 0x4C,
   SC_KP6 = 0x4D,
   SC_KP_PLUS = 0x4E,
   SC_KP1 = 0x4F,
   SC_KP2 = 0x50,
   SC_KP3 = 0x51,
   SC_KP0 = 0x52,
   SC_KP_PERIOD = 0x53,
   SC_ALT_PRINT = 0x54,
   SC_ANGLEBRACKET = 0x56,
   SC_F11 = 0x57,
   SC_F12 = 0x58,
   SC_OEM_1 = 0x5a,
   SC_OEM_2 = 0x5b,
   SC_OEM_3 = 0x5c,
   SC_ERASE_EOF = 0x5d,
   SC_CLEAR = 0x5d,
   SC_OEM_4 = 0x5e,
   SC_OEM_5 = 0x5f,
   SC_HELP = 0x63,
   SC_F13 = 0x64,
   SC_F14 = 0x65,
   SC_F15 = 0x66,
   SC_F16 = 0x67,
   SC_F17 = 0x68,
   SC_F18 = 0x69,
   SC_F19 = 0x6a,
   SC_F20 = 0x6b,
   SC_F21 = 0x6c,
   SC_F22 = 0x6d,
   SC_F23 = 0x6e,
   SC_OEM_6 = 0x6f,
   SC_KATAKANA = 0x70,
   SC_OEM_7 = 0x71,
   SC_F24 = 0x76,
   SC_SBCSCHAR = 0x77,
   SC_CONVERT = 0x79,
   SC_NONCONVERT = 0x7B,

   SC_MEDIA_PREV = 0xE010,
   SC_MEDIA_NEXT = 0xE019,
   SC_KP_ENTER = 0xE01C,
   SC_RCTRL = 0xE01D,
   SC_VOLUME_MUTE = 0xE020,
   SC_LAUNCH_APP2 = 0xE021,
   SC_MEDIA_PLAY = 0xE022,
   SC_MEDIA_STOP = 0xE024,
   SC_VOLUME_DOWN = 0xE02E,
   SC_VOLUME_UP = 0xE030,
   SC_BROWSER_HOME = 0xE032,
   SC_KP_DIVIDE = 0xE035,
   SC_PRINT = 0xE037,
   SC_RALT = 0xE038,
   SC_BREAK = 0xE046,
   SC_HOME = 0xE047,
   SC_UP = 0xE048,
   SC_PAGEUP = 0xE049,
   SC_LEFT = 0xE04B,
   SC_RIGHT = 0xE04D,
   SC_END = 0xE04F,
   SC_DOWN = 0xE050,
   SC_PAGEDOWN = 0xE051,
   SC_INSERT = 0xE052,
   SC_DELETE = 0xE053,
   SC_LSUPER = 0xE05B,
   SC_RSUPER = 0xE05C,
   SC_MENU = 0xE05D,
   SC_POWER = 0xE05E,
   SC_SLEEP = 0xE05F,
   SC_WAKE = 0xE063,
   SC_BROWSER_SEARCH = 0xE065,
   SC_BROWSER_FAVORITES = 0xE066,
   SC_BROWSER_REFRESH = 0xE067,
   SC_BROWSER_STOP = 0xE068,
   SC_BROWSER_FORWARD = 0xE069,
   SC_BROWSER_BACK = 0xE06A,
   SC_LAUNCH_APP1 = 0xE06B,
   SC_LAUNCH_EMAIL = 0xE06C,
   SC_LAUNCH_MEDIA = 0xE06D,

   SC_PAUSE = 0xFFFE/*0xE11D45*/,
   SC_LAST = 0xFFFF
};
#endif

/**
 * input_keymaps_init_keyboard_lut:
 * @map                   : Keyboard map.
 *
 * Initializes and sets the keyboard layout to a keyboard map (@map).
 **/
void input_keymaps_init_keyboard_lut(const struct rarch_key_map *map);

/**
 * input_keymaps_translate_keysym_to_rk:
 * @sym                   : Key symbol.
 *
 * Translates a key symbol from the keyboard layout table
 * to an associated retro key identifier.
 *
 * Returns: Retro key identifier.
 **/
enum retro_key input_keymaps_translate_keysym_to_rk(unsigned sym);

/**
 * input_keymaps_translate_rk_to_str:
 * @key                   : Retro key identifier.
 * @buf                   : Buffer.
 * @size                  : Size of @buf.
 *
 * Translates a retro key identifier to a human-readable
 * identifier string.
 **/
void input_keymaps_translate_rk_to_str(enum retro_key key, char *buf, size_t size);

extern enum retro_key rarch_keysym_lut[RETROK_LAST];

RETRO_END_DECLS

#endif
