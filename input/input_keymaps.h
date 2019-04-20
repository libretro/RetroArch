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

#ifdef __APPLE__
struct apple_key_name_map_entry
{
   const char* const keyname;
   const uint32_t hid_id;
};

extern const struct apple_key_name_map_entry apple_key_name_map[];
#endif

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
