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

#define RARCH_KEY_MAP_RWEBINPUT_SIZE 111

RETRO_BEGIN_DECLS

#define SC_ESCAPE 0x01
#define SC_1 0x02
#define SC_2 0x03
#define SC_3 0x04
#define SC_4 0x05
#define SC_5 0x06
#define SC_6 0x07
#define SC_7 0x08
#define SC_8 0x09
#define SC_9 0x0A
#define SC_0 0x0B
#define SC_MINUS 0x0C
#define SC_EQUALS 0x0D
#define SC_BACKSPACE 0x0E
#define SC_TAB 0x0F
#define SC_q 0x10
#define SC_w 0x11
#define SC_e 0x12
#define SC_r 0x13
#define SC_t 0x14
#define SC_y 0x15
#define SC_u 0x16
#define SC_i 0x17
#define SC_o 0x18
#define SC_p 0x19
#define SC_LEFTBRACKET 0x1A
#define SC_RIGHTBRACKET 0x1B
#define SC_RETURN 0x1C
#define SC_LCTRL 0x1D
#define SC_a 0x1E
#define SC_s 0x1F
#define SC_d 0x20
#define SC_f 0x21
#define SC_g 0x22
#define SC_h 0x23
#define SC_j 0x24
#define SC_k 0x25
#define SC_l 0x26
#define SC_SEMICOLON 0x27
#define SC_APOSTROPHE 0x28
#define SC_BACKQUOTE 0x29
#define SC_LSHIFT 0x2A
#define SC_BACKSLASH 0x2B
#define SC_z 0x2C
#define SC_x 0x2D
#define SC_c 0x2E
#define SC_v 0x2F
#define SC_b 0x30
#define SC_n 0x31
#define SC_m 0x32
#define SC_COMMA 0x33
#define SC_PERIOD 0x34
#define SC_SLASH 0x35
#define SC_RSHIFT 0x36
#define SC_KP_MULTIPLY 0x37
#define SC_LALT 0x38
#define SC_SPACE 0x39
#define SC_CAPSLOCK 0x3A
#define SC_F1 0x3B
#define SC_F2 0x3C
#define SC_F3 0x3D
#define SC_F4 0x3E
#define SC_F5 0x3F
#define SC_F6 0x40
#define SC_F7 0x41
#define SC_F8 0x42
#define SC_F9 0x43
#define SC_F10 0x44
#define SC_NUMLOCK 0x45
#define SC_SCROLLLOCK 0x46
#define SC_KP7 0x47
#define SC_KP8 0x48
#define SC_KP9 0x49
#define SC_KP_MINUS 0x4A
#define SC_KP4 0x4B
#define SC_KP5 0x4C
#define SC_KP6 0x4D
#define SC_KP_PLUS 0x4E
#define SC_KP1 0x4F
#define SC_KP2 0x50
#define SC_KP3 0x51
#define SC_KP0 0x52
#define SC_KP_PERIOD 0x53
#define SC_ALT_PRINT 0x54
#define SC_ANGLEBRACKET 0x56
#define SC_F11 0x57
#define SC_F12 0x58
#define SC_OEM_1 0x5a
#define SC_OEM_2 0x5b
#define SC_OEM_3 0x5c
#define SC_ERASE_EOF 0x5d
#define SC_CLEAR 0x5d
#define SC_OEM_4 0x5e
#define SC_OEM_5 0x5f
#define SC_HELP 0x63
#define SC_F13 0x64
#define SC_F14 0x65
#define SC_F15 0x66
#define SC_F16 0x67
#define SC_F17 0x68
#define SC_F18 0x69
#define SC_F19 0x6a
#define SC_F20 0x6b
#define SC_F21 0x6c
#define SC_F22 0x6d
#define SC_F23 0x6e
#define SC_OEM_6 0x6f
#define SC_KATAKANA 0x70
#define SC_OEM_7 0x71
#define SC_F24 0x76
#define SC_SBCSCHAR 0x77
#define SC_CONVERT 0x79
#define SC_NONCONVERT 0x7B
#define SC_MEDIA_PREV 0xE010
#define SC_MEDIA_NEXT 0xE019
#define SC_KP_ENTER 0xE01C
#define SC_RCTRL 0xE01D
#define SC_VOLUME_MUTE 0xE020
#define SC_LAUNCH_APP2 0xE021
#define SC_MEDIA_PLAY 0xE022
#define SC_MEDIA_STOP 0xE024
#define SC_VOLUME_DOWN 0xE02E
#define SC_VOLUME_UP 0xE030
#define SC_BROWSER_HOME 0xE032
#define SC_KP_DIVIDE 0xE035
#define SC_PRINT 0xE037
#define SC_RALT 0xE038
#define SC_BREAK 0xE046
#define SC_HOME 0xE047
#define SC_UP 0xE048
#define SC_PAGEUP 0xE049
#define SC_LEFT 0xE04B
#define SC_RIGHT 0xE04D
#define SC_END 0xE04F
#define SC_DOWN 0xE050
#define SC_PAGEDOWN 0xE051
#define SC_INSERT 0xE052
#define SC_DELETE 0xE053
#define SC_LSUPER 0xE05B
#define SC_RSUPER 0xE05C
#define SC_MENU 0xE05D
#define SC_POWER 0xE05E
#define SC_SLEEP 0xE05F
#define SC_WAKE 0xE063
#define SC_BROWSER_SEARCH 0xE065
#define SC_BROWSER_FAVORITES 0xE066
#define SC_BROWSER_REFRESH 0xE067
#define SC_BROWSER_STOP 0xE068
#define SC_BROWSER_FORWARD 0xE069
#define SC_BROWSER_BACK 0xE06A
#define SC_LAUNCH_APP1 0xE06B
#define SC_LAUNCH_EMAIL 0xE06C
#define SC_LAUNCH_MEDIA 0xE06D
#define SC_PAUSE 0xFFFE /*0xE11D45*/
#define SC_LAST 0xFFFF

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
 * @s                     : Buffer.
 * @len                   : Size of @s.
 *
 * Translates a retro key identifier to a human-readable
 * identifier string.
 **/
void input_keymaps_translate_rk_to_str(enum retro_key key, char *s, size_t len);

/**
 * input_translate_rk_to_ascii:
 * @key : Retro key identifier
 * @mod : retro_mod mask
 *
 * Translates a retro key identifier with mod mask to ASCII.
 */
uint8_t input_keymaps_translate_rk_to_ascii(enum retro_key key, enum retro_mod mod);

extern enum retro_key rarch_keysym_lut[RETROK_LAST];

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

RETRO_END_DECLS

#endif
