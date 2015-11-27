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

#ifndef INPUT_COMMON_H__
#define INPUT_COMMON_H__

#include <file/config_file.h>
#include <stdint.h>
#include "../driver.h"

const void *input_bind_map_get(unsigned i);

const char *input_bind_map_get_base(unsigned i);

unsigned input_bind_map_get_meta(unsigned i);

const char *input_bind_map_get_desc(unsigned i);

bool input_bind_map_get_valid(unsigned i);

/**
 * input_translate_coord_viewport:
 * @mouse_x                        : Pointer X coordinate.
 * @mouse_y                        : Pointer Y coordinate.
 * @res_x                          : Scaled  X coordinate.
 * @res_y                          : Scaled  Y coordinate.
 * @res_screen_x                   : Scaled screen X coordinate.
 * @res_screen_y                   : Scaled screen Y coordinate.
 *
 * Translates pointer [X,Y] coordinates into scaled screen
 * coordinates based on viewport info.
 *
 * Returns: true (1) if successful, false if video driver doesn't support
 * viewport info.
 **/
bool input_translate_coord_viewport(int mouse_x, int mouse_y,
      int16_t *res_x, int16_t *res_y, int16_t *res_screen_x,
      int16_t *res_screen_y);

/* auto_bind can be NULL. */
void input_get_bind_string(char *buf, const struct retro_keybind *bind,
      const struct retro_keybind *auto_bind, size_t size);

/**
 * input_translate_str_to_rk:
 * @str                            : String to translate to key ID.
 *
 * Translates tring representation to key identifier.
 *
 * Returns: key identifier.
 **/
enum retro_key input_translate_str_to_rk(const char *str);

const char *input_config_get_prefix(unsigned user, bool meta);

/**
 * input_translate_str_to_bind_id:
 * @str                            : String to translate to bind ID.
 *
 * Translate string representation to bind ID.
 *
 * Returns: Bind ID value on success, otherwise RARCH_BIND_LIST_END on not found.
 **/
unsigned input_translate_str_to_bind_id(const char *str);

void input_config_parse_key(config_file_t *conf,
      const char *prefix, const char *btn,
      struct retro_keybind *bind);

void input_config_parse_joy_button(config_file_t *conf, const char *prefix,
      const char *btn, struct retro_keybind *bind);

void input_config_parse_joy_axis(config_file_t *conf, const char *prefix,
      const char *axis, struct retro_keybind *bind);

/**
 * input_push_analog_dpad:
 * @binds                          : Binds to modify.
 * @mode                           : Which analog stick to bind D-Pad to.
 *                                   E.g:
 *                                   ANALOG_DPAD_LSTICK
 *                                   ANALOG_DPAD_RSTICK
 *
 * Push analog to D-Pad mappings to binds.
 **/
void input_push_analog_dpad(struct retro_keybind *binds, unsigned mode);

/**
 * input_pop_analog_dpad:
 * @binds                          : Binds to modify.
 *
 * Restores binds temporarily overridden by input_push_analog_dpad().
 **/
void input_pop_analog_dpad(struct retro_keybind *binds);

retro_input_t input_keys_pressed(void);

/**
 * input_poll:
 *
 * Input polling callback function.
 **/
void input_poll(void);

/**
 * input_state:
 * @port                 : user number.
 * @device               : device identifier of user.
 * @idx                  : index value of user.
 * @id                   : identifier of key pressed by user.
 *
 * Input state callback function.
 *
 * Returns: Non-zero if the given key (identified by @id) was pressed by the user
 * (assigned to @port).
 **/
int16_t input_state(unsigned port, unsigned device,
      unsigned idx, unsigned id);

#endif
